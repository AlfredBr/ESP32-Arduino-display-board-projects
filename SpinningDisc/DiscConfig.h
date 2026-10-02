#pragma once
namespace DiscConfig {
constexpr int ROTATION = 0;
constexpr int ENABLE_PIN = 3;
constexpr int FPS = 30;
constexpr float RPM = 2.4f;       // Negative values reverse the direction.
constexpr bool RANDOM_CHANGES = true;
constexpr float MIN_CHANGE_SECONDS = 6.0f;
constexpr float MAX_CHANGE_SECONDS = 10.0f;
constexpr float TRANSITION_SECONDS = 3.5f;
constexpr int ANGULAR_TILES = 18;
constexpr int CROSS_SPIRALS = 4;
}
