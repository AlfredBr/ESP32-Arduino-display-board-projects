# Factory sample build

From the Arduino workspace, run:

```powershell
./1_1_Factory_samples/Build.ps1
```

This compiles for ESP32-C3 with USB CDC enabled and the 3 MB Huge APP partition
(4 MB flash, no OTA). Firmware files are written to `build/`. Compilation does
not upload to the board.

The build uses project-local LVGL 8.3.3, because the generated UI uses LVGL 8
APIs. To restore this dependency on another computer, download
https://github.com/lvgl/lvgl/archive/refs/tags/v8.3.3.zip and extract it so that
`.dependencies/lvgl-8.3.3/library.properties` exists. The script generates its
LVGL configuration from `LVGL configuration file/lv_conf.h`, disabling the
unused FatFs adapter for compatibility with ESP32 Arduino core 3.x.

The display uses two 60-row buffers to fit ESP32-C3 internal RAM. The supplied
UI and 240 by 240 resolution are preserved.
