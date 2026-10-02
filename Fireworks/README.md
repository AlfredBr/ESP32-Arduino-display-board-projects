# Circular fireworks

Fireworks simulation for the WatchFace hardware: ESP32-C3 with a round
240 x 240 GC9A01 SPI screen and LovyanGFX. No Wi-Fi or additional assets needed.

Rockets rise from the bottom into four alternating effects: chrysanthemums,
two concentric rings, golden willows, and eight-branch palms. Six color palettes,
gravity, air resistance, fading trails, ember twinkles, and a faint star field
give the show depth. Launch targets sit inside the dial and the outer edge is
softly clipped to the circular screen. The show continues indefinitely.

## Build

Open `Fireworks.ino` in Arduino IDE. Select **ESP32C3 Dev Module**, with
**USB CDC On Boot: Enabled**, and your board's port. Or run from this folder:

```powershell
.\Build-Fireworks.ps1
.\Build-Fireworks.ps1 -Upload -Port COM3
```

The default is compile only. Upload replaces the sketch on the selected board.
Pins match WatchFace: SCLK 6, MOSI 7, DC 2, CS 10, board enable 3.

## Adjust the show

`FireworksConfig.h` sets rotation, brightness, target FPS (30 by default), and
launch interval. The geometry is specifically designed for 240 x 240.
`FireworksAnimation.h` contains the palettes, particle physics, and burst styles.

A fixed pool of 256 sparks bounds particle memory; no per-frame allocation is
needed. Physics runs at 60 fixed steps per second with bounded catch-up after
stalls. Rendering uses a 115,200-byte RGB565 sprite in internal RAM, falling
back to an 8-bit sprite or direct drawing if allocation fails. Direct drawing
may flicker. PSRAM is not required.

## Desktop preview

Run `python preview.py` with Pillow and g++ installed. It compiles the actual
animation header against a small desktop pixel renderer and creates
`preview.gif`, `preview.png`, and `preview-contact.png`. This previews the
simulation and circular clipping; it does not measure physical display speed.
Use `--output-dir <folder>` to choose another location for the preview files.

## Validation

Compiled successfully for `esp32:esp32:esp32c3:CDCOnBoot=cdc` with ESP32 core
3.3.12 and LovyanGFX 1.2.30: 407,420 bytes of flash, 44,776 bytes of static RAM
(plus the frame buffer allocated at runtime). The desktop renderer compiled
with warnings treated as errors, generated 180 preview frames, and exercised
ten simulated minutes of particle reuse with circular pixel-bound checks.
Seeded restart was also checked. Preview frames were visually inspected.
Not uploaded or verified on the physical display by Codex.
