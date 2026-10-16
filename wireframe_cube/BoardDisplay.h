#pragma once
#include <Arduino_GFX_Library.h>
#include <Adafruit_XCA9554.h>
#include <Wire.h>
#include <pin_config.h>
#include "CubeConfig.h"
#include "ShapeTypes.h"
#ifndef BOARD_AMOLED_V2
#define BOARD_AMOLED_V2 1
#endif

class BoardDisplay {
  Cube::Shape displayedShape = Cube::Shape::Count;
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
  void present(uint16_t* frame, Cube::Shape shape) {
    panel.draw16bitRGBBitmap((LCD_WIDTH - CubeConfig::WIDTH) / 2,
      (LCD_HEIGHT - CubeConfig::HEIGHT) / 2, frame, CubeConfig::WIDTH, CubeConfig::HEIGHT);
    if (shape != displayedShape) {
      // Place the label in the black margin below the animation region.
      const int top = (LCD_HEIGHT + CubeConfig::HEIGHT) / 2;
      panel.fillRect(0, top, LCD_WIDTH, LCD_HEIGHT - top, 0);
      panel.setTextSize(2);
      panel.setTextColor(0xFFFF);
      const char* name = Cube::shapeName(shape);
      int16_t x, y;
      uint16_t width, height;
      panel.getTextBounds(name, 0, 0, &x, &y, &width, &height);
      panel.setCursor((LCD_WIDTH - width) / 2 - x,
        top + (LCD_HEIGHT - top - height) / 2 - y);
      panel.print(name);
      displayedShape = shape;
    }
  }
};
