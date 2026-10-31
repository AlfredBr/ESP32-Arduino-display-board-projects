#pragma once
#include <math.h>
#include <stdint.h>

// Pure level math, shared by the sketch and the desktop tests.
// Screen frame: x right, y down, z out of the glass. "Up" is the
// accelerometer's reading at rest, which points away from the ground.
namespace level {

struct Vec3 { float x, y, z; };

// QMI8658 axes on the AMOLED 1.8: +x points to the screen top (the vendor
// demo shows portrait upright when acc.x > 0.8), and the board reads
// z = -1 g lying face up, so +z points into the glass and the right-handed
// +y points to the screen's right.
inline Vec3 toScreen(Vec3 acc) { return {acc.y, -acc.x, -acc.z}; }

// Rejects start-up garbage and hard knocks; at rest the reading is near 1 g.
inline bool plausible(Vec3 acc) {
  float g2 = acc.x * acc.x + acc.y * acc.y + acc.z * acc.z;
  return g2 > 0.5f * 0.5f && g2 < 1.6f * 1.6f;
}

// Flat: lying face up, a bullseye. Portrait: standing on the top or bottom
// edge, a horizontal tube. Landscape: standing on a long edge, a vertical tube.
enum class Mode : uint8_t { Flat, Portrait, Landscape };

// Calibration slots: flat, portrait upright/inverted, landscape on each long edge.
constexpr int kSlots = 5;

struct Reading {
  Mode mode;
  int slot;
  float x, y;  // Bubble offset in degrees along screen x and y; high side positive.
  float tilt;  // Total tilt in degrees.
};

constexpr float kDegrees = 57.2957795f;

inline Vec3 normalized(Vec3 v) {
  float n = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  if (n < 1e-6f) return {0, 0, 1};
  return {v.x / n, v.y / n, v.z / n};
}

// Hysteresis keeps the view from flickering near the 40-degree boundary and
// near the diagonal between the two edge orientations.
inline Mode chooseMode(Vec3 up, Mode previous) {
  Vec3 u = normalized(up);
  float z = fabsf(u.z), x = fabsf(u.x), y = fabsf(u.y);
  if (previous == Mode::Flat ? z > 0.70f : z > 0.80f) return Mode::Flat;
  if (previous == Mode::Portrait) return x > y + 0.15f ? Mode::Landscape : Mode::Portrait;
  if (previous == Mode::Landscape) return y > x + 0.15f ? Mode::Portrait : Mode::Landscape;
  return y >= x ? Mode::Portrait : Mode::Landscape;
}

inline Reading measure(Vec3 up, Mode mode) {
  Reading r{mode, 0, 0, 0, 0};
  switch (mode) {
  case Mode::Flat:
    r.x = atan2f(up.x, fabsf(up.z)) * kDegrees;
    r.y = atan2f(up.y, fabsf(up.z)) * kDegrees;
    r.tilt = atan2f(sqrtf(up.x * up.x + up.y * up.y), fabsf(up.z)) * kDegrees;
    break;
  case Mode::Portrait:
    r.slot = up.y < 0 ? 1 : 2;
    r.x = atan2f(up.x, fabsf(up.y)) * kDegrees;
    r.tilt = fabsf(r.x);
    break;
  case Mode::Landscape:
    r.slot = up.x > 0 ? 3 : 4;
    r.y = atan2f(up.y, fabsf(up.x)) * kDegrees;
    r.tilt = fabsf(r.y);
    break;
  }
  return r;
}

// Subtracts a stored zero (degrees) from a reading.
inline Reading applyZero(Reading r, float zeroX, float zeroY) {
  r.x -= zeroX;
  r.y -= zeroY;
  r.tilt = r.mode == Mode::Flat ? sqrtf(r.x * r.x + r.y * r.y) : fabsf(r.x + r.y);
  return r;
}

// Degrees to pixels: linear near level, easing into the vial wall.
inline float bubblePixels(float degrees, float range, float fullScale = 8.0f) {
  return range * tanhf(degrees / fullScale);
}

// Time-based exponential smoothing, so the bubble's lag doesn't depend on frame rate.
inline void smooth(Vec3 &state, Vec3 sample, float dtSeconds, float tau = 0.12f) {
  float k = 1.0f - expf(-dtSeconds / tau);
  state.x += (sample.x - state.x) * k;
  state.y += (sample.y - state.y) * k;
  state.z += (sample.z - state.z) * k;
}

}  // namespace level
