# Wobbly Hyperspace

A luminous star tunnel for the round 240 x 240 GC9A01 display. 150 stars fly
toward the viewer and stretch into icy white, cyan and violet trails. The
vanishing point sways, the view rolls gently, and depth-dependent bends make
each streak curve as it comes closer. Soft additive halos brighten overlapping
stars. The outer few pixels fade into the circular screen edge.

Random speed targets arrive every 4-8 seconds and ease smoothly into faster
or slower flight. Each reboot gets a fresh hardware-random starfield. Animation
continues indefinitely, recycling stars as they leave the view.

## Build and upload

Uses the same ESP32-C3 and LovyanGFX configuration as WatchFace: SCLK 6, MOSI 7,
DC 2, CS 10, enable 3. Open `Hyperspace.ino` in Arduino IDE, select **ESP32C3
Dev Module** and **USB CDC On Boot: Enabled**, or run from this folder:

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM3
```

Compilation is the default. Upload replaces the sketch on the selected board.

## Tune the flight

`HyperspaceConfig.h` controls rotation, target FPS, star count, speed range,
trail duration and wobble strength. `WOBBLE=0` gives straight flight; `1.0`
is the default. Increase it moderately for more sway. The geometry is built
for a 240 x 240 display.

Rendering targets 30 FPS; actual frame rate requires checking on the device.
The sketch uses a 115,200-byte RGB565 sprite in internal RAM with an 8-bit
fallback. A readable framebuffer is required for additive lighting; the LCD
has no MISO line. If neither allocation succeeds, a memory message is shown.
No PSRAM, network connection or external image assets are required.

## Preview

Run `python preview.py` with Pillow and g++ installed. It compiles the actual
animation header against a desktop RGB565 adapter and generates a 12-second
`preview.gif`, `preview.png`, and `preview-contact.png`. Use `--output-dir
<folder>` on systems with protected Documents folders. Preview checks cover
motion, drawing bounds, ten minutes of star recycling and deterministic seeded
replay. The GIF restarts at its loop boundary; the device simulation does not.

## Validation

Compiled successfully with ESP32 core 3.3.12 and LovyanGFX 1.2.30:
386,370 bytes flash and 22,344 bytes static RAM, plus the runtime framebuffer.
The desktop build passed with warnings treated as errors; 180 frames, drawing
bounds, a ten-minute simulation and seeded replay passed. Preview frames were
visually inspected. Not uploaded or physically verified by Codex.
