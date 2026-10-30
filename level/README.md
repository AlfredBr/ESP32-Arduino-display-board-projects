# level

A spirit level for the **Waveshare ESP32-S3-Touch-AMOLED-1.8 V2** (368 x 448
CO5300 AMOLED, CST816T touch, QMI8658 IMU), drawn with LVGL 9.6. It uses the
same board setup as PieMenuS3.

- **Lying flat:** a bullseye. The bubble floats to the high side. Rings mark
  where the bubble's edge sits at 1, 3 and 6 degrees. The big number is the
  total tilt. The line below it shows X (positive when the right side is high)
  and Y (positive when the top is high).
- **Standing on the top or bottom edge:** a horizontal vial. The bubble sits
  between the two marks when level. The bottom line shows the slope as a
  percentage.
- **Standing on a long edge:** a vertical vial on the right, with the readout
  on the left.

The view switches automatically. Hysteresis keeps it from flickering near
40 degrees from flat and near the diagonal between the two edges. The readout
and bubble turn green within 0.25 degrees of level.

## Zeroing

The sensor and the case are never perfectly square. On a known-level surface,
**tap** the screen and hold still: the level zeroes about a second later, once
the tap's wobble has settled. Each orientation keeps its own zero: flat, each
short edge, and each long edge. Zeros are stored in flash (Preferences
namespace `level`) and survive power cycles. **Long-press** to clear every
zero.

## Run

```powershell
.\Build.ps1
.\Build.ps1 -Upload -Port COM5
.\Check-Device.ps1
.\Test.ps1
```

`Build.ps1` options are the same as PieMenuS3's: `-Revision original` for the
SH8601 panel, `-DryRun` to print the command only. Dependencies: LVGL 9.6 from
`..\Libraries\lvgl`, plus GFX_Library_for_Arduino, Adafruit_XCA9554,
Adafruit_BusIO, Mylibrary and SensorLib from
`..\WaveshareHelloWorld\vendor\<revision>\libraries`.

`Check-Device.ps1` resets the board, confirms that the IMU answered, and prints
a second of readings. `Test.ps1` builds and runs the desktop tests for
`LevelMath.h` with MSYS2 MinGW g++. They cover axis mapping, angles, view
selection and hysteresis, zeroing, bubble scaling, and smoothing.

Serial commands at 115200 baud: `s` toggles a 5 Hz stream of raw acceleration,
the smoothed screen-frame up vector, and the reading; `z` zeroes; `c` clears
the zeros.

## How it works

- The QMI8658 at 0x6B shares the I2C bus with touch. It runs at a 2 g range
  and 250 Hz with its built-in low-pass filter. Samples far from 1 g, such as
  start-up garbage and hard knocks, are dropped.
- `toScreen()` in `LevelMath.h` converts the sensor axes to the screen frame
  (x right, y down, z out of the glass). On this board, sensor +x points to
  the screen top and +z points into the glass: lying face up, it reads
  z = -1 g. If the bubble ever moves toward the low side, this one line is
  what to fix.
- Two time-based exponential filters smooth the reading: 0.12 s for the
  bubble and 0.5 s for the numbers, which update eight times a second.
- Degrees map to pixels through `tanh`: about 15 px per degree near level on
  the bullseye, easing into the wall instead of hitting a hard stop.
- Only the bubble moves, so each frame redraws just its old and new area.

## Board notes

Before zeroing, the development board read about 2.3 degrees lying on a desk,
and 1.15 g rather than 1 g. Part of the 2.3 degrees may be the desk. The rest
is the QMI8658's normal offset and gain error. Angles come from ratios between
axes, so the gain error cancels out, and zeroing removes the offset.
