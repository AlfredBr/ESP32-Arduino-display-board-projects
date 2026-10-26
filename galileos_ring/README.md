# Galileo's Ring II

Pendulum-wave simulation for the workspace's ESP32-C3 and round 240x240 GC9A01 display, rendered with LovyanGFX. It models the physics demonstration "Galileo's Ring II" as seen from above: 48 pendulums on radial spokes, arranged in three identical groups of 16 for 120-degree symmetry. This is a simulation, not a sensor reading or a physical model.

## How it works

Each ball swings in and out along its own spoke around a pink hub. Pendulum `k` completes `baseOsc + (k % groupSize)` swings per cycle (`baseOsc` = `BASE_OSC`, `groupSize` = pendulum count / groups), so every ball realigns with the others once per cycle (60 s by default). Between those moments, the neighbouring pendulums drift in and out of phase, forming waves with 3-fold symmetry. All balls start at full swing, so the display is perfectly aligned at the start of each cycle.

The phase difference between neighbouring pendulums is `2π·t / T_cycle`. At `T_cycle / 3` it is 120°, which produces the three interlocking waves.

## Build and upload

```powershell
.\galileos_ring\Build.ps1
.\galileos_ring\Build.ps1 -Upload -Port COM4
.\galileos_ring\Build.ps1 -MaxNodes -Upload -Port COM4
.\galileos_ring\Build.ps1 -Trails -Upload -Port COM4
.\galileos_ring\Build.ps1 -MaxNodes -Trails -Upload -Port COM4
```

`-Trails` defines `GALILEO_TRAILS` and draws a fading comet tail behind every ball. It combines freely with `-MaxNodes`. Trails are replayed analytically from earlier phases (no per-ball history), and their length, spacing and brightness are `TRAIL_STEPS`, `TRAIL_STEP_MS` and `TRAIL_BRIGHTNESS` in `RingConfig.h`.

More optional looks, each a switch that combines freely with the others:

| Switch | Define | Effect |
|--------|--------|--------|
| `-Rainbow` | `GALILEO_RAINBOW` | Ball (and trail) hue follows each pendulum's swing phase, so the waves appear as colour bands |
| `-Bloom` | `GALILEO_BLOOM` | Balls whiten and gain a halo at peak speed (mid-swing); tune with `BLOOM_WHITE`, `BLOOM_HALO` |
| `-Cartesian` | `GALILEO_CARTESIAN` | Classic pendulum-wave view: a horizontal row of balls swinging up and down instead of radial spokes (the groups appear as repeats); tune with `CART_AMPLITUDE`, `CART_HALF_WIDTH` |
| `-Mirror` | `GALILEO_MIRROR` | Cartesian view only: the row sits higher and is reflected, dimmed, below a floor line (trails and ribbon too). Implies `-Cartesian` unless `-Auto` is set; tune with the `MIRROR_*` constants |
| `-Wave` | `GALILEO_WAVE` | Draws a ribbon through neighbouring balls: a travelling curve in the row view, a closed loop that morphs between lobed shapes in the ring view; `WAVE_BRIGHTNESS` |
| `-Auto` | `GALILEO_AUTO` | Self-running show: steps through six presets (ring and row views, 2, 3 or 4 groups, different `baseOsc`), one `CYCLE_MS` each, dipping the backlight between them. Edit the `presets` table in `galileos_ring.ino`; `AUTO_*` constants set preset length and fade. Serial `N` skips to the next preset |
| `-Stats` | `GALILEO_STATS` | Prints average/worst draw time and a per-stage breakdown over serial every 10 s |

```powershell
.\galileos_ring\Build.ps1 -MaxNodes -Trails -Rainbow -Bloom -Upload -Port COM4
.\galileos_ring\Build.ps1 -MaxNodes -Rainbow -Cartesian -Mirror -Upload -Port COM4
.\galileos_ring\Build.ps1 -MaxNodes -Trails -Rainbow -Bloom -Mirror -Wave -Auto -Upload -Port COM4
```

The default build has 48 pendulums (3 × 16). `-MaxNodes` defines `GALILEO_MAX_NODES` and builds the densest legible ring: 96 pendulums (3 × 32) with smaller balls and no glow. At the innermost swing radius the spokes are only about 1.8 px apart, so going denser would merge the balls. You can also `#define GALILEO_MAX_NODES` at the top of `RingConfig.h`. The pendulum count must stay divisible by 12 so that 2, 3 and 4 groups all divide it evenly.

Use your device's actual serial port (COM4 on the author's machine; the script's default is COM3). `Build.ps1` points arduino-cli at the workspace's shared `Libraries\` folder for LovyanGFX. Close Serial Monitor before uploading.

At 115200 baud, send a space to pause or resume (and `N` for the next preset in an `-Auto` build).

## Performance

The ESP32-C3 has no FPU, so the per-frame maths is integer only (a cosine lookup table and 16-bit swing angles). A full `-MaxNodes` build with every switch on, including `-Auto`, draws in about 17-19 ms per frame. About 11.5 ms of that is the fixed SPI transfer of the 240x240 sprite, so the 20 ms frame target holds. Use `-Stats` to measure your own combination.

## Files

| File | Purpose |
|------|---------|
| `galileos_ring.ino` | Sketch: geometry setup, per-frame drawing, presets, serial controls |
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
