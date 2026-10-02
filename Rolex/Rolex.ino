#include "BoardDisplay.h"
#include "RolexConfig.h"
#include "RolexDrawing.h"
#include "WiFiCredentials.h"
#include <WiFi.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
bool frameReady = false;
bool wifiConfigured = false;
bool wasConnected = false;
uint32_t lastWifiAttemptMs = 0;

void logIfRoom(const char* message) {
  const size_t length = strlen(message);
  if (Serial.availableForWrite() >= static_cast<int>(length)) {
    Serial.write(reinterpret_cast<const uint8_t*>(message), length);
  }
}

void updateNetwork() {
  if (!wifiConfigured) return;
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);
    logIfRoom("Wi-Fi connected. Synchronizing internet time.\n");
  }
  if (!connected && static_cast<uint32_t>(millis() - lastWifiAttemptMs) >= WIFI_RETRY_MS) {
    lastWifiAttemptMs = millis();
    WiFi.reconnect();
  }
  wasConnected = connected;
}

void setup() {
  Serial.begin(921600);
  Serial.setTxTimeoutMs(0);
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  screen.init();
  screen.setRotation(SCREEN_ROTATION);
  setenv("TZ", TIME_ZONE, 1);
  tzset();
  wifiConfigured = WIFI_SSID[0] != '\0' && strcmp(WIFI_SSID, "YOUR_WIFI_NAME") != 0;
  if (wifiConfigured) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttemptMs = millis();
  } else {
    logIfRoom("Enter Wi-Fi details in WiFiCredentials.h before uploading.\n");
  }
  // Allocate after Wi-Fi; the full RGB565 frame needs 115,200 bytes.
  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady = frame.createSprite(240, 240) != nullptr;
  if (!frameReady) {
    frame.setColorDepth(8);
    frameReady = frame.createSprite(240, 240) != nullptr;
  }
  if (!frameReady) logIfRoom("Drawing directly: framebuffer unavailable.\n");
}

void loop() {
  updateNetwork();
  static int64_t lastDrawFrame = -1;
  const int64_t elapsed = esp_timer_get_time() / 40000;
  if (elapsed != lastDrawFrame) {
    lastDrawFrame = elapsed;
    timeval wallTime = {};
    gettimeofday(&wallTime, nullptr);
    const time_t now = wallTime.tv_sec;
    // Once synchronized, system time continues through network outages.
    tm clock = {};
    const bool timeReady = now >= 1704067200 && localtime_r(&now, &clock) != nullptr;
    if (frameReady) {
      RolexFace::draw(frame, clock, timeReady, wallTime.tv_usec / 1000000.0f);
      frame.pushSprite(0, 0);
    } else {
      RolexFace::draw(screen, clock, timeReady, wallTime.tv_usec / 1000000.0f);
    }
  }
  delay(5);
}

