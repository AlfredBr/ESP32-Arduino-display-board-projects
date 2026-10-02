# Gold Coin Flip

Tap anywhere to toss a realistic gold coin. It rises, turns end over end,
shows its ridged edge, then settles on **HEADS** or **TAILS**. A held finger
counts once. A fresh tap during an animation starts a new toss immediately.
The outcome uses one bit from `esp_random()`; animation does not choose it.

## Build and deploy

For the same ESP32-C3 / GC9A01 round display as WatchFace, with LovyanGFX:

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM3
```

Board: **ESP32C3 Dev Module**, **USB CDC On Boot: Enabled**. Display pins are
SCLK 6, MOSI 7, DC 2, CS 10, enable 3. The CST816D touchscreen uses the supplied
factory example's pins: SDA 4, SCL 5, reset 1, interrupt 0. Polling uses bounded
I2C transfers, disables touch sleep, and requires a stable release before another
press. Missing touch hardware displays a message and retries once per second.

The two coin faces were created with the built-in image-generation tool.
The unchanged source is `assets/gold-coin.png`; the prompt is in
`assets/PROMPT.md`. `prepare_assets.py` packs resized RGBA textures into flash.
It needs NumPy and Pillow only when regenerating `CoinAssets.h`.

Rendering targets 30 FPS and uses a 115,200-byte RGB565 sprite in internal RAM.
If allocation fails, scanlines render directly. No PSRAM or Wi-Fi is required.
The USB serial command `?` at 921600 baud reports touch and framebuffer status.
It does not trigger a flip.

## Validation

Compiled with ESP32 core 3.3.12 and LovyanGFX 1.2.30: 586,944 bytes flash,
16,512 bytes static RAM, plus the runtime framebuffer. Both outcomes, restart
during a flip, held-touch suppression, release rearming and I2C recovery were
checked in the desktop harness. Preview frames were visually inspected.
Uploaded to COM3 successfully; esptool verified flash data and reset the board.
The board's serial diagnostic reports `TOUCH=READY FRAME=RGB565 STATE=IDLE`.
Physical touch interaction has not been observed by Codex.
