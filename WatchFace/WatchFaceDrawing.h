#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <time.h>
#include "WatchFaceConfig.h"

// Screen-only interpretation of the reference; the physical watch bezel is
// outside its display and is not drawn here. All colors are RGB565.
namespace WatchFace {
constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xF7BF;       // Slight lavender tint.
constexpr uint16_t PURPLE = 0xB19F;
constexpr uint16_t PALE_TICK = 0xD6B7;
constexpr uint16_t HEART_RATE = 0xEF5A;  // Pale yellow-green.
constexpr float PI_F = 3.14159265358979323846f;

inline void pointOnRing(int second, float radius, int& x, int& y) {
  const float angle = second * (2.0f * PI_F / 60.0f);
  x = lroundf(120.0f + sinf(angle) * radius);
  y = lroundf(120.0f - cosf(angle) * radius);
}

inline void draw(lgfx::LGFXBase& canvas, const tm& clock) {
  canvas.fillScreen(BLACK);
  canvas.setTextSize(1);
  canvas.setTextWrap(false);

  // Sixty slender ticks. The pale upper-left section matches the photo.
  for (int tick = 0; tick < 60; ++tick) {
    int x0, y0, x1, y1;
    const bool major = tick % 5 == 0;
    pointOnRing(tick, major ? 100.0f : 103.0f, x0, y0);
    pointOnRing(tick, 112.0f, x1, y1);
    const uint16_t color = (tick >= 52 || tick == 0) ? PALE_TICK : PURPLE;
    canvas.drawWideLine(x0, y0, x1, y1, major ? 0.85f : 0.55f, color);
  }

  // A moving white seconds marker, initially in the photo's upper-right area.
  int dotX, dotY;
  pointOnRing(clock.tm_sec, 108.0f, dotX, dotY);
  canvas.fillSmoothCircle(dotX, dotY, 5, WHITE);

  canvas.setTextDatum(lgfx::textdatum_t::baseline_right);
  canvas.setTextColor(HEART_RATE, BLACK);
  canvas.setFont(&fonts::FreeSans12pt7b);
  char heartRate[12];
  snprintf(heartRate, sizeof(heartRate), "%d", SAMPLE_HEART_RATE);
  canvas.drawString(heartRate, 126, 71);

  // Small filled heart built from two lobes and a tapered base.
  canvas.fillSmoothCircle(136, 64, 3, WHITE);
  canvas.fillSmoothCircle(142, 64, 3, WHITE);
  canvas.fillTriangle(132, 65, 146, 65, 139, 73, WHITE);

  // Native 72-pixel DejaVu glyphs, with compact spacing to fit the dial.
  // Separate positions keep the width stable when the digits change.
  canvas.setFont(&fonts::DejaVu72);
  canvas.setTextColor(WHITE, BLACK);
  canvas.setTextDatum(lgfx::textdatum_t::baseline_center);
  char clockText[6];
  snprintf(clockText, sizeof(clockText), "%02d:%02d", clock.tm_hour, clock.tm_min);
  constexpr int positions[5] = {48, 90, 119, 148, 190};
  for (int index = 0; index < 5; ++index) {
    const char glyph[2] = {clockText[index], '\0'};
    canvas.drawString(glyph, positions[index], 138);
  }

  static const char* const months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  char dateText[16];
  snprintf(dateText, sizeof(dateText), "%s %d", months[clock.tm_mon], clock.tm_mday);
  canvas.setFont(&fonts::FreeSans12pt7b);
  canvas.drawString(dateText, 120, 165);
}
}  // namespace WatchFace
