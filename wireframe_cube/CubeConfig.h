#pragma once
#include <stdint.h>

// Set to 0 to show all wireframe edges, or 1 to hide rear edges.
#ifndef HIDDEN_LINE_REMOVAL
#define HIDDEN_LINE_REMOVAL 1
#endif

namespace CubeConfig {
constexpr int WIDTH = 288;
constexpr int HEIGHT = 288;
constexpr int FPS = 60;
constexpr float SHAPE_SECONDS = 20.0f;
constexpr uint8_t BRIGHTNESS = 160;
constexpr float CAMERA_DISTANCE = 5.5f;
constexpr float FOCAL_LENGTH = 300.0f;
constexpr float SPEED_X = 0.43f;
constexpr float SPEED_Y = 0.71f;
constexpr float SPEED_Z = 0.19f;
}
