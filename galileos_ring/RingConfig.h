#pragma once
#include <stdint.h>

namespace RingConfig {
constexpr int COUNT = 48;               // pendulums
constexpr int GROUP = 16;               // pendulums per symmetric group (3 groups)
constexpr int BASE_OSC = 12;            // swings of the slowest pendulum per cycle
constexpr uint32_t CYCLE_MS = 60000;    // time until all balls realign
constexpr uint32_t FRAME_MS = 20;
constexpr float CENTER = 119.5f;
constexpr float MID_R = 66.0f;          // swing centre radius
constexpr float AMPLITUDE = 38.0f;      // swing radius either side of MID_R
constexpr float RIM_R = 112.0f;         // rim marker radius
constexpr int HUB_R = 17;
constexpr uint16_t RIM_COLOR = 0x4208;
constexpr uint16_t GLOW_COLOR = 0x0A28;
constexpr uint16_t HUB_COLOR = 0xFD79;
}
