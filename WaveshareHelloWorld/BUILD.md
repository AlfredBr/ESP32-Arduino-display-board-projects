## Current feature app verification (2026-10-09)

- V2 build passed: 1,489,463 bytes application flash (47%), 54,076 bytes static RAM (16%).
- Uploaded to the ESP32-S3 on COM5; esptool verified flash hashes.
- On-device diagnostics: display, touch, IMU, RTC and PMU initialized successfully.
- Wi-Fi connected; NTP synchronized; WTXT HTTPS request returned Updated.
- Motion readings changed when the device moved; microphone DMA supplied nonzero sample data.
- Native SwipeGesture tests passed: left/right, one event per contact, suppression
  of taps after swipes, release debounce, vertical drag, long press and millis rollover.
- Speaker audibility, microphone recording/playback quality, BLE phone connection,
  SD round-trip, battery/button behavior and offline RTC retention remain physical
  checks using the feature pages. No recording was made during automated checks.

# Build and upload

Run `Build-HelloScreen.ps1` in this folder. Defaults: ESP32-S3 Dev Module,
USB CDC On Boot enabled, 16 MB flash, OPI PSRAM, 3 MB application / FAT partition,
COM5, V2 board revision. The existing script name is preserved.

```powershell
.\Build-HelloScreen.ps1
.\Build-HelloScreen.ps1 -Revision v2
.\Build-HelloScreen.ps1 -Revision v2 -Upload -Port COM5
.\Build-HelloScreen.ps1 -Revision v2 -Upload -Port COM5 -DryRun
```

`-Upload` compiles before flashing. Close Serial Monitor if COM5 is busy.
`-DryRun` only prints the command. The script reuses Arduino IDE's CLI and YAML
configuration. The revision chooses both the library set and panel constructor.

Compile an upstream example with the same board settings and local libraries:

```powershell
.\Build-HelloScreen.ps1 -SketchPath .\vendor\v2\examples\15_ES8311
.\Build-HelloScreen.ps1 -Revision v2 -SketchPath .\vendor\v2\examples\01_HelloWorld
```

Optional parameters: `-SketchPath`, `-CliPath`, `-ConfigFile`, `-Fqbn`, `-Port`,
`-Revision`. Sensor diagnostics in this project's sketch use 921600 baud;
upstream examples usually use 115200 baud and some instantiate HWCDC directly.

The microphone/speaker example plays a bundled clip then echoes microphone
samples to the speaker. Its upstream default volume is 85%; lower
EXAMPLE_VOICE_VOLUME before testing if desired. It is not part of HelloScreen.

For a bare Arduino IDE configuration choose ESP32S3 Dev Module, Flash Size 16MB,
PSRAM OPI PSRAM, USB CDC On Boot Enabled and the current port. The script is the
configured build route for this project because it resolves the vendor libraries
and revision macro without changing the shared sketchbook library installation.



## Earlier hello-world baseline (2026-10-09)

- Converted hello/sensor sketch, original revision: compile passed, 470080 bytes flash.
- Converted hello/sensor sketch, V2 revision (default): compile passed, 470092 bytes flash,
  29664 bytes static RAM.
- Manufacturer original ES8311 microphone/speaker echo example: compile passed,
  996748 bytes flash. V2 audio example and remaining upstream UI examples were not
  separately compiled in this session.
- Hardware display, sensor readings, audio, buttons and networking still require
  on-device testing. Nothing was uploaded during this conversion.

