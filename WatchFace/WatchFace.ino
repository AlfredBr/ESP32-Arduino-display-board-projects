#include "BoardDisplay.h"
#include "WatchFaceDrawing.h"
#include <esp_timer.h>
#include <stdlib.h>
#include <time.h>

// ESP32C3 Dev Module; USB CDC On Boot = Enabled; LovyanGFX library.
BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
bool frameReady = false;
time_t referenceEpoch = 0;
int64_t clockStartedUs = 0;

void logIfRoom(const char* message) {
  const size_t length = strlen(message);
  if (Serial.availableForWrite() >= static_cast<int>(length)) {
    Serial.write(reinterpret_cast<const uint8_t*>(message), length);
  }
}

void setup() {
  Serial.begin(921600);
  Serial.setTxTimeoutMs(0);  // A closed Serial Monitor must not stall the clock.

  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);  // Board enable setting from your factory examples.
  screen.init();
  screen.setRotation(SCREEN_ROTATION);

  // One 16-bit frame is 115,200 bytes. Use C3 internal RAM, not PSRAM.
  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady = frame.createSprite(240, 240) != nullptr;
  if (!frameReady) {
    logIfRoom("Frame buffer unavailable; drawing directly to the display.\n");
  }

  // UTC is used only for predictable calendar arithmetic in this demo.
  // No Wi-Fi, RTC, timezone inference, or heart-rate measurement is involved.
  setenv("TZ", "UTC0", 1);
  tzset();
  tm reference = {};
  reference.tm_year = START_YEAR - 1900;
  reference.tm_mon = START_MONTH - 1;
  reference.tm_mday = START_DAY;
  reference.tm_hour = START_HOUR;
  reference.tm_min = START_MINUTE;
  reference.tm_sec = START_SECOND;
  reference.tm_isdst = 0;
  referenceEpoch = mktime(&reference);
  clockStartedUs = esp_timer_get_time();
  logIfRoom("WatchFace ready. Reference time 21:44; heart rate is sample data.\n");
}

void loop() {
  // The 64-bit monotonic timer also avoids millis()'s 49-day wraparound.
  static int64_t lastElapsedSecond = -1;
  const int64_t elapsed = (esp_timer_get_time() - clockStartedUs) / 1000000;
  if (elapsed != lastElapsedSecond) {
    lastElapsedSecond = elapsed;
    const time_t now = referenceEpoch + elapsed;
    tm clock;
    gmtime_r(&now, &clock);
    if (frameReady) {
      WatchFace::draw(frame, clock);
      frame.pushSprite(0, 0);
    } else {
      WatchFace::draw(screen, clock);
    }
  }
  delay(5);
}
