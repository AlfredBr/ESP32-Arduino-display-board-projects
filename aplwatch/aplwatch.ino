// Watch faces for the Waveshare ESP32-S3-Touch-AMOLED-1.8: a light fan cast
// between dark hands, and big stacked digits. Taps cycle Eastern analog,
// Eastern digital (pink), UTC analog (tricolore) and UTC digital (white).
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_sntp.h>
#include <atomic>
#include <sys/time.h>
#include <time.h>
#include "SensorPCF85063.hpp"
#include "BoardDisplay.h"
#include "AplConfig.h"
#include "AplFaces.h"
#include "WiFiCredentials.h"
using namespace AplFaces;

BoardDisplay lcd;
SensorPCF85063 rtc;
Preferences prefs;
static constexpr uint8_t touchAddress = 0x15; // CST820, CST816-compatible registers
static bool touchPresent = false, rtcReady = false, wifiConfigured = false, wasConnected = false;
static uint32_t lastWifiAttempt = 0;
static std::atomic<bool> ntpEvent{false};

// The tap cycle, in order. Even modes are analog; the upper half is UTC.
enum Mode : uint8_t { EASTERN_ANALOG, EASTERN_DIGITAL, UTC_ANALOG, UTC_DIGITAL, MODE_COUNT };
static const char *const MODE_NAMES[MODE_COUNT] = {"Eastern analog", "Eastern digital", "UTC analog", "UTC digital"};
static Mode mode = EASTERN_ANALOG;
static bool isAnalog(Mode m) { return m % 2 == 0; }
static bool isUtc(Mode m) { return m >= UTC_ANALOG; }
// Zone of the analog face shown now, or of the next one after a digital face,
// so the light fan is already rendered when the tap comes.
static bool lightUtcFor(Mode m) { return isUtc(isAnalog(m) ? m : Mode((m + 1) % MODE_COUNT)); }
static const LightPalette &lightPalette(bool utc) { return utc ? FRENCH_LIGHTS : EASTERN_LIGHTS; }

// Full-screen RGB565 buffers in PSRAM: the digital face, the light fan being
// shown and the one being rendered on core 0, and a scratch frame for pushes.
static uint16_t *digitalBg, *lightFront, *lightBack, *scratch;
static std::atomic<bool> lightDone{false}, wantLight{true}, lightUtc{false};
// Zones the light buffers were rendered for; core 0 sets backUtc before lightDone.
static bool frontUtc = false, backUtc = false;
static bool needFull = true;
static Rect lastHand{0, 0, 0, 0};

static bool timeReady(time_t now) { return now >= 1704067200; } // After 2024-01-01.

static void zoneNow(bool utc, tm &clock, float &fraction, bool &ready) {
  timeval tv{};
  gettimeofday(&tv, nullptr);
  ready = timeReady(tv.tv_sec);
  if (utc) gmtime_r(&tv.tv_sec, &clock);
  else localtime_r(&tv.tv_sec, &clock);
  fraction = tv.tv_usec / 1e6f;
}
static float hourAngle(const tm &c, float frac) {
  return ((c.tm_hour % 12) + (c.tm_min + (c.tm_sec + frac) / 60) / 60) * PI_F / 6;
}
static float minuteAngle(const tm &c, float frac) {
  return (c.tm_min + (c.tm_sec + frac) / 60) * PI_F / 30;
}

// Core 0: renders the light fan whenever the hands have moved on, about
// 0.3 s per frame, so the seconds hand on core 1 keeps sweeping meanwhile.
static void lightTask(void *) {
  long renderedKey = -1;
  for (;;) {
    bool utc = lightUtc.load();
    tm clock{}; float frac; bool ready;
    zoneNow(utc, clock, frac, ready);
    long key = (ready ? static_cast<long>(time(nullptr) / LIGHT_REFRESH_S) * 2 : -4) + utc;
    if ((key != renderedKey || wantLight.load()) && !lightDone.load()) {
      wantLight.store(false);
      uint32_t start = millis();
      renderLights({lightBack, 0, 0, W, H}, hourAngle(clock, frac), minuteAngle(clock, frac), ready, lightPalette(utc));
      Serial.printf("aplwatch: %s light face rendered in %lu ms\n", utc ? "UTC" : "Eastern", millis() - start);
      backUtc = utc;
      renderedKey = key;
      lightDone.store(true);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static void restoreTimeFromRtc() {
  // Trust the RTC only if this app or WaveshareHelloWorld wrote UTC to it.
  Preferences other;
  bool helloUtc = other.begin("s3-feature", true) && other.getBool("rtcUTC", false);
  other.end();
  if (!rtcReady || !(prefs.getBool("rtcUTC", false) || helloUtc) || !rtc.isClockIntegrityGuaranteed()) return;
  auto d = rtc.getDateTime();
  tm utc{};
  utc.tm_year = d.getYear() - 1900; utc.tm_mon = d.getMonth() - 1; utc.tm_mday = d.getDay();
  utc.tm_hour = d.getHour(); utc.tm_min = d.getMinute(); utc.tm_sec = d.getSecond();
  setenv("TZ", "UTC0", 1); tzset();
  time_t epoch = mktime(&utc);
  setenv("TZ", TIME_ZONE, 1); tzset();
  if (timeReady(epoch) && d.getMonth() >= 1 && d.getMonth() <= 12) {
    timeval tv{epoch, 0};
    settimeofday(&tv, nullptr);
    Serial.println("aplwatch: time restored from the RTC");
  }
}

static void updateNetwork() {
  if (wifiConfigured) {
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !wasConnected) {
      configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);
      Serial.println("aplwatch: Wi-Fi connected, synchronizing time");
    }
    if (!connected && millis() - lastWifiAttempt >= WIFI_RETRY_MS) {
      lastWifiAttempt = millis();
      WiFi.reconnect();
    }
    wasConnected = connected;
  }
  if (ntpEvent.exchange(false) && rtcReady) {
    time_t now = time(nullptr);
    tm utc{};
    gmtime_r(&now, &utc);
    rtc.setDateTime(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
    if (!prefs.getBool("rtcUTC", false)) prefs.putBool("rtcUTC", true);
    Serial.println("aplwatch: NTP synchronized; RTC set to UTC");
  }
}

// Reports a tap the moment a finger lands: waiting for the lift added
// 60-130 ms, and no other gesture needs telling apart.
static bool pollTap() {
  static bool down = false;
  static uint32_t downAt = 0;
  static int startX = 0, startY = 0, travel = 0;
  static uint32_t lastPoll = 0;
  if (!touchPresent || millis() - lastPoll < 10) return false;
  lastPoll = millis();
  Wire.beginTransmission(touchAddress); Wire.write(0x02);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(touchAddress, uint8_t(5)) != 5) return false;
  uint8_t fingers = Wire.read();
  int x = (Wire.read() & 15) << 8; x |= Wire.read();
  int y = (Wire.read() & 15) << 8; y |= Wire.read();
  bool pressed = fingers && x < W && y < H;
  if (pressed && !down) {
    down = true; downAt = millis(); startX = x; startY = y; travel = 0;
    return true;
  }
  if (pressed) travel = max(travel, abs(x - startX) + abs(y - startY));
  if (!pressed && down) {
    down = false;
    Serial.printf("aplwatch: touch at %d,%d for %lu ms, moved %d px\n", startX, startY, millis() - downAt, travel);
  }
  return false;
}

static void pushFull(const uint16_t *pixels) {
  lcd.push(0, 0, W, H, const_cast<uint16_t *>(pixels));
}

// Takes a finished light frame from core 0, if there is one.
static bool takeLightFrame() {
  if (!lightDone.load()) return false;
  std::swap(lightFront, lightBack);
  frontUtc = backUtc;
  lightDone.store(false);
  return true;
}

// Keeps digitalBg drawn for the given zone and the current minute. Returns
// true when it was redrawn. Called during the analog faces too, so a tap to
// a digital face only has to push it.
static bool prepareDigital(bool utc) {
  static int renderedKey = -1;
  tm clock{}; float frac; bool ready;
  zoneNow(utc, clock, frac, ready);
  int key = ready ? (clock.tm_hour * 60 + clock.tm_min) * 2 + utc : -2 - utc;
  if (key == renderedKey) return false;
  renderedKey = key;
  renderDigital({digitalBg, 0, 0, W, H}, clock.tm_hour, clock.tm_min, ready, USE_24_HOUR, utc ? WHITE : PINK);
  return true;
}

static void showDigital(bool force, bool utc) {
  if (prepareDigital(utc) || force) pushFull(digitalBg);
}

static void showLights(bool utc, bool switched) {
  if (takeLightFrame()) needFull = true;
  // After quick taps the fan may still be for the other zone; leave the
  // previous face up until core 0 delivers the right one.
  if (frontUtc != utc) return;
  tm clock{}; float frac; bool ready;
  zoneNow(utc, clock, frac, ready);
  if (!ready) {
    if (needFull) pushFull(lightFront);
    needFull = false;
    return;
  }
  float angle = (clock.tm_sec + frac) * PI_F / 30;
  Rect hand = secondsBounds(angle);
  if (needFull && switched) {
    // Right after a tap, push the fan as it is: copying it into scratch to
    // add the seconds hand cost ~40 ms. The hand follows on the next frame.
    pushFull(lightFront);
    lastHand = hand;
    needFull = false;
    return;
  }
  Rect area = needFull ? Rect{0, 0, W, H} : unite(hand, lastHand);
  Window win{scratch, area.x, area.y, area.w, area.h};
  copyFrom(win, lightFront);
  drawSeconds(win, angle, lightPalette(utc).seconds);
  lcd.push(area.x, area.y, area.w, area.h, scratch);
  lastHand = hand;
  needFull = false;
}

void setup() {
  Serial.begin(115200);
  if (!lcd.begin()) Serial.println("aplwatch: display initialization failed");
  lcd.setBrightness(BRIGHTNESS);
  delay(200); // Touch controller start-up after the expander reset.
  Wire.beginTransmission(touchAddress); touchPresent = Wire.endTransmission() == 0;
  // Disable auto-sleep so the first tap is not lost.
  if (touchPresent) { Wire.beginTransmission(touchAddress); Wire.write(0xFE); Wire.write(0xFF); Wire.endTransmission(); }
  rtcReady = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  prefs.begin("aplwatch");
  // Older builds saved "face": 0 digital, 1 lights (both Eastern).
  uint8_t saved = prefs.getUChar("mode", prefs.getUChar("face", 1) == 1 ? EASTERN_ANALOG : EASTERN_DIGITAL);
  mode = saved < MODE_COUNT ? Mode(saved) : EASTERN_ANALOG;
  lightUtc.store(lightUtcFor(mode));
  setenv("TZ", TIME_ZONE, 1); tzset();
  restoreTimeFromRtc();
  sntp_set_time_sync_notification_cb([](timeval *) { ntpEvent.store(true); });
  wifiConfigured = WIFI_SSID[0] != '\0' && strcmp(WIFI_SSID, "YOUR_WIFI_NAME") != 0;
  if (wifiConfigured) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttempt = millis();
  } else {
    Serial.println("aplwatch: enter Wi-Fi details in WiFiCredentials.h");
  }

  size_t bytes = sizeof(uint16_t) * W * H;
  digitalBg = static_cast<uint16_t *>(ps_malloc(bytes));
  lightFront = static_cast<uint16_t *>(ps_malloc(bytes));
  lightBack = static_cast<uint16_t *>(ps_malloc(bytes));
  scratch = static_cast<uint16_t *>(ps_malloc(bytes));
  if (!digitalBg || !lightFront || !lightBack || !scratch) {
    Serial.println("aplwatch: PSRAM allocation failed; check PSRAM=opi");
    for (;;) delay(1000);
  }
  memset(lightFront, 0, bytes);
  xTaskCreatePinnedToCore(lightTask, "lights", 4096, nullptr, 1, nullptr, 0);
  Serial.printf("aplwatch: touch %s, RTC %s, %s face. Serial: f cycles faces.\n",
                touchPresent ? "detected" : "absent", rtcReady ? "detected" : "absent", MODE_NAMES[mode]);
}

void loop() {
  updateNetwork();
  bool tapped = pollTap(), typed = false;
  while (Serial.available()) typed |= Serial.read() == 'f';
  static uint32_t switchedAt = 0;
  if (tapped || typed) {
    switchedAt = millis();
    mode = Mode((mode + 1) % MODE_COUNT);
    prefs.putUChar("mode", mode);
    uint32_t saveMs = millis() - switchedAt;
    lightUtc.store(lightUtcFor(mode));
    needFull = true;
    Serial.printf("aplwatch: %s face (%s), saved in %lu ms\n", MODE_NAMES[mode], tapped ? "tap" : "serial", saveMs);
  }
  static uint32_t lastFrame = 0;
  if (!isAnalog(mode)) {
    showDigital(needFull, isUtc(mode));
    needFull = false;
    // Keep the next analog face's light frame current for an instant switch.
    takeLightFrame();
  } else if (millis() - lastFrame >= FRAME_MS || needFull || lightDone.load()) {
    lastFrame = millis();
    showLights(isUtc(mode), switchedAt != 0);
    // The digital face after this one is in the same zone. Draw it ahead,
    // once the switch to this face is on screen.
    if (!switchedAt) prepareDigital(isUtc(mode));
  }
  if (switchedAt && !needFull) {
    Serial.printf("aplwatch: face on screen %lu ms after the switch\n", millis() - switchedAt);
    switchedAt = 0;
  }
  delay(2);
}
