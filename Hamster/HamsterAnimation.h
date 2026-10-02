#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdint.h>
#include "HamsterConfig.h"
#include "HamsterAssets.h"

namespace Hamster {
constexpr float TAU_F = 6.28318530718f;
class Animation {
  uint32_t rng = 1;
  float gait = 0, wheel = 0, moment = -1, wait = 4.5f, speed = 1;
  uint16_t raster[240];
  static uint16_t blend(uint16_t base, uint16_t over, unsigned alpha) {
    const unsigned inverse = 255-alpha;
    return ((((base>>11)*inverse+(over>>11)*alpha)/255)<<11) |
           (((((base>>5)&63)*inverse+((over>>5)&63)*alpha)/255)<<5) |
           (((base&31)*inverse+(over&31)*alpha)/255);
  }
  uint32_t random() {
    rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng;
  }
public:
  void begin(uint32_t seed) {
    rng = seed ? seed : 1;
    gait = wheel = 0; moment = -1; wait = 4.5f; speed = 1;
  }
  void update(float seconds) {
    seconds = fmaxf(0, fminf(seconds, .1f));
    if (moment < 0) {
      wait -= seconds;
      if (wait <= 0) moment = 0;
    } else {
      moment += seconds;
      if (moment > 3.3f) {
        moment = -1; wait = 4.5f+(random()%3000)/1000.0f;
      }
    }
    const float target = moment > 1.2f && moment < 2.8f ? 1.55f : 1.0f;
    speed += (target-speed)*fminf(1, seconds*5);
    gait = fmodf(gait+seconds*HamsterConfig::RUN_CYCLES_PER_SECOND*speed, 1.0f);
    // At the bottom, clockwise rotation moves the tread left under the paws.
    wheel = fmodf(wheel+seconds*1.05f*speed, TAU_F);
  }
  int pose() const {
    if (moment >= 0 && moment < .95f) return 8+static_cast<int>(gait*4)%4;
    if (moment >= .95f && moment < 1.30f)
      return 12+static_cast<int>((moment-.95f)/.35f*4)%4;
    return static_cast<int>(gait*8)%8;
  }
  void draw(lgfx::LGFXBase& canvas) {
    canvas.setSwapBytes(true);
    canvas.pushImage(0, 0, 240, 240, HamsterAssets::BACKGROUND);
    // The spokes and axle sit behind the animal, keeping its face unobstructed.
    for (int spoke = 0; spoke < 6; ++spoke) {
      const float angle = wheel+spoke*TAU_F/6;
      const int x = lroundf(120+cosf(angle)*109);
      const int y = lroundf(120+sinf(angle)*109);
      canvas.drawLine(120, 120, x, y, 0x2126);
      canvas.drawLine(121, 120, x+1, y, 0x2987);
    }
    // Short moving tread marks make rotation readable around the entire rim.
    for (int mark = 0; mark < 40; ++mark) {
      const float angle = wheel+mark*TAU_F/40;
      const float c = cosf(angle), s = sinf(angle);
      canvas.drawLine(lroundf(120+c*111), lroundf(120+s*111),
                      lroundf(120+c*114), lroundf(120+s*114), 0x39C8);
    }
    canvas.fillCircle(120, 120, 10, 0x0842);
    canvas.fillCircle(120, 119, 8, 0x4AAB);
    canvas.fillCircle(118, 117, 5, 0x6BAE);
    canvas.fillCircle(120, 120, 3, 0x18C4);
    // Grounding shadow follows the slight body bounce.
    const int bob = lroundf(sinf(gait*TAU_F*2)*.8f);
    canvas.fillEllipse(120, 218, 40, 4, 0x0822);
    canvas.fillEllipse(120, 219, 29, 2, 0x0000);
    const int frame = pose();
    const int originY = HamsterConfig::SPRITE_Y+bob;
    const int frameOffset = frame*HamsterAssets::WIDTH*HamsterAssets::HEIGHT;
    // Alpha blend into the sprite so fine fur and whiskers retain soft edges.
    for (int y = 0; y < HamsterAssets::HEIGHT; ++y) {
      const int screenY = originY+y;
      if (screenY < 0 || screenY >= 240) continue;
      for (int x = 0; x < HamsterAssets::WIDTH; ++x) {
        const int screenX = HamsterConfig::SPRITE_X+x;
        const int offset = frameOffset+y*HamsterAssets::WIDTH+x;
        const unsigned alpha = HamsterAssets::ALPHA[offset];
        const uint16_t background = canvas.readPixel(screenX, screenY);
        raster[x] = alpha ? blend(background, HamsterAssets::SPRITES[offset], alpha) : background;
        const float dx = screenX-119.5f, dy = screenY-119.5f;
        if (dx*dx+dy*dy > 119.5f*119.5f) raster[x] = 0;
      }
      canvas.pushImage(HamsterConfig::SPRITE_X, screenY, HamsterAssets::WIDTH, 1, raster);
    }
  }
};
}
