#pragma once
#include <Arduino_GFX_Library.h>
#include <Adafruit_XCA9554.h>
#include <Wire.h>
#include <pin_config.h>
#ifndef BOARD_AMOLED_V2
#define BOARD_AMOLED_V2 1
#endif

// Waveshare ESP32-S3-Touch-AMOLED-1.8, 368 x 448. Settings from wireframe_cube:
// QSPI CS 12, clock 11, data 4/5/6/7; I2C SDA 15, SCL 14 (shared with touch).
class BoardDisplay {
  Arduino_ESP32QSPI bus{LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3};
#if BOARD_AMOLED_V2
  Arduino_CO5300 panel{&bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT, 16, 0, 0, 0};
#else
  Arduino_SH8601 panel{&bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT};
#endif
public:
  bool begin() {
    pinMode(PA, OUTPUT);
    digitalWrite(PA, LOW); // Keep the speaker amplifier off.
    Wire.begin(IIC_SDA, IIC_SCL);
    Wire.setTimeOut(25);
#if BOARD_AMOLED_V2
    // The expander drives the panel and touch reset lines.
    Adafruit_XCA9554 expander;
    if (!expander.begin(0x20)) return false;
    for (int i = 0; i < 3; ++i) {
      expander.pinMode(i, OUTPUT);
      expander.digitalWrite(i, LOW);
    }
    delay(20);
    for (int i = 0; i < 3; ++i) expander.digitalWrite(i, HIGH);
    delay(30);
#endif
    if (!panel.begin()) return false;
    panel.setBrightness(160);
    panel.fillScreen(0);
    return true;
  }
  void setBrightness(uint8_t level) { panel.setBrightness(level); }
  // Blocking: the caller may reuse the pixels as soon as this returns.
  void push(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *pixels) {
    panel.draw16bitRGBBitmap(x, y, pixels, w, h);
  }
};
