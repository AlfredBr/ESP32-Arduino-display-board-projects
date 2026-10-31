// Desktop tests for the shared LevelMath.h. Run with ..\Test.ps1.
#include "../LevelMath.h"
#include <cstdio>

using namespace level;
static int failures = 0;

static void check(bool ok, const char *what) {
  if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}
static bool near(float a, float b, float eps = 0.05f) { return fabsf(a - b) < eps; }
static Vec3 tilted(float degX, float degY) {
  // Flat up-vector tilted toward screen +x and +y by the given angles.
  return normalized({tanf(degX / kDegrees), tanf(degY / kDegrees), 1});
}

int main() {
  // Axis mapping: acc.x > 0 is portrait upright, so up is toward the screen top (-y).
  Vec3 upright = toScreen({1, 0, 0});
  check(near(upright.x, 0) && near(upright.y, -1), "acc +x maps to screen up");
  check(near(toScreen({0, 0, -1}).z, 1), "face up (acc z = -1) maps out of the glass");
  check(near(toScreen({0, 1, 0}).x, 1), "acc +y maps to screen right");
  check(plausible({0, -0.05f, -1.15f}) && !plausible({0, 0, 0}) && !plausible({0, 2, 0}), "plausible samples");

  // Flat readings.
  Reading flat = measure({0, 0, 1}, Mode::Flat);
  check(near(flat.x, 0) && near(flat.y, 0) && near(flat.tilt, 0), "flat is level");
  Reading right = measure(tilted(3, 0), Mode::Flat);
  check(near(right.x, 3) && near(right.y, 0) && near(right.tilt, 3), "right edge raised 3 degrees");
  Reading both = measure(tilted(-2, 4), Mode::Flat);
  check(near(both.x, -2) && near(both.y, 4), "two-axis tilt");
  check(both.tilt > 4 && both.tilt < 4.5f, "total tilt combines axes");
  check(near(measure({0, 0, -1}, Mode::Flat).tilt, 0), "face down still reads level");

  // Edge readings: rotate the board 2 degrees in the screen plane.
  float s = sinf(2 / kDegrees), c = cosf(2 / kDegrees);
  Reading portrait = measure({s, -c, 0}, Mode::Portrait);
  check(portrait.slot == 1 && near(portrait.x, 2) && near(portrait.y, 0), "portrait upright");
  check(measure({s, c, 0}, Mode::Portrait).slot == 2, "portrait inverted slot");
  Reading landscape = measure({c, -s, 0}, Mode::Landscape);
  check(landscape.slot == 3 && near(landscape.y, -2) && near(landscape.tilt, 2), "landscape on left edge");
  check(measure({-c, s, 0}, Mode::Landscape).slot == 4, "landscape on right edge slot");

  // Mode selection and hysteresis.
  check(chooseMode({0, 0, 1}, Mode::Portrait) == Mode::Flat, "flat detected");
  check(chooseMode({0, -1, 0}, Mode::Flat) == Mode::Portrait, "portrait detected");
  check(chooseMode({1, 0, 0}, Mode::Flat) == Mode::Landscape, "landscape detected");
  Vec3 boundary = {0, -0.6f, 0.75f};  // |z| = 0.78 normalized
  check(chooseMode(boundary, Mode::Flat) == Mode::Flat, "stays flat inside hysteresis");
  check(chooseMode(boundary, Mode::Portrait) == Mode::Portrait, "stays on edge inside hysteresis");
  Vec3 diagonal = normalized({0.75f, -0.66f, 0});
  check(chooseMode(diagonal, Mode::Portrait) == Mode::Portrait, "edge hysteresis near diagonal");
  check(chooseMode(diagonal, Mode::Landscape) == Mode::Landscape, "edge hysteresis both ways");

  // Zeroing.
  Reading zeroed = applyZero(measure(tilted(1, -1), Mode::Flat), 1, -1);
  check(near(zeroed.x, 0) && near(zeroed.y, 0) && near(zeroed.tilt, 0), "zero removes offset");
  check(near(applyZero(portrait, 0.5f, 0).tilt, 1.5f), "edge zero");

  // Bubble mapping.
  check(near(bubblePixels(0, 100), 0), "level bubble centered");
  check(near(bubblePixels(1, 100), -bubblePixels(-1, 100)), "bubble mapping symmetric");
  check(bubblePixels(1, 100) > 12 && bubblePixels(1, 100) < 13, "about 12 px per degree near level");
  check(bubblePixels(90, 100) <= 100, "bubble stays inside the vial");

  // Smoothing converges and is frame-rate independent.
  Vec3 a{0, 0, 0}, b{0, 0, 0};
  for (int i = 0; i < 10; ++i) smooth(a, {1, 0, 0}, 0.01f);
  smooth(b, {1, 0, 0}, 0.05f);
  smooth(b, {1, 0, 0}, 0.05f);
  check(near(a.x, b.x, 1e-4f), "smoothing independent of step size");
  for (int i = 0; i < 200; ++i) smooth(a, {1, 0, 0}, 0.01f);
  check(near(a.x, 1, 1e-3f), "smoothing converges");

  if (failures) { std::printf("%d test(s) failed.\n", failures); return 1; }
  std::printf("All level math tests passed.\n");
  return 0;
}
