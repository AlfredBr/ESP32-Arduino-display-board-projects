# HelloScreen

A simple hello-world display for the ESP32-C3 and 240 x 240 GC9A01 screen
configured in your Arduino factory examples. Uses installed LovyanGFX 1.2.30.

## Run it

1. Open HelloScreen.ino in Arduino IDE (File > Open).
2. Select Tools > Board > esp32 > ESP32C3 Dev Module.
3. Select Tools > USB CDC On Boot > Enabled.
4. Select Tools > Port > COM3 (or the board's current port).
5. Click Upload. The sketch starts automatically.

Expected screen: cyan circle, ESP32-C3 title, Hello, World!, and a green
seconds-since-start counter. Optional Serial Monitor: 921600 baud.
If the display is rotated, change screen.setRotation(0) to 1, 2, or 3.

Keep BoardDisplay.h in the same folder as HelloScreen.ino. The header holds
the panel configuration; setup() draws the greeting once, and loop() updates
the counter once per second. To change the greeting, edit drawString calls.

## Hardware settings copied from your examples

GC9A01 panel, 240 x 240, SPI2, 80 MHz write / 20 MHz read.
SCLK GPIO 6; MOSI GPIO 7; DC GPIO 2; CS GPIO 10; no separate reset pin.
GPIO 3 is driven HIGH, as in the supplied examples. No touch input is used.
Reference: Arduino/1_2_Factory_samples_without_touch/Factory_samples_without_touch/
Factory_samples_without_touch.ino.

The provided LVGL demos use v8 APIs while the installed LVGL is v9.6.0.
This sketch draws directly with LovyanGFX and does not require LVGL or lv_conf.h.

## Stopping

No stop step is needed before uploading: a new upload replaces the sketch.
Closing Serial Monitor does not stop the board. RESET restarts the sketch.
Removing all power stops it; reconnecting power runs the saved sketch again.

Saved in Downloads because Windows Controlled Folder Access blocked creation
inside the OneDrive Documents/Arduino folder. You may move this entire folder
into your Arduino sketchbook using File Explorer.
