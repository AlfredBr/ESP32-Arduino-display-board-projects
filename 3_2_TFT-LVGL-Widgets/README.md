# LVGL widgets sample

Compile from the Arduino workspace:

```powershell
./3_2_TFT-LVGL-Widgets/Build.ps1
```

The script targets ESP32-C3 with USB CDC enabled and the 3 MB Huge APP
partition (4 MB flash, no OTA). Firmware artifacts are copied to `build/`.
Compilation does not upload.

The sample requires LVGL 8 APIs. Its isolated dependency is LVGL 8.3.3 under
`.dependencies/lvgl-8.3.3`. To restore it, extract
https://github.com/lvgl/lvgl/archive/refs/tags/v8.3.3.zip into `.dependencies`.
The script copies the demo sources into the library's `src/demos` so Arduino
compiles them, and generates a configuration from the supplied `lv_conf.h`
with the unused legacy FatFs adapter disabled for ESP32 core 3.x.
