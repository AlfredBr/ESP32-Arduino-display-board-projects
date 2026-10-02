# Circular Matrix rain

Animated code rain for the same ESP32-C3 and 240 x 240 GC9A01 round display
as WatchFace. Uses LovyanGFX; no Wi-Fi, clock service or extra assets required.

Twenty-two independent streams mix bright mint heads, emerald fading trails,
and slower, dimmer background columns. Original 5 x 7 symbols occasionally
change in place. Streams enter and leave along the circular screen boundary,
with a soft edge vignette and a subtly breathing phosphor rim.

Open `Matrix.ino` in Arduino IDE, select **ESP32C3 Dev Module** with
**USB CDC On Boot: Enabled**, and compile/upload to your board's port.
Or, from this directory:

```powershell
.\Build-Matrix.ps1
.\Build-Matrix.ps1 -Upload -Port COM3
```

The script compiles only unless `-Upload` is supplied. Uploading replaces the
currently running sketch on the selected board.

`MatrixConfig.h` controls rotation, frame rate, speed and brightness. The
layout is designed for 240 x 240; changing screen dimensions also requires
adjusting the geometry in `MatrixRain.h`. `BoardDisplay.h` keeps the proven
WatchFace pinout: SCLK 6, MOSI 7, DC 2, CS 10, board enable 3.

Rendering targets 30 FPS with elapsed-time animation and a 115,200-byte RGB565
frame buffer in internal RAM. Allocation falls back to an 8-bit buffer, then
direct drawing if needed (direct drawing may flicker). No PSRAM is required.
Actual frame rate and appearance need confirmation on the physical board.

## Preview and validation

`preview.gif` shows eight seconds of animation. `preview.png` is a 3x enlarged
still. Both are rendered from the actual `MatrixRain.h` drawing code using a
small desktop graphics adapter. To regenerate: `python preview.py` (requires
Pillow and g++). The desktop preview does not measure device performance.

Compiled for `esp32:esp32:esp32c3:CDCOnBoot=cdc`: 375,948 bytes of flash and
16,856 bytes of static RAM, plus the frame buffer allocated at runtime.
The desktop renderer compiled with warnings treated as errors and rendered
120 frames without out-of-bounds pixel writes. Not uploaded or tested on
physical hardware.
