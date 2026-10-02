#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdint.h>
#include "FireworksConfig.h"

namespace Fireworks {
constexpr float PI_F = 3.14159265358979323846f;
struct Color { uint8_t r, g, b; };
constexpr Color PALETTE[] = {
  {255, 177, 52}, {70, 205, 255}, {255, 75, 139},
  {156, 113, 255}, {75, 255, 160}, {255, 113, 48}
};

class Animation {
  struct Point { float x, y; };
  struct Spark {
    Point position;
    float vx, vy, age, life, drag, gravity;
    Color color;
    bool glitter;
    uint8_t count;
    Point trail[FireworksConfig::TRAIL_POINTS];
  };
  struct Rocket {
    bool active;
    Point position;
    float vx, vy, fuse, exhaust;
    uint8_t style;
    Color color;
  };
  struct Star { int16_t x, y; float phase; };
  Spark sparks[FireworksConfig::MAX_SPARKS] = {};
  Rocket rockets[3] = {};
  Star stars[24] = {};
  uint32_t rng = 1;
  unsigned cursor = 0;
  unsigned showIndex = 0;
  unsigned tick = 0;
  float accumulator = 0;
  float untilLaunch = 0;
  float skyPhase = 0;

  uint32_t random() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
  }
  float range(float low, float high) {
    return low + (high - low) * (random() & 65535) / 65535.0f;
  }
  static float clamp(float value) {
    return value < 0 ? 0 : (value > 1 ? 1 : value);
  }
  static void pixel(lgfx::LGFXBase& canvas, int x, int y, Color color, float light) {
    const float dx = x - 119.5f, dy = y - 119.5f;
    const float radius2 = dx * dx + dy * dy;
    if (radius2 > 118.0f * 118.0f) return;
    // Feather just the outermost eight pixels of the circular aperture.
    light = clamp(light * FireworksConfig::BRIGHTNESS) *
            clamp((118.0f * 118.0f - radius2) / 1824.0f);
    const uint16_t r = static_cast<uint16_t>(color.r * light);
    const uint16_t g = static_cast<uint16_t>(color.g * light);
    const uint16_t b = static_cast<uint16_t>(color.b * light);
    const uint16_t ink = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    if (ink) canvas.drawPixel(x, y, ink);
  }
  static void line(lgfx::LGFXBase& canvas, Point from, Point to,
                   Color color, float light) {
    const float dx = to.x - from.x, dy = to.y - from.y;
    const int steps = static_cast<int>(ceilf(fmaxf(fabsf(dx), fabsf(dy))));
    if (!steps) {
      pixel(canvas, lroundf(to.x), lroundf(to.y), color, light);
      return;
    }
    for (int i = 0; i <= steps; ++i) {
      const float t = static_cast<float>(i) / steps;
      pixel(canvas, lroundf(from.x + dx * t), lroundf(from.y + dy * t), color, light);
    }
  }
  void emit(Point position, float vx, float vy, Color color, float life,
            float drag, float gravity, bool glitter) {
    // A fixed pool bounds RAM. Drop a new spark if the pool is full.
    for (int search = 0; search < FireworksConfig::MAX_SPARKS; ++search) {
      Spark& spark = sparks[cursor];
      cursor = (cursor + 1) % FireworksConfig::MAX_SPARKS;
      if (spark.life > 0) continue;
      spark.position = position;
      spark.vx = vx;
      spark.vy = vy;
      spark.age = 0;
      spark.life = life;
      spark.drag = drag;
      spark.gravity = gravity;
      spark.color = color;
      spark.glitter = glitter;
      spark.count = 1;
      spark.trail[0] = position;
      return;
    }
  }
  void burst(const Rocket& rocket) {
    const bool willow = rocket.style == 2;
    const bool palm = rocket.style == 3;
    const int count = palm ? 56 : 72;
    const float rotation = range(0, 2 * PI_F);
    for (int i = 0; i < count; ++i) {
      float angle = rotation + 2 * PI_F * i / count;
      float speed = range(43, 77);
      Color color = rocket.color;
      if (rocket.style == 0) {
        // Filled chrysanthemum with a few fast outer sparks.
        speed *= range(0.45f, 1.0f);
        angle += range(-0.06f, 0.06f);
      } else if (rocket.style == 1) {
        // Two concentric rings with a warmer inner ring.
        speed = (i % 2 ? 67 : 36) * range(0.96f, 1.04f);
        if (!(i % 2)) color = {255, 206, 117};
      } else if (willow) {
        speed = range(26, 60);
        color = {255, static_cast<uint8_t>(range(150, 211)), 53};
      } else {
        // Eight distinct palm branches, each with seven speeds.
        angle = rotation + 2 * PI_F * (i / 7) / 8 + range(-0.025f, 0.025f);
        speed = 27 + (i % 7) * 8;
      }
      emit(rocket.position, cosf(angle) * speed, sinf(angle) * speed,
           color, willow ? range(2.3f, 3.4f) : range(1.5f, 2.3f),
           willow ? 0.65f : 0.85f, willow ? 23.0f : 19.0f, i % 5 == 0);
    }
    // Short-lived white-hot core, localized rather than a full-screen flash.
    for (int i = 0; i < 10; ++i) {
      const float angle = 2 * PI_F * i / 10;
      emit(rocket.position, cosf(angle) * 25, sinf(angle) * 25,
           {255, 244, 214}, 0.22f, 2, 0, false);
    }
  }
  void launch() {
    for (auto& rocket : rockets) {
      if (rocket.active) continue;
      rocket.active = true;
      rocket.position = {range(91, 149), 235};
      rocket.fuse = range(0.85f, 1.10f);
      const float targetX = range(77, 163);
      const float targetY = range(72, 117);
      rocket.vx = (targetX - rocket.position.x) / rocket.fuse;
      rocket.vy = (targetY - rocket.position.y) / rocket.fuse - 35 * rocket.fuse;
      rocket.exhaust = 0;
      rocket.style = showIndex % 4;
      rocket.color = PALETTE[showIndex % 6];
      ++showIndex;
      return;
    }
  }
  void step(float seconds) {
    ++tick;
    skyPhase = fmodf(skyPhase + seconds, 2 * PI_F);
    untilLaunch -= seconds;
    if (untilLaunch <= 0) {
      launch();
      untilLaunch = fmaxf(0.3f, FireworksConfig::LAUNCH_INTERVAL + range(-0.25f, 0.25f));
    }
    for (auto& spark : sparks) {
      if (spark.life <= 0) continue;
      spark.age += seconds;
      if (spark.age >= spark.life) { spark.life = 0; continue; }
      const float drag = 1.0f / (1.0f + spark.drag * seconds);
      spark.vx *= drag;
      spark.vy = spark.vy * drag + spark.gravity * seconds;
      spark.position.x += spark.vx * seconds;
      spark.position.y += spark.vy * seconds;
      if (tick % 2 == 0) {
        if (spark.count < FireworksConfig::TRAIL_POINTS) ++spark.count;
        for (int i = spark.count - 1; i > 0; --i) spark.trail[i] = spark.trail[i - 1];
        spark.trail[0] = spark.position;
      }
    }
    for (auto& rocket : rockets) {
      if (!rocket.active) continue;
      rocket.position.x += rocket.vx * seconds;
      rocket.position.y += rocket.vy * seconds + 35 * seconds * seconds;
      rocket.vy += 70 * seconds;
      rocket.fuse -= seconds;
      rocket.exhaust += seconds;
      if (rocket.exhaust >= 1.0f / 45) {
        rocket.exhaust -= 1.0f / 45;
        emit(rocket.position, range(-8, 8), range(12, 30), {255, 161, 53},
             range(0.18f, 0.42f), 1.0f, 12, false);
      }
      if (rocket.fuse <= 0) { burst(rocket); rocket.active = false; }
    }
  }
public:
  void begin(uint32_t seed) {
    rng = seed ? seed : 1;
    cursor = showIndex = tick = 0;
    accumulator = untilLaunch = skyPhase = 0;
    for (auto& spark : sparks) spark.life = 0;
    for (auto& rocket : rockets) rocket.active = false;
    for (auto& star : stars) {
      const float angle = range(0, 2 * PI_F), radius = sqrtf(range(0, 1)) * 111;
      star = {static_cast<int16_t>(120 + cosf(angle) * radius),
              static_cast<int16_t>(120 + sinf(angle) * radius), range(0, 2 * PI_F)};
    }
  }
  void update(float seconds) {
    // Fixed physics steps keep flight paths consistent at varying render rates.
    accumulator += fmaxf(0, fminf(seconds, 0.1f));
    while (accumulator >= 1.0f / 60) {
      step(1.0f / 60);
      accumulator -= 1.0f / 60;
    }
  }
  void draw(lgfx::LGFXBase& canvas) const {
    canvas.fillScreen(0);
    for (const auto& star : stars)
      pixel(canvas, star.x, star.y, {92, 125, 184}, 0.12f + 0.08f * sinf(skyPhase + star.phase));
    for (const auto& spark : sparks) {
      if (spark.life <= 0) continue;
      const float remaining = clamp(1 - spark.age / spark.life);
      float light = sqrtf(remaining);
      if (spark.glitter && remaining < 0.5f)
        light *= 0.45f + 0.55f * fabsf(sinf(spark.age * 43));
      for (int i = spark.count - 1; i > 0; --i) {
        const float trailLight = 1.0f - static_cast<float>(i) / spark.count;
        line(canvas, spark.trail[i], spark.trail[i - 1], spark.color,
             light * trailLight * trailLight * 0.60f);
      }
      const int x = lroundf(spark.position.x), y = lroundf(spark.position.y);
      pixel(canvas, x - 1, y, spark.color, light * 0.10f);
      pixel(canvas, x + 1, y, spark.color, light * 0.10f);
      pixel(canvas, x, y - 1, spark.color, light * 0.10f);
      pixel(canvas, x, y + 1, spark.color, light * 0.10f);
      Color head = spark.color;
      const float heat = clamp(1 - spark.age / 0.3f);
      head.r += (255 - head.r) * heat;
      head.g += (255 - head.g) * heat;
      head.b += (255 - head.b) * heat;
      pixel(canvas, x, y, head, light);
    }
    for (const auto& rocket : rockets) {
      if (!rocket.active) continue;
      const int x = lroundf(rocket.position.x), y = lroundf(rocket.position.y);
      pixel(canvas, x - 1, y, {255, 145, 44}, 0.5f);
      pixel(canvas, x + 1, y, {255, 145, 44}, 0.5f);
      pixel(canvas, x, y + 1, {255, 188, 88}, 0.8f);
      pixel(canvas, x, y, {255, 250, 225}, 1);
    }
  }
};
}
