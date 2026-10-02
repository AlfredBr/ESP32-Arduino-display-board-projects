# LVGL widget gallery

For this workspace's ESP32-C3, round 240x240 GC9A01 display and CST816D touch controller. Uses the installed LVGL 9.6 and LovyanGFX libraries.

## Build and upload

From the Arduino workspace:

```powershell
.\LVGL_Demo\Build.ps1
.\LVGL_Demo\Build.ps1 -Upload -Port COM3
```

Change COM3 to your board's port. Build.ps1 is required: it supplies LVGL 9 configuration flags for every library translation unit, bypassing the workspace's older LVGL 8 configuration without modifying it. A plain Arduino IDE build does not supply these flags.

## Explore

Tap the bottom arrows to browse 33 pages. Manipulate sliders, switches, rollers and other controls; the status line reports events. The serial monitor at 115200 baud accepts `n` (next), `p` (previous) and `a` (toggle a six-second automatic tour). Serial navigation also works when touch hardware is absent.

The gallery covers all 33 standard widgets enabled by LVGL 9.6's default configuration, from labels and buttons to charts, canvas, images, menus, tabs and windows. Optional widgets needing additional configuration or external libraries (such as QR codes, barcodes, GIF, video and Lottie) are not enabled. Complex widgets are miniature examples; keyboard keys and calendar dates are necessarily small on this display.

## Learn from the code

Start with `showPage()` in LVGL_Demo.ino. Each numbered case demonstrates the create/configure sequence for the corresponding widget. `changed()` demonstrates event callbacks. `setup()` connects the display, touch driver and millisecond clock; `loop()` services LVGL and defers page deletion until callbacks have returned.

Only one page exists at a time. The partial RGB565 display buffer uses 11,520 bytes and the LVGL heap is 96 KB. Images and canvas use small generated buffers, so no SD card or downloaded assets are needed. Display transfers are synchronous to avoid reusing a buffer while DMA is still reading it.

Pins: display SCLK 6, MOSI 7, DC 2, CS 10, enable 3; touch SDA 4, SCL 5, reset 1, interrupt 0. Rotation is zero, matching the factory sample.

LVGL 9 widget documentation: https://docs.lvgl.io/9.4/details/widgets/index.html
