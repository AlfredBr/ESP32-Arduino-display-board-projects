# Hamster wheel

A realistic golden hamster runs in a wheel whose 238-pixel outer diameter
fills the 240 x 240 circular GC9A01 display. Detailed transparent fur sprites
animate a running gait, a mischievous glance and a quick wink, followed by an
enthusiastic sprint. Wheel rotation accelerates with the running pace. Spokes
and an axle remain behind the hamster; the bottom contact shadow grounds it.

Hardware and pins match WatchFace: ESP32-C3, SCLK 6, MOSI 7, DC 2, CS 10,
board enable 3. Requires LovyanGFX, with no Wi-Fi or PSRAM.

## Build

Open `Hamster.ino` in Arduino IDE, select **ESP32C3 Dev Module**, enable
**USB CDC On Boot**, and select the board's port. Or run from this directory:

```powershell
.\Build-Hamster.ps1
.\Build-Hamster.ps1 -Upload -Port COM3
```

The default is compile only. Upload replaces the sketch on the board.

`HamsterConfig.h` controls rotation, target FPS and running cadence.
`HamsterAnimation.h` controls the wheel and behavior. The layout is designed
specifically for this 240 x 240 display. Rendering targets 30 FPS; actual
performance requires checking on the physical board.

## Artwork and memory

`assets/hamster-sheet.png` is the original AI-generated transparent sprite sheet.
The built-in image-generation tool produced the artwork; its prompt is recorded
in `assets/PROMPT.md`. `prepare_assets.py` registers the frames to a shared scale
and baseline and packs their RGB565 pixels and alpha into `HamsterAssets.h`.
The source PNG remains unchanged. NumPy and Pillow are needed only to regenerate
assets. Alpha is preserved to keep fine fur edges soft.

Texture data stays in flash. A 115,200-byte RGB565 framebuffer is allocated in
internal RAM, with an 8-bit fallback. A readable framebuffer is required for
alpha compositing because the LCD has no MISO connection. If both allocations
fail, the sketch displays a memory message instead of attempting panel readback.

## Preview

`preview.py` compiles the actual C++ animation with a desktop graphics adapter
and generates `preview.png`, `preview-contact.png` and `preview.gif`. Requires
Pillow and g++. Both Python scripts accept `--output-dir <folder>` to support
systems with Controlled Folder Access. The preview exercises the running,
glance and wink states, checks drawing bounds and simulates ten minutes.

## Validation

Compiled successfully for `esp32:esp32:esp32c3:CDCOnBoot=cdc`, using ESP32 core
3.3.12 and LovyanGFX 1.2.30: 1,032,020 bytes flash (78%) and 16,288 bytes static
RAM, plus the framebuffer allocated at runtime. The desktop renderer compiled
with warnings treated as errors and generated 150 preview frames; run, glance,
wink, drawing bounds and a ten-minute simulation passed. Preview frames were
visually inspected. This sketch has not yet been uploaded by Codex.
