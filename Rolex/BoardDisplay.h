#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Settings from your Factory_samples_without_touch example.
// GC9A01, 240 x 240: SCLK=6, MOSI=7, DC=2, CS=10.
class BoardDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 panel;
  lgfx::Bus_SPI bus;

public:
  BoardDisplay() {
    auto spi = bus.config();
    spi.spi_host = SPI2_HOST;
    spi.spi_mode = 0;
    spi.freq_write = 80000000;
    spi.freq_read = 20000000;
    spi.spi_3wire = true;
    spi.use_lock = true;
    spi.dma_channel = SPI_DMA_CH_AUTO;
    spi.pin_sclk = 6;
    spi.pin_mosi = 7;
    spi.pin_miso = -1;
    spi.pin_dc = 2;
    bus.config(spi);
    panel.setBus(&bus);

    auto lcd = panel.config();
    lcd.pin_cs = 10;
    lcd.pin_rst = -1;
    lcd.pin_busy = -1;
    lcd.memory_width = 240;
    lcd.memory_height = 240;
    lcd.panel_width = 240;
    lcd.panel_height = 240;
    lcd.offset_x = 0;
    lcd.offset_y = 0;
    lcd.offset_rotation = 0;
    lcd.dummy_read_pixel = 8;
    lcd.dummy_read_bits = 1;
    lcd.readable = false;
    lcd.invert = true;
    lcd.rgb_order = false;
    lcd.dlen_16bit = false;
    lcd.bus_shared = false;
    panel.config(lcd);
    setPanel(&panel);
  }
};
