# Spinning Disc

An animated optical spiral inspired by the supplied orange, gold and teal
reference. Curved shaded tiles become progressively smaller toward a glossy
teal central hub. The artwork fills the round 240 x 240 GC9A01 display.

The pattern starts in the reference palette. After four seconds, it begins
randomly evolving: colors rotate through new palettes, the disc changes speed
and direction, and the curved tiles change shape and scale. Each change eases
over 3.5 seconds, with new targets every 6-10 seconds. The glossy hub remains
stationary. A hardware random seed gives each startup a different sequence.

## Build and upload

Uses the same ESP32-C3, LovyanGFX and pinout as WatchFace: SCLK 6, MOSI 7,
DC 2, CS 10, enable 3. Open `SpinningDisc.ino` with **ESP32C3 Dev Module** and
**USB CDC On Boot: Enabled**, or run from this folder:

```powershell
.\Build-SpinningDisc.ps1
.\Build-SpinningDisc.ps1 -Upload -Port COM3
```

The script compiles only by default. Upload replaces the sketch on the board.

`DiscConfig.h` controls frame rate, base RPM, random changes and transition
timing. Set `RANDOM_CHANGES=false` for constant rotation in the original colors.
Negative RPM reverses the fixed rotation. To change spiral winding counts,
also update and rerun `generate_pattern.py`; compile-time checks enforce matching
geometry and seam continuity.

## Rendering and preview

Polar geometry, shaded tile textures and the glossy center are baked into
`DiscTables.h` in flash. Per-frame work uses table lookups, an integer color
transform, and RGB565 scanlines. No per-pixel logarithms or trigonometry are
needed. A 115,200-byte internal-RAM sprite buffers the output, with direct
scanline drawing as a fallback (which may show tearing). No PSRAM or Wi-Fi is
required. Rendering targets 30 FPS; physical frame rate needs device testing.

`generate_pattern.py` regenerates the tables using NumPy. `preview.py` uses
Pillow and g++ to render the actual C++ animation into a 20-second GIF and
stills. Both accept `--output-dir <folder>` for protected-folder environments.
The GIF demonstrates one seeded sequence and restarts at its loop boundary;
the actual device animation continues indefinitely.

## Validation

Compiled for `esp32:esp32:esp32c3:CDCOnBoot=cdc` with ESP32 core 3.3.12 and
LovyanGFX 1.2.30: 868,044 bytes flash (66%), 15,864 bytes static RAM, plus the
runtime framebuffer. The desktop build passed with warnings treated as errors.
300 preview frames verified visible rotation, changed palette, screen bounds,
and deterministic seeded replay after a ten-minute simulation. Original and
evolved color variants were visually inspected. Not uploaded or physically
tested by Codex.
