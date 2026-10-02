# Rolex GMT-Master watch face

A 240 x 240 circular face based on your watch: red-and-blue Pepsi bezel, black
dial, luminous hour markers, silver Mercedes hour hand, sword minute hand,
sweeping seconds hand, and red 24-hour arrow. The magnified window at 3 o'clock
shows only the day of the month (1–31), with no leading zero or weekday.
The bezel fills the circular display to the edge.

Timekeeping matches WatchFaceRTC: Wi-Fi and background NTP synchronization,
Eastern time with automatic daylight-saving changes, and connection retries
every 30 seconds. After the first sync, time continues during network outages.
The date changes at local midnight. The red arrow indicates the same local time
on the fixed 24-hour bezel. Hands target 25 frames per second for smooth motion.

At a cold start, the date window shows a dash and the hands remain hidden until
network time is available. `WiFiCredentials.h` was copied from WatchFaceRTC;
edit it if your network changes. Timezone, NTP servers and display rotation are
configured in `RolexConfig.h`.

## Build and upload

Run from `C:\Users\alfre\OneDrive\Documents\Arduino\Rolex`:

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM3
```

Uses ESP32C3 Dev Module with USB CDC On Boot enabled, LovyanGFX and the same
GC9A01 display wiring as WatchFaceRTC: SCLK 6, MOSI 7, DC 2, CS 10, enable 3.
The runtime RGB565 framebuffer needs 115,200 bytes of internal RAM. Allocation
falls back to an 8-bit buffer, then direct drawing if necessary; PSRAM is not
required. This sketch has not been uploaded.

Compilation passed with ESP32 core 3.3.12 and LovyanGFX 1.2.30: 1,166,166 bytes
flash (88%) and 39,012 bytes static RAM (11%), plus the runtime framebuffer.

## Artwork and previews

The dial texture was generated in image-edit mode with the built-in image
tool, using your photo. The unchanged result is [assets/dial.png](assets/dial.png).
The exact prompt is saved in [assets/PROMPT.md](assets/PROMPT.md).
`prepare_assets.py` crops the source margin and packs RGB565 data into
`DialTexture.h`. All four hands and the date numerals are drawn at runtime in
`RolexDrawing.h`.

[preview.png](preview.png) shows the 30th at 10:08:32.
[preview.gif](preview.gif) demonstrates the seconds-hand motion, and
[preview-contact.png](preview-contact.png) shows single- and double-digit dates
and the initial unsynchronized state.

The desktop renderer uses the actual C++ drawing code. It exercises all 31 date
values and a full 24 hours of hand positions, checks drawing bounds and verifies
that the unsynchronized state alters only the date dash. The previews were
visually inspected. To regenerate, `preview.py` needs g++ and Pillow, and
`prepare_assets.py` additionally needs NumPy.
