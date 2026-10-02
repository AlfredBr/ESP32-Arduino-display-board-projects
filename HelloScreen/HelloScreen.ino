#include "BoardDisplay.h"

// Arduino IDE: ESP32C3 Dev Module, USB CDC On Boot = Enabled.
// Uses your installed LovyanGFX library. LVGL is not required.
BoardDisplay screen;
constexpr int DISPLAY_ENABLE_PIN = 3;
constexpr uint16_t BACKGROUND = 0x0000;  // Black (RGB565).
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t GREEN = 0x07E0;

void setup() {
  Serial.begin(921600);  // Keep your existing Serial Monitor setting.

  // GPIO 3 is set HIGH in the supplied board display examples.
  pinMode(DISPLAY_ENABLE_PIN, OUTPUT);
  digitalWrite(DISPLAY_ENABLE_PIN, HIGH);

  screen.init();
  screen.setRotation(0);  // Use 1, 2, or 3 to rotate the picture.
  screen.fillScreen(BACKGROUND);
  screen.setTextDatum(lgfx::textdatum_t::middle_center);

  // Keep the text inside the round screen's visible area.
  screen.drawCircle(120, 120, 112, CYAN);
  screen.drawCircle(120, 120, 110, CYAN);
  screen.setTextColor(CYAN, BACKGROUND);
  screen.setTextSize(2);
  screen.drawString("ESP32-C3", 120, 55);

  screen.setTextColor(WHITE, BACKGROUND);
  screen.setTextSize(3);
  screen.drawString("Hello,", 120, 92);
  screen.drawString("World!", 120, 124);

  screen.setTextSize(1);
  screen.drawString("LovyanGFX display demo", 120, 192);
  Serial.println("Screen initialized. Hello, World!");
}

void loop() {
  // Redraw only the counter area so the rest of the screen stays steady.
  char counter[24];
  const unsigned long seconds = millis() / 1000;
  snprintf(counter, sizeof(counter), "%lu s", seconds);

  screen.fillRect(40, 151, 160, 28, BACKGROUND);
  screen.setTextSize(2);
  screen.setTextColor(GREEN, BACKGROUND);
  screen.drawString(counter, 120, 165);

  Serial.printf("Screen running: %lu seconds\n", seconds);
  delay(1000);
}
