# Blinking Eye

A human eye for the ESP32-C3 and round 240 x 240 GC9A01 screen used by WatchFace.
Original procedural textures provide blue-green iris fibers, a gold collarette,
a dark limbal ring, subtle capillaries, skin grain and fine eyelashes.
Curved lids close in 100 ms, pause briefly and reopen over 200 ms, with occasional
double blinks. Small rapid gaze shifts, pupil variation, lid shadows and corneal
reflections add life. No Wi-Fi or PSRAM is needed.

## Build and upload

Open `BlinkingEye.ino` in Arduino IDE with **ESP32C3 Dev Module** and
**USB CDC On Boot: Enabled**, or run from this folder:

```powershell
.\Build-BlinkingEye.ps1
.\Build-BlinkingEye.ps1 -Upload -Port COM3
```

The script compiles only unless `-Upload` is supplied. The pinout matches
WatchFace: SCLK 6, MOSI 7, DC 2, CS 10, enable 3.

`EyeConfig.h` controls rotation, target frame rate, blink spacing and gaze range.
Rendering targets 30 FPS; the actual rate depends on board performance. Textures
stay in flash, and a 115,200-byte RGB565 sprite buffers the output in internal
RAM. If allocation fails, it falls back to direct scanline drawing, which may
show tearing. The layout is specifically designed for 240 x 240.

## Assets and preview

`EyeTextures.h` is included and ready to compile. `generate_assets.py` recreates
it using NumPy and Pillow. These are original mathematical textures, not a
photograph. `preview.py` compiles the actual animation against a desktop renderer
using g++ and saves an animated GIF and stills (requires Pillow). Both scripts
accept `--output-dir <folder>` for systems with protected Documents folders.

The desktop preview checks blinking, reopening, scanline bounds and a ten-minute
simulation. It cannot verify device frame rate or physical LCD color rendering.

## Validation and deployment

Compiled with ESP32 core 3.3.12 and LovyanGFX 1.2.30: 639,358 bytes flash and
19,688 bytes static RAM, plus the runtime frame buffer. The desktop build passed
with warnings treated as errors; 240 frames were rendered and the open,
partially closed, closed and reopening frames were visually inspected.
Uploaded successfully to ESP32-C3 on COM3; esptool verified the flash data and
reset the board. Physical appearance and frame rate still require observation
on the actual screen.
