# PieMenu

Round-display buttons built as custom LVGL 9.6 objects. The demo targets the
existing ESP32-C3 / GC9A01 240 x 240 board, using the same SPI and touch pins as
LVGL_Demo. Other projects only need lv_piemenu.h and lv_piemenu.cpp.

## Run

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM3
```

The demo starts with four full-pie slices. Tap Next (or the arrow) to cycle
through two to eight slices. Ring pages have a center Next button. The first
slice toggles its checked state; the penultimate slice is disabled on pages
with six or more slices. Other slices log clicks to USB Serial at 115200 baud.

Serial commands: `n` next, `p` previous, `2` through `8` slice count, `r` switch
ring/pie, `a` toggle the eight-second tour. Without a detected touch controller,
the tour starts automatically. With touch, it waits for you.

## API example

```cpp
#include "lv_piemenu.h"

static void lights_clicked(lv_event_t *event) {
    // The event target is the slice, just as with an LVGL button.
}

lv_obj_t *menu = lv_piemenu_create(lv_screen_active());
lv_obj_set_size(menu, 240, 240);
lv_obj_center(menu);
lv_piemenu_set_rotation(menu, -135); // first of four slices centered at top
lv_piemenu_set_gap(menu, 6);         // separator width in pixels; zero fills gaps

lv_obj_t *lights = lv_piemenu_add_button(menu);
lv_piemenu_button_set_text(lights, "Lights");
lv_obj_add_event_cb(lights, lights_clicked, LV_EVENT_CLICKED, nullptr);
lv_obj_set_style_bg_color(lights, lv_color_hex(0x185B82), LV_PART_MAIN);
lv_obj_set_style_bg_color(lights, lv_color_hex(0x428BAC), LV_STATE_PRESSED);

// Add three more slices. Angles and labels reflow automatically.
for (const char *text : {LV_SYMBOL_AUDIO, LV_SYMBOL_SETTINGS, "Back"}) {
    lv_piemenu_button_set_text(lv_piemenu_add_button(menu), text);
}

lv_piemenu_set_inner_radius(menu, 38); // pixels; zero returns to full pie
lv_obj_t *home = lv_piemenu_get_center_button(menu);
lv_piemenu_button_set_text(home, LV_SYMBOL_HOME);
```

- Angles increase clockwise from the right. Default starting angle: -90 degrees.
- Up to eight equal slices. Adding a ninth returns nullptr. Use two to eight
  for practical menus; a single slice is supported while constructing a menu.
- The circle fits the smaller menu dimension. Keep menu padding at zero and
  let the widget control slice position and size.
- Inner radius is clamped to 70% of the outer radius to preserve ring space.
  Zero hides and retains the center object; getting it before first creating
  a ring returns nullptr. Deleting the center clears its stored pointer.
- Separator edges are parallel, with a constant width in pixels (default 6).
  Excessive widths are clamped so the central inset stays within 20% of the
  outer radius. Corners, gaps, and the center
  hole do not activate a slice. The center has a one-pixel inset for separation.
- `lv_obj_set_checkable(button, true)` enables standard toggling;
  `lv_obj_add_state(button, LV_STATE_DISABLED)` disables input.
- Colors, background opacity, text colors, fonts, and state selectors follow
  LVGL conventions. `lv_piemenu_button_get_label()` exposes the label for
  additional label formatting. Text and LVGL symbol icons are supported;
  keep labels short enough to fit. Bitmap icon placement is not automated.
- Delete slices or the menu with `lv_obj_delete()`. Slice angles reflow on
  add/delete and labels reflow on resize. Avoid reparenting slices or deleting
  their internal labels; they belong to their menu.
- Curved fills replace LVGL's rectangular background rendering. Rectangle
  borders, shadows, gradients, and corner-radius styles are not implemented
  for sectors. The initial rasterizer has pixel-aligned, non-antialiased edges.
- The implementation uses LVGL private class definitions and is tested against
  the workspace's 9.6.0 library. Recheck it when changing LVGL versions.

## Desktop verification

`Test.ps1` requires the installed CMake and MinGW tools. It builds against the
actual workspace LVGL sources with the same 96 KB LVGL pool and 240 x 24 partial
buffer as the device. Build outputs and PPM screenshots go to the temporary
PieMenu-host-build folder. Tests cover every slice count in both layouts,
real LVGL pointer input, state changes, gaps/corners, label positions, resizing,
slice replacement, center deletion/recreation, solid fills, and repeated
creation/deletion without accumulating memory. PNG previews in `previews`
were converted from those real LVGL renderings.

After uploading, run `.\Check-Device.ps1` to exercise all seven pages over
USB Serial and leave the four-slice demo ready for touch. This checks firmware
responses; physical touch accuracy and the panel's appearance still need a
hands-on review.


