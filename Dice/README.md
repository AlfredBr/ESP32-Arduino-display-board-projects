# Dice and gold coin

A touchscreen menu for all seven standard DnD dice plus the realistic gold coin.
Two pages show four choices each. Tap a choice, then tap the die to roll. Tap
again to reroll; use **< MENU** to choose something else. Holding a finger does
not repeat rolls. A new tap during a roll starts a fresh one.

Choices: **D4 (1-4), D6 (1-6), D8 (1-8), D10 (0-9), D12 (1-12), D20 (1-20),
D% (1-100), COIN (heads/tails)**. D10 follows the 0-9 face labeling in your
reference. Percentile mode rolls independent tens and ones dice: `00` + `0`
means 100; all other combinations give 1-99. Hardware random values use rejection
sampling for unbiased ranges. The animation settles on the selected result face.

## Build

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM3
```

ESP32-C3 / GC9A01 hardware and CST816D touchscreen match CoinFlip and the factory
example. Display pins: SCLK 6, MOSI 7, DC 2, CS 10, enable 3. Touch: SDA 4,
SCL 5, reset 1, interrupt 0. Requires LovyanGFX. Select **ESP32C3 Dev Module**,
**USB CDC On Boot: Enabled**. Upload replaces the current board program.

`generate_meshes.py` creates true tetrahedron, cube, octahedron, pentagonal
trapezohedron, dodecahedron and icosahedron meshes, verifying all face counts.
Dice rotate in 3D with shaded faces, outlines and face labels. The coin shares
CoinFlip's generated gold artwork; its prompt and unchanged source are in
`assets`. The app and each roll use fixed memory and no network connection.

The default frame target is 30 FPS with an internal RGB565 framebuffer and an
8-bit fallback. `CoinConfig.h` contains display and touch configuration.

## Validation

Compiled with ESP32 core 3.3.12 and LovyanGFX 1.2.30: 596,630 bytes flash and
16,544 bytes static RAM, plus the runtime framebuffer. The desktop harness
verified both menu pages, all eight choices and their maximum outcomes, the
percentile `00 + 0 = 100` case, face selection and rendering bounds. Menu and
result previews were visually inspected. `preview.py` recreates the contact
sheet and animated preview using Pillow and g++. This separate Dice sketch
has not been uploaded; CoinFlip remains installed on the board.
