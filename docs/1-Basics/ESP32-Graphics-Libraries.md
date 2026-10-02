# ESP32 graphics libraries: LovyanGFX and LVGL

Start with [ESP32 basics](ESP32-Basics-and-Core-Libraries.md). This guide describes the actual Arduino workspace Libraries folder, inspected September 30, 2026.

## 1. Installed libraries and their roles

| Library | Local version | Purpose |
| --- | --- | --- |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | 1.2.30 | Display hardware, drawing primitives, fonts, images, sprites. |
| [LVGL](https://github.com/lvgl/lvgl) | 9.6.0 | UI objects, styles, layouts, events, widgets, animation. |

Versions come from local library.properties files. Libraries/lv_conf.h is configuration, not a third library. It sets LV_COLOR_DEPTH to 16 and enables the widgets demo.

Use LovyanGFX alone for watch faces, games, counters, and custom animations. Use LVGL for buttons, sliders, menus, and multiple screens. They can work together:

```text
Application -> LVGL renders UI -> RAM pixel buffer
 -> flush callback -> LovyanGFX -> SPI -> display
Touch controller -> input callback -> LVGL events -> application
```

## 2. Your display hardware configuration

HelloScreen/BoardDisplay.h defines BoardDisplay, derived from lgfx::LGFX_Device, with a Bus_SPI and Panel_GC9A01. It connects the bus to the panel and the panel to the device.

| Setting | Existing project value |
| --- | --- |
| Controller / size | GC9A01 / 240 x 240 |
| SPI host | SPI2_HOST |
| SCLK / MOSI | GPIO 6 / GPIO 7 |
| DC / CS | GPIO 2 / GPIO 10 |
| MISO / reset | -1, not configured |
| Write clock | 80 MHz |
| Display enable | GPIO 3 set HIGH in HelloScreen.ino |

These are board-specific settings, not a generic wiring diagram. Copy them only for the same hardware. Round displays still have rectangular coordinates: corner pixels are outside the visible circle.

For the examples below, copy HelloScreen/BoardDisplay.h into your new sketch folder beside the .ino file. Use the original workspace path to find that header, even if reading this guide from another folder. If adding a build script for a new project, name it Build.ps1.

## 3. First LovyanGFX sketch

Complete example for the existing board, with the copied header:

```cpp
#include <Arduino.h>
#include "BoardDisplay.h"
BoardDisplay screen;
void setup() {
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  screen.init();
  screen.setRotation(0);
  screen.fillScreen(0x0000);
  screen.drawCircle(120, 120, 105, 0x07FF);
  screen.setTextDatum(lgfx::textdatum_t::middle_center);
  screen.setTextColor(0xFFFF, 0x0000);
  screen.setTextSize(2);
  screen.drawString("Hello ESP32", 120, 120);
}
void loop() { delay(10); }
```

init() initializes hardware. setRotation() changes orientation. Coordinates start upper-left, x increases right, y increases down. middle_center anchors text at its center.

## 4. Drawing vocabulary

| Call | Meaning |
| --- | --- |
| drawPixel(x,y,color) | One pixel. |
| drawLine(x1,y1,x2,y2,color) | A line. |
| drawRect / fillRect(x,y,w,h,color) | Outline or filled rectangle. |
| drawCircle / fillCircle(x,y,r,color) | Outline or filled circle. |
| setCursor then print | Cursor-based text. |
| drawString(text,x,y) | Text positioned using current datum. |
| setFont | Choose a font; explore the local examples. |
| pushImage | Transfer pixels; match format and byte order. |

RGB565 uses 5 red, 6 green, and 5 blue bits: two bytes per pixel. Black=0x0000, white=0xFFFF, red=0xF800, green=0x07E0, blue=0x001F. color565(r,g,b) converts 8-bit RGB components.

Test solid red/green/blue before changing color settings. Swapped red/blue may be panel rgb_order; broadly corrupted image colors may be byte order. Panel inversion is a separate setting.

## 5. Sprites and animation

A sprite is an off-screen image buffer. Draw into it, then transfer it to the display to reduce visible clearing and flicker. Redrawing only changed areas also helps.

Complete example, again with BoardDisplay.h:

```cpp
#include <Arduino.h>
#include "BoardDisplay.h"
BoardDisplay screen;
lgfx::LGFX_Sprite tile(&screen);
bool ready = false;
unsigned long lastFrame = 0;
int x = 10;
void setup() {
  pinMode(3, OUTPUT); digitalWrite(3, HIGH);
  screen.init(); screen.fillScreen(0x0000);
  tile.setColorDepth(16);
  ready = tile.createSprite(120, 40) != nullptr;
}
void loop() {
  unsigned long now = millis();
  if (ready && now - lastFrame >= 33UL) {
    lastFrame = now;
    tile.fillSprite(0x0000);
    tile.fillCircle(x, 20, 8, 0x07E0);
    tile.pushSprite(60, 100);
    x = x >= 110 ? 10 : x + 2;
  }
  delay(1);
}
```

The sprite needs about 9,600 bytes plus overhead. A 240 x 240 RGB565 framebuffer needs 115,200 bytes; two need 230,400 bytes before other allocations. Start small on C3. PSRAM is not guaranteed. Allocate once, reuse buffers, check results, and deleteSprite() when finished.

startWrite()/endWrite() group transactions; keep them balanced on shared buses. With DMA, buffers must remain valid and unchanged until the transfer completes. Start with synchronous transfers.

## 6. LVGL objects, styles, and events

LVGL retains a tree of objects. Create a widget once, then update its properties. Parent objects define ownership and placement; deleting a parent deletes children. Styles control colors, borders, padding, and fonts; flex layout arranges multiple widgets.

Fragment after LVGL/display initialization:

```cpp
lv_obj_t *label = lv_label_create(lv_screen_active());
lv_label_set_text(label, "Hello LVGL");
lv_obj_center(label);
```

Use lv_button_create() for a button and lv_obj_add_event_cb() to attach a callback filtered to LV_EVENT_CLICKED. LV_EVENT_VALUE_CHANGED is useful for sliders. Callback signature: void onClick(lv_event_t *event). A drawn button needs a registered input device to receive clicks.

## 7. Minimal LVGL 9 + LovyanGFX integration

LVGL needs a display, persistent pixel buffers, a flush callback, an elapsed-time source, and frequent timer servicing. This complete board-specific sketch requires the copied BoardDisplay.h and your local LVGL 9 configuration:

```cpp
#include <Arduino.h>
#include "BoardDisplay.h"
#include <lvgl.h>
BoardDisplay screen;
alignas(4) static uint8_t drawBuffer[240 * 20 * 2];
static uint32_t tickMillis() { return millis(); }
static void flushPixels(lv_display_t *display,
                        const lv_area_t *area, uint8_t *pixels) {
  int32_t width = area->x2 - area->x1 + 1;
  int32_t height = area->y2 - area->y1 + 1;
  screen.startWrite();
  screen.pushImage(area->x1, area->y1, width, height,
                   reinterpret_cast<const uint16_t *>(pixels));
  screen.endWrite();
  lv_display_flush_ready(display);
}
void setup() {
  Serial.begin(115200);
  pinMode(3, OUTPUT); digitalWrite(3, HIGH);
  screen.init(); screen.setRotation(0);
  screen.setSwapBytes(false); // Native RGB565, no extra byte swapping.
  screen.fillScreen(0x0000);
  lv_init();
  lv_tick_set_cb(tickMillis);
  lv_display_t *display = lv_display_create(240, 240);
  if (!display) {
    Serial.println("Display allocation failed");
    while (true) { delay(1000); }
  }
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, drawBuffer, nullptr,
                         sizeof(drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flushPixels);
  lv_obj_t *label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Hello LVGL 9");
  lv_obj_center(label);
}
void loop() {
  lv_timer_handler();
  delay(5);
}
```

The 20-row buffer needs 9,600 bytes. Buffer capacity is in **bytes**, not pixels. Static storage keeps it alive after setup(). Partial rendering avoids a full framebuffer.

This uses synchronous pushImage: pixels are consumed before flush_ready. If switching to DMA, signal flush completion only after DMA finishes. Otherwise LVGL may overwrite data still being transferred. Do not add a second byte swap unless the actual pixel format requires it.

The tick supplies time; lv_timer_handler() performs rendering, input, and animation work. Neither replaces the other. Avoid blocking network calls and long delays in the UI loop. Use one owning task or explicit synchronization; do not draw from interrupts.

See the [official integration overview](https://lvgl.io/docs/open/9.5/integration/overview). The local Libraries/lvgl/examples/arduino/LVGL_Arduino example shows v9 registration but its generic flush/touch callbacks need hardware implementations.

## 8. Touch is separate from the display

The older widgets sketch includes CST816D.h, SDA=4, SCL=5, interrupt=0, reset=1. Verify your actual controller and wiring instead of assuming those pins apply. A controller driver can live inside a sketch rather than Libraries.

1. Initialize the controller; verify coordinates in Serial.
2. Create input with lv_indev_create().
3. Set LV_INDEV_TYPE_POINTER.
4. Register lv_indev_set_read_cb().
5. Report LV_INDEV_STATE_PRESSED or RELEASED and point.x/point.y.
6. Map coordinates to the display rotation.

The v9 read callback signature is void readTouch(lv_indev_t *input, lv_indev_data_t *data). Keep callbacks brief.

## 9. Important version mismatch

3_2_TFT-LVGL-Widgets/LvglWidgets/LvglWidgets.ino uses v8 APIs. Installed LVGL is 9.6.0; the old sketch can fail unchanged.

| v8 | v9 direction |
| --- | --- |
| lv_disp_draw_buf_t | Persistent buffers via lv_display_set_buffers |
| lv_disp_drv_t / lv_disp_drv_register | lv_display_create and display setters |
| lv_disp_flush_ready | lv_display_flush_ready |
| lv_indev_drv_t / lv_indev_drv_register | lv_indev_create and input setters |
| lv_scr_act | lv_screen_active |
| lv_btn_create | lv_button_create |

Migration needs more than renaming: signatures, buffer units, color handling, and configuration also change. Prefer v9 examples for new work. Reproduce vendor v8 demos in an isolated dependency environment if needed; do not casually replace the shared v9 library.

lv_conf.h affects multiple projects. Arduino demo/example compilation may require additional source setup described by the local LVGL Arduino example. You do not need demo compilation for your own label or button.

## 10. Local learning resources

| Path relative to Arduino workspace | Lesson |
| --- | --- |
| HelloScreen/HelloScreen.ino and BoardDisplay.h | Board configuration and text. |
| Libraries/LovyanGFX/examples/HowToUse | Setup, fonts, images, drawing. |
| Libraries/LovyanGFX/examples/Sprite | Animation, rotation, gauges. |
| Libraries/LovyanGFX/examples/Test/TFT_graphicstest_PDQ | Drawing benchmark. |
| Libraries/lvgl/examples/arduino/LVGL_Arduino | v9 integration structure. |
| Libraries/lvgl/examples/widgets | Individual widgets. |
| Libraries/lvgl/demos/widgets | Larger UI after integration. |
| 3_2_TFT-LVGL-Widgets/LvglWidgets | Historical board/touch wiring, requiring migration. |

## 11. Practice and troubleshooting

First draw text, update a counter without clearing the whole screen, and animate a sprite. Then run the LVGL label bridge, add a button, verify touch, and connect events. Finally show a sensor reading by updating an existing label.

| Symptom | Check |
| --- | --- |
| Blank screen | Power/enable, controller, pin assignments, init. |
| LovyanGFX works, LVGL blank | Real flush transfer, buffer registration, timer handler. |
| First frame then stalls | Missing flush completion, blocked loop, DMA lifetime. |
| Wrong colors | Panel RGB/BGR order versus pixel byte order. |
| Misaligned touch | Rotation and coordinate mapping. |
| Resets/allocation failure | Buffer size, heap/stack, power, PSRAM assumptions. |
| lv_disp_drv_t not found | v8 example against v9 installation. |
| Unexpected library version | Check compiler-selected library paths. |

Examples were reviewed against local APIs but have not been compiled or tested on hardware. They depend on the board configuration described above.
