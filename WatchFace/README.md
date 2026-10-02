# WatchFace

A separate ESP32-C3 / GC9A01 240 x 240 Arduino project inspired by your watch
photo. It uses the same LovyanGFX display configuration as HelloScreen.

## Appearance and behavior

- Black dial with 60 purple perimeter ticks and a pale upper-left segment.
- Large native-resolution DejaVu time digits in a pale lavender-white.
- Date underneath; sample 57 heart-rate readout and heart above.
- White marker moves around the ring once per second.
- Starts at Sep 29, 2026, 21:44:06 on each reset; hours and date advance normally.

This is reference-photo mode, not the current local time. The 57 is demonstration
data, not a health measurement. No heart-rate sensor, Wi-Fi credentials, or
network clock is configured. Edit WatchFaceConfig.h to change the starting
calendar/time, sample heart rate, and screen rotation.

## Compile and upload

Save all edits, then close Serial Monitor before uploading from PowerShell.

```powershell
Set-Location 'C:\Users\alfre\Downloads\WatchFace'
.\Build-WatchFace.ps1                       # Compile only
.\Build-WatchFace.ps1 -Upload -Port COM3    # Compile and flash
.\Build-WatchFace.ps1 -Upload -DryRun       # Print the command only
```

If script execution is disabled:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\alfre\Downloads\WatchFace\Build-WatchFace.ps1' -Upload -Port COM3
```

Or open WatchFace.ino in Arduino IDE. Select ESP32C3 Dev Module, USB CDC On Boot
Enabled, and the board's current port. Upload. Optional Serial Monitor uses
921600 baud; it is not needed for the clock to run.

Exact compile command:

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn 'esp32:esp32:esp32c3:CDCOnBoot=cdc' --config-file 'C:\Users\alfre\.arduinoIDE\arduino-cli.yaml' 'C:\Users\alfre\Downloads\WatchFace'
```

Add `--upload --port COM3` before the sketch path to compile and upload.
Uploading this project replaces the program on the board. HelloScreen's source
files remain in their original folder and can be uploaded again later.

## Files

- WatchFace.ino: display setup, reference clock, nonblocking update loop.
- WatchFaceDrawing.h: dial layout, colors, fonts, and drawing commands.
- WatchFaceConfig.h: reference time, sample value, rotation.
- BoardDisplay.h: existing working SPI pin / GC9A01 configuration.
- Build-WatchFace.ps1: build/upload wrapper; defaults to compile only.
- preview.png / preview-240.png: PC layout previews, not photos of the board.
- preview.py: optional PC preview generator using Pillow and the installed
  LovyanGFX font bitmaps. It is not needed to build or upload.

A 16-bit frame buffer uses 115,200 bytes of internal RAM and avoids showing
individual drawing steps. If allocation fails, the sketch draws directly to
the display. A 64-bit monotonic timer drives updates, and optional serial
logging never waits for the host, avoiding the earlier three-second pauses.
The display approximates the watch face; it does not draw the physical bezel.

## Validation

Compiled successfully with ESP32 core 3.3.12 and LovyanGFX 1.2.30:
418,644 bytes flash; 15,952 bytes static RAM (the frame buffer is allocated at
runtime). Upload arguments checked in dry-run mode. Preview visually inspected.
Not yet uploaded or verified on the physical display.

