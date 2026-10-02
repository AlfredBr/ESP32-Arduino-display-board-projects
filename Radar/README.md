# Vintage radar

LVGL 9 animation for the workspace's ESP32-C3 and round 240x240 GC9A01 display. A green beam rotates clockwise once every ten seconds, with a dim phosphor trail, four range rings, compass ticks and six simulated aircraft echoes. Contacts light up when crossed by the beam, emit a small expanding halo and fade before the next scan. This is a stylized WWII radar animation, not a working radar or a model of a specific historical instrument.

```powershell
.\Radar\Build.ps1
.\Radar\Build.ps1 -Upload -Port COM3
```

Use your device's actual serial port. Build through Build.ps1 so LVGL receives its configuration flags without modifying the older workspace configuration. Requires the already installed LVGL 9.6 and LovyanGFX libraries. At 115200 baud, send a space to pause or resume.

Adjust sweep speed, fade duration, frame interval and trail length in RadarConfig.h. Contact bearings, distances (fraction of scope radius) and per-scan drift are in the contacts array in Radar.ino.

To learn LVGL, inspect createScope(): it uses circle-shaped base objects, labels and line widgets. animate() changes line points and object opacity; loop() calls lv_timer_handler(). Point arrays remain alive for as long as their line widgets. Partial rendering uses an 18.75 KB RGB565 buffer, with synchronous display transfers.

The sweep targets a 20 ms update/refresh interval (50 FPS when rendering permits). A denser 32-ray trail spans 36 degrees. The slower sweep moves in smaller angular steps; actual frame rate depends on display rendering time.

