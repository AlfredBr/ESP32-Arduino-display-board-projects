# Breitling Aerospace watch face

A 240 x 240 circular face based on your watch photograph: slate-blue dial,
gold numerals and hands, Patrouille de France details and a brushed-metal bezel.
The lower LCD shows a two-letter weekday and day of month, for example **WE 30**.
The upper LCD stays blank. The hour and minute hands show local time; there is
no seconds hand, matching the reference watch.

Timekeeping follows WatchFaceRTC: Wi-Fi and background NTP synchronization,
Eastern time with automatic daylight-saving changes, and reconnection attempts
every 30 seconds. After synchronization the clock continues during network
outages. A cold start needs a network time sync; until then the lower LCD shows
`-- --` and the hands are hidden. The date changes at local midnight.

`WiFiCredentials.h` was copied from WatchFaceRTC. Edit it if your network changes.
Timezone, NTP servers and rotation are in `BreitlingConfig.h`.

## Build and upload

Run from `C:\Users\alfre\OneDrive\Documents\Arduino\Breitling`:

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM3
```

Uses ESP32C3 Dev Module, USB CDC On Boot enabled, and LovyanGFX. Display wiring
matches WatchFaceRTC: SCLK 6, MOSI 7, DC 2, CS 10, enable GPIO 3. A full RGB565
framebuffer uses 115,200 bytes of runtime RAM, with an 8-bit fallback and direct
drawing if allocation fails. No PSRAM is required.

## Artwork and preview

The static dial was generated in image-edit mode from your photograph. The
unchanged generated source is [assets/dial.png](assets/dial.png); the exact
prompt and provenance are in [assets/PROMPT.md](assets/PROMPT.md).
`prepare_assets.py` crops the black source margin, enlarges the face by 3.6%,
and packs it as `DialTexture.h` (RGB565 in flash). The bezel fills the circular
240-pixel screen to its edge; the live hands and LCD use the same transform.
Hands and LCD segments are drawn dynamically by `BreitlingDrawing.h`.

[preview.png](preview.png) shows Wednesday the 30th at 10:08:30.
[preview-contact.png](preview-contact.png) shows every weekday and the initial
unsynchronized state. `preview.py` compiles the actual C++ renderer using g++
and requires Pillow; asset preparation additionally requires NumPy.

The desktop checks cover weekday mappings, the blank upper LCD before sync,
rendering of dates 01–06, 30 and 31, and hand bounds throughout twelve hours.
ESP32-C3 compilation passed with ESP32 core 3.3.12 and LovyanGFX 1.2.30:
1,165,808 bytes flash (88%) and 39,012 bytes static RAM (11%), plus the runtime
framebuffer. This sketch has not been uploaded; CoinFlip remains on the board.
