#pragma once
#include <stdint.h>

namespace RingConfig {
// Define GALILEO_MAX_NODES (build with `Build.ps1 -MaxNodes`) for the densest
// ring that stays legible on the 240 px display. At the innermost swing radius
// (~28 px) 96 spokes sit about 1.8 px apart, so balls shrink to 3 px discs.
#ifdef GALILEO_MAX_NODES
constexpr int GROUP = 32;               // pendulums per symmetric group (3 groups)
constexpr int BALL_R = 1;
constexpr bool BALL_GLOW = false;
#else
constexpr int GROUP = 16;
constexpr int BALL_R = 2;
constexpr bool BALL_GLOW = true;
#endif
constexpr int COUNT = GROUP * 3;        // pendulums
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
constexpr float CENTER = 119.5f;
constexpr float MID_R = 66.0f;          // swing centre radius
constexpr float AMPLITUDE = 38.0f;      // swing radius either side of MID_R
constexpr float RIM_R = 112.0f;         // rim marker radius
constexpr int HUB_R = 17;
constexpr uint16_t RIM_COLOR = 0x4208;
constexpr uint16_t GLOW_COLOR = 0x0A28;
constexpr uint16_t HUB_COLOR = 0xFD79;
}
