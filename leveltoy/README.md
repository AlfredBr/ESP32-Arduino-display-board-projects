# LevelToy

A small tilt-controlled toy for the **Waveshare ESP32-S3-Touch-AMOLED-1.8 V2**.
It reuses the display, touchscreen, and QMI8658 accelerometer setup from
[`../level`](../level), with a four-color triangular board inspired by the
reference image.

Tap the screen to cycle through five modes:

- **Labyrinth:** an 8x8 randomly generated maze with a single winding route.
  Tilt the bead from the start corner to the green goal at the farthest end;
  each goal reached generates a brand-new labyrinth.
- **Pinwheel:** freely drift the bead around the four colored wedges.
- **Center balance:** keep the bead inside the small center ring and build up
  your hold time.
- **Monster dodge:** avoid the little monsters as they walk across the board
  from the top, bottom, left, and right. They get faster the longer you last.
  A collision ends the run with a red flash, shockwave ring, bead shake and a
  speaker crash sound; tap to try again.
- **Tunnel swim:** steer through a scrolling tunnel whose left and right walls
  wander in changing waves. Avoid the walls and the top and bottom edges.
  The water slowly speeds up the longer you last. Collect occasional blue
  diamonds, which appear at random spots in the water; missed diamonds disappear and make the
  bead slightly larger and play a sad wah-wah. Catching a diamond plays a bright chime; hitting a wall,
  the top or the bottom plays a scary shriek. Tap after a crash to retry.

The board should be held face-up. If touch is unavailable, send `m` over Serial
at 115200 baud to switch modes. Send `s` to toggle a stream of accelerometer
and bead-position data.

## Run

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM5
.\Test.ps1
```

`Build.ps1` supports `-Revision original` for the SH8601 panel and `-DryRun` to
print the Arduino CLI command. It uses LVGL 9.6 from `..\Libraries\lvgl`, plus
GFX_Library_for_Arduino, Adafruit_XCA9554, Adafruit_BusIO, Mylibrary, and
SensorLib from `..\WaveshareHelloWorld\vendor\<revision>\libraries`.

The host tests cover the shared level math used to map the board's accelerometer
axes and smooth the tilt signal.
