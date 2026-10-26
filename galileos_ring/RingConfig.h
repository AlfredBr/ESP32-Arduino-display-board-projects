#pragma once
#include <stdint.h>

namespace RingConfig {
// Define GALILEO_MAX_NODES (build with `Build.ps1 -MaxNodes`) for the densest
// ring that stays legible on the 240 px display. At the innermost swing radius
// (~28 px) 96 spokes sit about 1.8 px apart, so balls shrink to 3 px discs.
#ifdef GALILEO_MAX_NODES
constexpr int COUNT = 96;               // pendulums
constexpr int BALL_R = 1;
constexpr bool BALL_GLOW = false;
#else
constexpr int COUNT = 48;
constexpr int BALL_R = 2;
constexpr bool BALL_GLOW = true;
#endif
constexpr int DEFAULT_GROUPS = 3;       // symmetric groups; Auto also uses 2 and 4
static_assert(COUNT % 12 == 0, "COUNT must divide evenly into 2, 3 and 4 groups");
constexpr int BASE_OSC = 12;            // swings of the slowest pendulum per cycle
constexpr uint32_t CYCLE_MS = 60000;    // time until all balls realign
constexpr uint32_t FRAME_MS = 20;
// Used only when GALILEO_TRAILS is defined (Build.ps1 -Trails).
constexpr int TRAIL_STEPS = 8;              // segments per trail
constexpr uint32_t TRAIL_STEP_MS = 30;      // time covered by each segment
constexpr float TRAIL_BRIGHTNESS = 0.8f;    // newest segment brightness, 0..1
// GALILEO_BLOOM (Build.ps1 -Bloom): peak-speed brightening at mid-swing.
constexpr float BLOOM_WHITE = 0.55f;        // blend toward white at peak speed, 0..1
constexpr float BLOOM_HALO = 0.6f;          // halo brightness at peak speed, 0..1
// GALILEO_CARTESIAN (Build.ps1 -Cartesian): a row of balls swinging up and down.
constexpr float CART_AMPLITUDE = 56.0f;     // swing either side of the centre line, px
constexpr float CART_HALF_WIDTH = 96.0f;    // half the row length, px
// GALILEO_MIRROR (Build.ps1 -Mirror): Cartesian row sits higher, reflected below a floor line.
#ifdef GALILEO_MIRROR
constexpr bool MIRROR = true;
#else
constexpr bool MIRROR = false;
#endif
constexpr float MIRROR_CENTER_Y = 76.0f;    // row centre line
constexpr float MIRROR_AMPLITUDE = 40.0f;
constexpr float MIRROR_HALF_WIDTH = 70.0f;  // narrower to stay inside the round screen
constexpr float MIRROR_FLOOR_Y = 124.0f;
constexpr float MIRROR_FADE = 0.35f;        // reflection brightness, 0..1
constexpr uint16_t FLOOR_COLOR = 0x39E7;
// GALILEO_WAVE (Build.ps1 -Wave): ribbon joining neighbouring balls.
constexpr float WAVE_BRIGHTNESS = 0.55f;    // 0..1
// GALILEO_AUTO (Build.ps1 -Auto): steps through the presets in galileos_ring.ino.
constexpr uint32_t AUTO_CYCLES_PER_PRESET = 1;
constexpr uint32_t AUTO_PRESET_MS = CYCLE_MS * AUTO_CYCLES_PER_PRESET;
constexpr uint32_t AUTO_FADE_MS = 900;      // backlight fade at each end of a preset
constexpr float CENTER = 119.5f;
constexpr float MID_R = 66.0f;          // swing centre radius
constexpr float AMPLITUDE = 38.0f;      // swing radius either side of MID_R
constexpr float RIM_R = 112.0f;         // rim marker radius
constexpr int HUB_R = 17;
constexpr uint16_t RIM_COLOR = 0x4208;
constexpr uint16_t GLOW_COLOR = 0x0A28;
constexpr uint16_t HUB_COLOR = 0xFD79;
}
