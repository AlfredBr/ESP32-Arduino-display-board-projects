#include <Arduino.h>
#include <string.h>
#include "BoardDisplay.h"
#include "WatchFaceDrawing.h"
#include "WiFiCredentials.h"
#include <WiFi.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <time.h>

// ESP32C3 Dev Module; USB CDC On Boot = Enabled; LovyanGFX library.
BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
bool frameReady = false;
bool wifiConfigured = false;
bool timeServiceStarted = false;
bool wasConnected = false;
uint32_t lastWifiAttemptMs = 0;

void logIfRoom(const char* message) {
  const size_t length = strlen(message);
  if (Serial.availableForWrite() >= static_cast<int>(length)) {
    Serial.write(reinterpret_cast<const uint8_t*>(message), length);
  }
}

void updateNetwork() {
  if (!wifiConfigured) {
    return;
  }
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    // Start (or restart after reconnecting) the asynchronous NTP service.
    // It updates system time and periodically resynchronizes in background.
    configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);
    timeServiceStarted = true;
    logIfRoom("Wi-Fi connected. Synchronizing internet time.\n");
  }
  if (!connected && (uint32_t)(millis() - lastWifiAttemptMs) >= WIFI_RETRY_MS) {
    lastWifiAttemptMs = millis();
    WiFi.reconnect();  // Initiates a retry; there is no waiting loop here.
  }
  wasConnected = connected;
}

void setup() {
  Serial.begin(921600);
  Serial.setTxTimeoutMs(0);  // A closed Serial Monitor must not stall the clock.

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
    logIfRoom("Enter Wi-Fi details in WiFiCredentials.h, then compile and upload.\n");
  }

  // Allocate after starting Wi-Fi so its initial allocations take priority.
  // A full 16-bit frame uses 115,200 bytes of internal RAM; no PSRAM needed.
  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady = frame.createSprite(240, 240) != nullptr;
  if (!frameReady) {
    // An 8-bit frame still preserves the watch colors and uses half the RAM.
    frame.setColorDepth(8);
    frameReady = frame.createSprite(240, 240) != nullptr;
  }
  if (!frameReady) {
    logIfRoom("Frame buffer unavailable; drawing directly to the display.\n");
  }
}

void loop() {
  updateNetwork();

  // A monotonic tick schedules drawing even before internet time arrives.
  static int64_t lastDrawSecond = -1;
  const int64_t elapsed = esp_timer_get_time() / 1000000;
  if (elapsed != lastDrawSecond) {
    lastDrawSecond = elapsed;
    const time_t now = time(nullptr);
    // No made-up date while waiting for the first sync. A system clock from
    // 2024 or later is plausible; once set it continues running while offline.
    const bool timeReady = now >= 1704067200;
    tm clock = {};
    if (timeReady) {
      localtime_r(&now, &clock);  // Applies Eastern timezone and DST rules.
    } else {
      clock.tm_sec = elapsed % 60;  // Keep the dot alive during setup.
    }

    const char* status = nullptr;
    if (!wifiConfigured) {
      status = "Set Wi-Fi details";
    } else if (WiFi.status() != WL_CONNECTED) {
      status = timeReady ? "Offline" : "Connecting Wi-Fi";
    } else if (!timeReady) {
      status = timeServiceStarted ? "Waiting for NTP" : "Starting sync";
    }

    if (frameReady) {
      WatchFace::draw(frame, clock, timeReady, status);
      frame.pushSprite(0, 0);
    } else {
      WatchFace::draw(screen, clock, timeReady, status);
    }
  }
  delay(5);
}
