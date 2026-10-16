#pragma once
#include <Arduino_GFX_Library.h>
#include <Adafruit_XCA9554.h>
#include <Wire.h>
#include <pin_config.h>
#include "CubeConfig.h"
#ifndef BOARD_AMOLED_V2
#define BOARD_AMOLED_V2 1
#endif

class BoardDisplay {
  Arduino_ESP32QSPI bus{LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3};
#if BOARD_AMOLED_V2
  Arduino_CO5300 panel{&bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT, 16, 0, 0, 0};
#else
  Arduino_SH8601 panel{&bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT};
#endif
public:
  Arduino_GFX& graphics() { return panel; }
  bool begin() {
    pinMode(PA, OUTPUT);
    digitalWrite(PA, LOW);
    Wire.begin(IIC_SDA, IIC_SCL);
    Wire.setTimeOut(25);
#if BOARD_AMOLED_V2
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
    panel.setBrightness(CubeConfig::BRIGHTNESS);
    panel.fillScreen(0);
    return true;
  }
  void present(uint16_t* frame) {
    panel.draw16bitRGBBitmap((LCD_WIDTH - CubeConfig::WIDTH) / 2,
      (LCD_HEIGHT - CubeConfig::HEIGHT) / 2, frame, CubeConfig::WIDTH, CubeConfig::HEIGHT);
  }
};
