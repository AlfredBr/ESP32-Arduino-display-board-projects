#pragma once
#include <Arduino_GFX_Library.h>
#include <pin_config.h>
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
  void setBrightness(uint8_t value) { panel.setBrightness(value); }
  bool begin() {
    if (!panel.begin()) return false;
    panel.setBrightness(160);
    return true;
  }
};


