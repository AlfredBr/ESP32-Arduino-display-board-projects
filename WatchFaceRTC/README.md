# WatchFace: internet clock

A watch face for the ESP32-C3 and GC9A01 240 x 240 screen. It keeps the design
from the reference photo: purple ticks, large 24-hour digits, date, white
seconds dot, and sample heart-rate display.

## First-time Wi-Fi setup

1. Open WiFiCredentials.h in this project.
2. Replace YOUR_WIFI_NAME and YOUR_WIFI_PASSWORD with your Wi-Fi details.
   The ESP32-C3 needs a 2.4 GHz network. An open network uses an empty password.
3. Save the file. These are C++ strings: escape any literal backslash as `\\`
   and double quote as `\"` inside a value.
4. Compile and upload using the commands below.

WiFiCredentials.h is kept separate and listed in .gitignore. Do not include it
when sharing the project. You do not need to send the password through chat.

The timezone defaults to America/New_York (Eastern Time). In WatchFaceConfig.h,
TIME_ZONE is `EST5EDT,M3.2.0/2,M11.1.0/2`, which automatically handles US daylight
saving transitions. Change this POSIX TZ rule if you want another timezone.
SCREEN_ROTATION and SAMPLE_HEART_RATE are in the same configuration file.

## How the clock gets real time

The ESP32 uses its built-in NTP client, rather than an HTTP web API. NTP is
specifically intended for clock synchronization and does not require an API
key, JSON library, or TLS certificate setup. The servers are pool.ntp.org and
time.nist.gov; their names can be changed in WatchFaceConfig.h.

After Wi-Fi connects, the clock synchronizes in the background and continues
resynchronizing periodically. It displays local time with localtime_r(), so
the date and daylight saving offset change automatically. The format remains
00:00 through 23:59. The seconds indicator is a moving white dot.

Before initial synchronization, the display shows --:-- instead of an invented
time. Status messages indicate what is needed:

- Set Wi-Fi details: configure WiFiCredentials.h and upload again.
- Connecting Wi-Fi: waiting for your network; retries continue in background.
- Waiting for NTP: Wi-Fi is connected but internet time has not arrived yet.
- Offline: time was set previously and is continuing from the internal clock.

After a loss of power, it needs Wi-Fi again to obtain time. While running,
Wi-Fi interruptions do not stop the clock; it reconnects and resynchronizes.
If Waiting for NTP persists, verify internet access and whether your network
permits outbound UDP port 123. No inbound port-forwarding setup is needed.

The displayed 57 and heart remain sample data, not a health measurement.

## Compile and upload

Save edits and close Arduino Serial Monitor before an external upload.

```powershell
Set-Location 'C:\Users\alfre\Downloads\WatchFace'
.\Build-WatchFace.ps1                       # Compile only
.\Build-WatchFace.ps1 -Upload -Port COM3    # Compile and flash
.\Build-WatchFace.ps1 -Upload -DryRun       # Print the command only
```

If PowerShell script execution is disabled:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\alfre\Downloads\WatchFace\Build-WatchFace.ps1' -Upload -Port COM3
```

Or open WatchFace.ino in Arduino IDE. Select ESP32C3 Dev Module, USB CDC On Boot
Enabled, and the board's current port, then Upload. Serial Monitor is optional
and uses 921600 baud. No new libraries need to be installed.

Exact compile command:

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn 'esp32:esp32:esp32c3:CDCOnBoot=cdc' --config-file 'C:\Users\alfre\.arduinoIDE\arduino-cli.yaml' 'C:\Users\alfre\Downloads\WatchFace'
```

Add `--upload --port COM3` before the sketch path to compile and upload.
Uploading replaces the program on the board. HelloScreen's source files remain
in their separate folder and can be uploaded again later.

## Project files

- WatchFace.ino: Wi-Fi, NTP initialization, system-clock reads, update loop.
- WatchFaceDrawing.h: layout, fonts, ticks, time, date, and status text.
- WatchFaceConfig.h: timezone, NTP servers, sample heart rate, rotation.
- WiFiCredentials.h: your local Wi-Fi configuration.
- BoardDisplay.h: working SPI pin / GC9A01 settings from the factory examples.
- Build-WatchFace.ps1: compile/upload script; defaults to compile only.
- preview.png / preview-240.png: static reference-layout previews showing
  Sep 29 at 21:44; they are not live screenshots of the internet clock.
- preview.py: optional PC preview generator using Pillow and LovyanGFX glyphs.

The sketch tries a 115,200-byte 16-bit frame buffer in internal RAM. If needed,
it falls back to a 57,600-byte 8-bit buffer, then direct drawing. NTP and Wi-Fi
connection retries do not use waiting loops. Serial logging uses a zero write
timeout so a closed Serial Monitor does not cause the earlier 3-second pauses.

## Reference

Espressif's [System Time documentation](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-reference/system/system_time.html)
describes background SNTP synchronization and timezone/daylight-saving conversion.
The implementation also follows the SimpleTime example installed with ESP32
Arduino core 3.3.12, using configTzTime() for timezone-aware synchronization.

## Validation

The internet-clock version compiles successfully for ESP32-C3 with USB CDC
enabled: 618,788 bytes flash (47%) and 31,732 bytes static RAM (9%).
Wi-Fi/NTP synchronization has not yet been tested on the physical board;
enter the local network credentials, upload, and check the displayed time.

