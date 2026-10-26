# Galileo's Ring II

Pendulum-wave simulation for the workspace's ESP32-C3 and round 240x240 GC9A01 display, rendered with LovyanGFX. It models the physics demonstration "Galileo's Ring II" as seen from above: 48 pendulums on radial spokes, arranged in three identical groups of 16 for 120-degree symmetry. This is a simulation, not a sensor reading or a physical model.

## How it works

Each ball swings in and out along its own spoke around a pink hub. Pendulum `k` completes `BASE_OSC + (k % 16)` swings per cycle, so every ball realigns with the others once per cycle (60 s by default). Between those moments, the neighbouring pendulums drift in and out of phase, forming waves with 3-fold symmetry. All balls start at full swing, so the display is perfectly aligned at the start of each cycle.

For a pendulum count `n`, the phase difference between neighbours is `2π·t / T_cycle`. At `T_cycle / 3` it is 120°, which produces the three interlocking waves.

## Build and upload

```powershell
.\galileos_ring\Build.ps1
.\galileos_ring\Build.ps1 -Upload -Port COM4
.\galileos_ring\Build.ps1 -MaxNodes -Upload -Port COM4
```

The default build has 48 pendulums (3 × 16). `-MaxNodes` defines `GALILEO_MAX_NODES` and builds the densest legible ring: 96 pendulums (3 × 32) with smaller balls and no glow. At the innermost swing radius the spokes are only about 1.8 px apart, so going denser would merge the balls. You can also `#define GALILEO_MAX_NODES` at the top of `RingConfig.h`.

Use your device's actual serial port (COM4 on the author's machine; the script's default is COM3). `Build.ps1` points arduino-cli at the workspace's shared `Libraries\` folder for LovyanGFX. Close Serial Monitor before uploading.

At 115200 baud, send a space to pause or resume.

## Files

| File | Purpose |
|------|---------|
| `galileos_ring.ino` | Sketch: geometry setup, per-frame drawing, serial pause |
| `RingConfig.h` | Tunable constants |
| `BoardDisplay.h` | GC9A01 pin and panel configuration (same as Radar, Matrix, etc.) |
| `Build.ps1` | Compile and optional upload script |

## Tuning

Edit [RingConfig.h](./RingConfig.h):

- `CYCLE_MS`: time until all balls realign.
- `BASE_OSC`: swings of the slowest pendulum per cycle. Higher values give faster motion and tighter patterns.
- `MID_R`, `AMPLITUDE`: swing centre and extent, in pixels from the display centre.
- `RIM_R`, `HUB_R`: rim marker radius and hub size.
- `FRAME_MS`: frame interval (20 ms targets 50 FPS; actual rate depends on display transfer time).
- Colours: `RIM_COLOR`, `GLOW_COLOR`, `HUB_COLOR` (RGB565).

Rendering draws the full scene into a 240x240 RGB565 sprite (about 112 KB) and pushes it in one transfer each frame.
