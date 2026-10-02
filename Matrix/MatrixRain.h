#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdint.h>
#include "MatrixConfig.h"

// Original 5x7 symbols: digits, circuitry and angular code fragments.
// Each byte is a row, with the five low bits read from left to right.
namespace MatrixRain {
constexpr uint8_t GLYPHS[][7] = {
  {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
  {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
  {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
  {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8},
  {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12},
  {31,4,4,31,4,4,4}, {16,18,18,31,2,2,6},
  {4,31,4,14,21,4,12}, {1,1,31,5,9,17,1},
  {31,1,2,4,10,17,0}, {17,17,31,1,1,2,12},
  {4,4,31,4,4,10,17}, {0,31,1,1,1,1,31},
  {16,8,0,17,17,2,12}, {31,4,4,4,4,4,31},
  {2,4,8,24,8,8,8}, {10,10,31,10,10,2,4},
  {14,10,14,4,31,4,4}, {31,17,21,21,21,17,31},
  {4,14,21,4,21,14,4}, {17,10,4,31,4,10,17},
  {28,16,16,16,16,16,28}, {7,1,1,1,1,1,7},
  {0,4,0,31,0,4,0}, {21,14,31,14,21,0,4},
  {31,16,23,21,23,1,31}, {4,21,21,31,4,4,14}
};
constexpr unsigned GLYPH_COUNT = sizeof(GLYPHS) / sizeof(GLYPHS[0]);

class Animation {
  struct Stream {
    float head, speed, tail, top, bottom, light;
    bool distant;
    uint8_t glyph[MatrixConfig::ROWS];
  };
  Stream streams[MatrixConfig::COLUMNS] = {};
  uint32_t randomState = 1;
  float mutationTime = 0;
  float phase = 0;

  uint32_t nextRandom() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
  }
  float range(float low, float high) {
    return low + (high - low) * (nextRandom() & 65535) / 65535.0f;
  }
  static float clamp(float value) {
    return value < 0 ? 0 : (value > 1 ? 1 : value);
  }
  static uint16_t color(float red, float green, float blue, float intensity) {
    const float level = clamp(intensity * MatrixConfig::BRIGHTNESS);
    const uint16_t r = static_cast<uint16_t>(red * level);
    const uint16_t g = static_cast<uint16_t>(green * level);
    const uint16_t b = static_cast<uint16_t>(blue * level);
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  }
  void reset(Stream& stream, bool initial) {
    stream.speed = range(25, 62) * (stream.distant ? 0.55f : 1.0f);
    stream.tail = range(65, 150);
    stream.light = stream.distant ? range(0.18f, 0.32f) : range(0.65f, 1.0f);
    stream.head = initial ? range(stream.top, stream.bottom + stream.tail * 0.6f)
                          : stream.top - range(8, 65);
    for (auto& glyph : stream.glyph) glyph = nextRandom() % GLYPH_COUNT;
  }
  static void drawGlyph(lgfx::LGFXBase& canvas, int x, int y,
                        uint8_t glyph, uint16_t ink) {
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if (!(GLYPHS[glyph][row] & (16 >> column))) continue;
        const int px = x + column;
        const int py = y + row;
        const float dx = px - 119.5f;
        const float dy = py - 119.5f;
        if (dx * dx + dy * dy <= 117.0f * 117.0f)
          canvas.drawPixel(px, py, ink);
      }
    }
  }
public:
  void begin(uint32_t seed) {
    randomState = seed ? seed : 1;
    phase = 0;
    mutationTime = 0;
    for (int column = 0; column < MatrixConfig::COLUMNS; ++column) {
      Stream& stream = streams[column];
      const float x = 4 + column * MatrixConfig::CELL;
      const float dx = x - 119.5f;
      const float halfChord = sqrtf(fmaxf(0, 117 * 117 - dx * dx));
      stream.top = 119.5f - halfChord;
      stream.bottom = 119.5f + halfChord;
      stream.distant = column % 3 == 1;
      reset(stream, true);
    }
  }
  void update(float seconds) {
    // Bound recovery after a stall; normal animation remains time-based.
    if (seconds > 0.1f) seconds = 0.1f;
    phase = fmodf(phase + seconds * 0.7f, 6.2831853f);
    mutationTime += seconds;
    for (auto& stream : streams) {
      stream.head += stream.speed * MatrixConfig::SPEED * seconds;
      if (stream.head - stream.tail > stream.bottom + 8) reset(stream, false);
    }
    if (mutationTime >= 0.09f) {
      mutationTime = 0;
      // Change a few characters in place instead of randomizing the whole dial.
      for (auto& stream : streams)
        stream.glyph[nextRandom() % MatrixConfig::ROWS] = nextRandom() % GLYPH_COUNT;
    }
  }
  void draw(lgfx::LGFXBase& canvas) const {
    canvas.fillScreen(0);
    // Quiet phosphor rim, with a slow breathing brightness.
    canvas.drawCircle(120, 120, 119, color(0, 65, 34, 0.30f + 0.08f * sinf(phase)));
    for (int column = 0; column < MatrixConfig::COLUMNS; ++column) {
      const Stream& stream = streams[column];
      const int x = 2 + column * MatrixConfig::CELL;
      for (int row = 0; row < MatrixConfig::ROWS; ++row) {
        const int y = 1 + row * MatrixConfig::CELL;
        const float behind = stream.head - (y + 3);
        if (behind < 0 || behind > stream.tail) continue;
        const float dx = x + 2 - 119.5f;
        const float dy = y + 3 - 119.5f;
        const float radius = sqrtf(dx * dx + dy * dy);
        const float edge = clamp((119.0f - radius) / 17.0f);
        if (edge <= 0) continue;
        const float remaining = 1.0f - behind / stream.tail;
        const float intensity = remaining * remaining * stream.light * edge;
        uint16_t ink;
        if (behind < MatrixConfig::CELL && !stream.distant) {
          ink = color(185, 255, 214, stream.light * edge);
        } else {
          const float mint = clamp(1.0f - behind / 34.0f);
          ink = color(12 + 65 * mint, 245, 65 + 65 * mint, intensity);
        }
        if (ink) drawGlyph(canvas, x, y, stream.glyph[row], ink);
      }
    }
  }
};
}
