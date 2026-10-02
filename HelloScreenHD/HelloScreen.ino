#include "BoardDisplay.h"

// Arduino IDE: ESP32C3 Dev Module, USB CDC On Boot = Enabled.
// Uses your installed LovyanGFX library. LVGL is not required.
BoardDisplay screen;
constexpr int DISPLAY_ENABLE_PIN = 3;
constexpr uint16_t BACKGROUND = 0x0000;  // Black (RGB565).
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t GREEN = 0x07E0;

// Logging is optional: skip a message if the host is not draining USB output.
// Never make the screen wait for Serial Monitor to read a message.
void logIfRoom(const char* message) {
  const size_t length = strlen(message);
  if (Serial.availableForWrite() >= static_cast<int>(length)) {
    Serial.write(reinterpret_cast<const uint8_t*>(message), length);
  }
}

void setup() {
  Serial.begin(921600);  // Keep your existing Serial Monitor setting.
  Serial.setTxTimeoutMs(0);  // ESP32-C3 USB CDC: do not wait for a full TX buffer.

  // GPIO 3 is set HIGH in the supplied board display examples.
  pinMode(DISPLAY_ENABLE_PIN, OUTPUT);
  digitalWrite(DISPLAY_ENABLE_PIN, HIGH);

  screen.init();
  screen.setRotation(0);  // Use 1, 2, or 3 to rotate the picture.
  screen.fillScreen(BACKGROUND);
  screen.setTextDatum(lgfx::textdatum_t::middle_center);
  screen.setTextSize(1);  // Draw each font at its native resolution.

  // Keep the text inside the round screen's visible area.
  screen.drawCircle(120, 120, 112, CYAN);
  screen.drawCircle(120, 120, 110, CYAN);
  screen.setTextColor(CYAN, BACKGROUND);
  screen.setFont(&fonts::FreeSansBold9pt7b);
  screen.drawString("ESP32-C3", 120, 50);

  screen.setTextColor(WHITE, BACKGROUND);
  // Larger, detailed glyphs instead of magnifying the tiny default font.
  screen.setFont(&fonts::FreeSansBold18pt7b);
  screen.drawString("Hello,", 120, 87);
  screen.drawString("World!", 120, 124);

  screen.setFont(&fonts::FreeSans9pt7b);
  screen.drawString("LovyanGFX", 120, 198);
  logIfRoom("Screen initialized. Hello, World!\n");
}

void loop() {
  // Update when the uptime second changes, instead of adding a one-second
  // delay after drawing/logging. Drawing time no longer accumulates as drift.
  static unsigned long lastDisplayedSecond = ~0UL;
  const unsigned long seconds = millis() / 1000;
  if (seconds != lastDisplayedSecond) {
    lastDisplayedSecond = seconds;
    char counter[24];
    snprintf(counter, sizeof(counter), "%lu s", seconds);

    // Redraw only the counter area, keeping the greeting and font unchanged.
    screen.fillRect(32, 148, 176, 36, BACKGROUND);
    screen.setFont(&fonts::FreeSans12pt7b);
    screen.setTextColor(GREEN, BACKGROUND);
    screen.drawString(counter, 120, 165);

    char message[64];
    snprintf(message, sizeof(message), "Screen running: %lu seconds\n", seconds);
    logIfRoom(message);
  }
  delay(5);  // Yield to background tasks between checks.
}
