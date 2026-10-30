#pragma once
// The watch faces, rendered in software into RGB565 pixel buffers. No
// display library is used here, so preview.py runs this exact code on a PC.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "DigitFont.h"

namespace AplFaces {
constexpr int W = 368, H = 448;
constexpr float CX = W / 2.0f, CY = H / 2.0f;
constexpr float PI_F = 3.14159265358979f;

// A screen rectangle and the w * h pixels that cover it.
struct Window {
  uint16_t *px;
  int x, y, w, h;
};
struct Rect { int x, y, w, h; };
struct Color { float r, g, b; };

inline uint16_t pack(Color c) {
  auto channel = [](float v, int max) {
    v = v < 0 ? 0 : v > 1 ? 1 : v;
    return static_cast<uint16_t>(v * max + 0.5f);  // lroundf is a slow libm call.
  };
  return channel(c.r, 31) << 11 | channel(c.g, 63) << 5 | channel(c.b, 31);
}
// Ordered 4 x 4 dithering hides RGB565 banding in the soft light gradients.
inline uint16_t packDithered(Color c, int x, int y) {
  static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  float t = (bayer[(y & 3) * 4 + (x & 3)] + 0.5f) * (1 / 16.0f) - 0.5f;
  return pack({c.r + t * (1 / 31.0f), c.g + t * (1 / 63.0f), c.b + t * (1 / 31.0f)});
}
inline Color unpack(uint16_t p) {
  return {(p >> 11) / 31.0f, ((p >> 5) & 63) / 63.0f, (p & 31) / 31.0f};
}
inline Color mix(Color a, Color b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
// exp(x) for x <= 0 to about 1e-5: 2^x with a quartic for the fraction. The
// ESP32-S3's expf made the light face take 2.5 s per frame.
inline float fastExp(float x) {
  if (x < -80) return 0;
  float t = x * 1.4426950f + 0.5f;
  int32_t whole = static_cast<int32_t>(t);
  if (t < whole) --whole;  // floor without the libm call
  float f = t - 0.5f - whole;  // f in [-0.5, 0.5]
  float p = 1 + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
  union { float f; int32_t i; } u{p};
  u.i += whole << 23;
  return u.f;
}

// ---------------------------------------------------------------- Digital face

constexpr Color PINK{1.0f, 0.17f, 0.48f}, WHITE{1.0f, 1.0f, 1.0f};
constexpr int DIGIT_GAP = 6, RIGHT_MARGIN = 10, LINE_GAP = 30;

inline const DigitFont::Glyph *glyph(char ch) {
  for (const auto &g : DigitFont::GLYPHS)
    if (g.ch == ch) return &g;
  return nullptr;
}
inline int textWidth(const char *text) {
  int width = 0;
  for (const char *p = text; *p; ++p)
    if (auto g = glyph(*p)) width += g->advance + (p[1] ? DIGIT_GAP : 0);
  return width;
}
// Draws right-aligned text whose digits start at row top.
inline void drawLine(Window win, const char *text, int top, const uint16_t *shades) {
  int pen = W - RIGHT_MARGIN - textWidth(text);
  for (const char *p = text; *p; ++p) {
    auto g = glyph(*p);
    if (!g) continue;
    const uint8_t *alpha = DigitFont::ALPHA + g->offset;
    for (int j = 0; j < g->height; ++j) {
      int y = top + g->y + j - win.y;
      if (y < 0 || y >= win.h) continue;
      for (int i = 0; i < g->width; ++i) {
        int x = pen + g->x + i - win.x;
        uint8_t a = alpha[j * g->width + i];
        if (a && x >= 0 && x < win.w) win.px[y * win.w + x] = shades[a];
      }
    }
    pen += g->advance + DIGIT_GAP;
  }
}
// Hours on the first line and ":MM" on the second, like a stacked phone clock.
inline void renderDigital(Window win, int hour, int minute, bool ready, bool use24h, Color ink) {
  memset(win.px, 0, sizeof(uint16_t) * win.w * win.h);
  uint16_t shades[256];
  Color color = ready ? ink : mix(Color{0, 0, 0}, ink, 0.35f);
  for (int a = 0; a < 256; ++a) shades[a] = pack(mix(Color{0, 0, 0}, color, a / 255.0f));
  char first[4] = "--", second[4] = ":--";
  if (ready) {
    int shown = use24h ? hour : (hour + 11) % 12 + 1;
    if (use24h) snprintf(first, sizeof first, "%02d", shown);
    else snprintf(first, sizeof first, "%d", shown);
    snprintf(second, sizeof second, ":%02d", minute);
  }
  int top = (H - 2 * DigitFont::DIGIT_HEIGHT - LINE_GAP) / 2;
  drawLine(win, first, top, shades);
  drawLine(win, second, top + DigitFont::DIGIT_HEIGHT + LINE_GAP, shades);
}

// ------------------------------------------------------------ Light-fan face

// Broad dark hands; colored light pours from their facing edges into the
// wedge between them, one color per hand, under a thin seconds hand.
struct LightPalette { Color hour, minute, seconds; };
// Eastern time: green hour light, red minute light, yellow seconds.
constexpr LightPalette EASTERN_LIGHTS{{0.32f, 1.0f, 0.42f}, {1.0f, 0.26f, 0.32f}, {0.93f, 0.88f, 0.24f}};
// UTC in the French tricolore: blue hour light, red minute light, white seconds.
constexpr LightPalette FRENCH_LIGHTS{{0.10f, 0.33f, 1.0f}, {1.0f, 0.20f, 0.24f}, {1.0f, 1.0f, 1.0f}};
constexpr Color HAND_BODY{0.035f, 0.035f, 0.04f};
constexpr float HOUR_LENGTH = 128, MINUTE_LENGTH = 174, HAND_HALF_WIDTH = 7;
constexpr float SECONDS_LENGTH = 196, SECONDS_TAIL = 22, SECONDS_HALF_WIDTH = 1.4f;
constexpr float CAP_OUTER = 6.5f, CAP_INNER = 3.2f;

struct Hand {
  float dx, dy;      // Unit vector from the center toward the tip (screen axes).
  float length;
  float side;        // +1 when the lit wedge is clockwise of this hand.
  Color light;
  float inverseLength;
};
// Angles are clockwise from 12 o'clock, in radians.
inline Hand makeHand(float angle, float length, Color light) {
  return {sinf(angle), -cosf(angle), length, 1, light, 1 / length};
}
// Signed distance from a hand's center line; positive is clockwise of it.
inline float across(const Hand &h, float vx, float vy) { return -h.dy * vx + h.dx * vy; }
inline float along(const Hand &h, float vx, float vy) { return h.dx * vx + h.dy * vy; }

// Light reaching a point on a hand's lit side, at distance q past its edge.
inline float wallLight(const Hand &h, float a, float q) {
  // Multiplying by reciprocals: float division is slow on the ESP32-S3.
  float s = a * h.inverseLength;
  float reach = a < h.length ? 0.15f + 0.85f * s * s : fastExp((h.length - a) * (1 / 120.0f));
  if (a < 0) reach = 0.15f * fastExp(a * (1 / 20.0f));
  return reach * (fastExp(q * (-1 / 26.0f)) + 0.45f * fastExp(q * (-1 / 140.0f)));
}
inline float handCoverage(const Hand &h, float vx, float vy) {
  float a = along(h, vx, vy), c = fabsf(across(h, vx, vy));
  return clamp01(HAND_HALF_WIDTH + 0.5f - c) * clamp01(h.length + 0.5f - a) *
         clamp01(a + HAND_HALF_WIDTH + 0.5f);
}

inline void renderLights(Window win, float hourAngle, float minuteAngle, bool ready, const LightPalette &colors) {
  if (!ready) {
    // Waiting for the time: only a dim cap ring, so the screen isn't blank.
    memset(win.px, 0, sizeof(uint16_t) * win.w * win.h);
    for (int j = 0; j < win.h; ++j)
      for (int i = 0; i < win.w; ++i) {
        float vx = win.x + i + 0.5f - CX, vy = win.y + j + 0.5f - CY, d = sqrtf(vx * vx + vy * vy);
        float ring = clamp01(CAP_OUTER + 0.5f - d) * (1 - clamp01(CAP_INNER + 0.5f - d));
        if (ring > 0) win.px[j * win.w + i] = pack(mix(Color{0, 0, 0}, colors.seconds, 0.4f * ring));
      }
    return;
  }
  Hand hour = makeHand(hourAngle, HOUR_LENGTH, colors.hour);
  Hand minute = makeHand(minuteAngle, MINUTE_LENGTH, colors.minute);
  // The lit wedge is the narrower angle between the hands.
  float span = fmodf(minuteAngle - hourAngle + 4 * PI_F, 2 * PI_F);
  bool hourFirst = span <= PI_F;  // Clockwise from hour to minute.
  hour.side = hourFirst ? 1 : -1;
  minute.side = -hour.side;
  float hourTipX = hour.dx * hour.length, hourTipY = hour.dy * hour.length;
  float minuteTipX = minute.dx * minute.length, minuteTipY = minute.dy * minute.length;

  for (int j = 0; j < win.h; ++j) {
    float vy = win.y + j + 0.5f - CY;
    uint16_t *row = win.px + j * win.w;
    for (int i = 0; i < win.w; ++i) {
      float vx = win.x + i + 0.5f - CX;
      float ch = across(hour, vx, vy) * hour.side, cm = across(minute, vx, vy) * minute.side;
      float r = 0, g = 0, b = 0, white = 0;
      // Inside the wedge: on the lit side of both hands' center lines, so
      // overlapping hands close it instead of leaving a beam past the tip.
      if (ch > 0 && cm > 0) {
        float lh = wallLight(hour, along(hour, vx, vy), fmaxf(ch - HAND_HALF_WIDTH, 0));
        float lm = wallLight(minute, along(minute, vx, vy), fmaxf(cm - HAND_HALF_WIDTH, 0));
        r += lh * hour.light.r + lm * minute.light.r;
        g += lh * hour.light.g + lm * minute.light.g;
        b += lh * hour.light.b + lm * minute.light.b;
      }
      // A hot spot at each tip, visible from every side.
      for (int k = 0; k < 2; ++k) {
        const Hand &h = k ? minute : hour;
        float tx = vx - (k ? minuteTipX : hourTipX), ty = vy - (k ? minuteTipY : hourTipY);
        float d2 = tx * tx + ty * ty;
        if (d2 > 160 * 160) continue;  // The glow is below one RGB565 step out here.
        float halo = 0.45f * fastExp(sqrtf(d2) * (-1 / 22.0f));
        r += halo * h.light.r; g += halo * h.light.g; b += halo * h.light.b;
        white += 0.9f * fastExp(d2 * (-1 / 70.0f));
      }
      Color c{1 - fastExp(-1.5f * (r + white)), 1 - fastExp(-1.5f * (g + white)), 1 - fastExp(-1.5f * (b + white))};
      // Hands are dark silhouettes over the light; the minute hand is on top.
      c = mix(c, HAND_BODY, handCoverage(hour, vx, vy));
      c = mix(c, HAND_BODY, handCoverage(minute, vx, vy));
      row[i] = packDithered(c, win.x + i, win.y + j);
    }
  }
}

// Screen bounds of the seconds hand and center cap, padded and even-aligned
// for the CO5300, clipped to the screen.
inline Rect secondsBounds(float angle) {
  float dx = sinf(angle), dy = -cosf(angle), pad = CAP_OUTER + 2;
  float x0 = fminf(CX + dx * SECONDS_LENGTH, CX - dx * SECONDS_TAIL) - pad;
  float x1 = fmaxf(CX + dx * SECONDS_LENGTH, CX - dx * SECONDS_TAIL) + pad;
  float y0 = fminf(CY + dy * SECONDS_LENGTH, CY - dy * SECONDS_TAIL) - pad;
  float y1 = fmaxf(CY + dy * SECONDS_LENGTH, CY - dy * SECONDS_TAIL) + pad;
  int left = static_cast<int>(floorf(x0)) & ~1, top = static_cast<int>(floorf(y0)) & ~1;
  int right = static_cast<int>(ceilf(x1)) | 1, bottom = static_cast<int>(ceilf(y1)) | 1;
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > W - 1) right = W - 1;
  if (bottom > H - 1) bottom = H - 1;
  return {left, top, right - left + 1, bottom - top + 1};
}
inline Rect unite(Rect a, Rect b) {
  int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
  int x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w;
  int y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
  return {x0, y0, x1 - x0, y1 - y0};
}

// Blends the sweeping seconds hand and its ring-shaped cap into a window.
inline void drawSeconds(Window win, float angle, Color color) {
  Rect box = secondsBounds(angle);
  float dx = sinf(angle), dy = -cosf(angle);
  int x0 = box.x > win.x ? box.x : win.x, y0 = box.y > win.y ? box.y : win.y;
  int x1 = box.x + box.w < win.x + win.w ? box.x + box.w : win.x + win.w;
  int y1 = box.y + box.h < win.y + win.h ? box.y + box.h : win.y + win.h;
  for (int y = y0; y < y1; ++y) {
    float vy = y + 0.5f - CY;
    for (int x = x0; x < x1; ++x) {
      float vx = x + 0.5f - CX;
      float a = dx * vx + dy * vy, c = fabsf(-dy * vx + dx * vy);
      float hand = clamp01(SECONDS_HALF_WIDTH + 0.5f - c) * clamp01(SECONDS_LENGTH + 0.5f - a) *
                   clamp01(a + SECONDS_TAIL + 0.5f);
      float d = sqrtf(vx * vx + vy * vy);
      float cap = clamp01(CAP_OUTER + 0.5f - d), hole = clamp01(CAP_INNER + 0.5f - d);
      float ink = fmaxf(hand, cap) * (1 - hole);
      if (ink <= 0 && hole <= 0) continue;
      uint16_t &p = win.px[(y - win.y) * win.w + (x - win.x)];
      Color under = mix(unpack(p), Color{0, 0, 0}, hole);
      p = pack(mix(under, color, ink));
    }
  }
}

// Copies part of a full-screen background into a window.
inline void copyFrom(Window win, const uint16_t *screen) {
  for (int j = 0; j < win.h; ++j)
    memcpy(win.px + j * win.w, screen + (win.y + j) * W + win.x, sizeof(uint16_t) * win.w);
}

}  // namespace AplFaces
