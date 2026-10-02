#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdint.h>
#include "DiscConfig.h"
#include "DiscTables.h"

namespace SpinningDisc {
static_assert((DiscConfig::ANGULAR_TILES+DiscConfig::CROSS_SPIRALS)%2 == 0,
              "Alternating colors require even combined winding to avoid a seam");
static_assert(DiscConfig::ANGULAR_TILES == DiscTables::ANGULAR_TILES &&
              DiscConfig::CROSS_SPIRALS == DiscTables::CROSS_SPIRALS,
              "Regenerate DiscTables.h when changing the spiral geometry");
class Animation {
  double turns = 0;
  struct Style { float rpm, hue, zoom, curve; };
  Style style = {DiscConfig::RPM,0,0,1}, from = style, target = style;
  uint32_t rng = 1;
  float wait = 4, transition = 0;
  uint32_t random() {
    rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng;
  }
  float range(float low, float high) {
    return low+(high-low)*(random()&65535)/65535.0f;
  }
  static int clampByte(int value) { return value<0 ? 0 : (value>255 ? 255 : value); }
  static uint16_t recolor(uint16_t color, int a, int b, int c) {
    const int red = (color>>11)*255/31;
    const int green = ((color>>5)&63)*255/63;
    const int blue = (color&31)*255/31;
    const unsigned r = clampByte((a*red+b*green+c*blue)/256);
    const unsigned g = clampByte((c*red+a*green+b*blue)/256);
    const unsigned bl = clampByte((b*red+c*green+a*blue)/256);
    return ((r>>3)<<11)|((g>>2)<<5)|(bl>>3);
  }
public:
  void begin(uint32_t seed=1) {
    rng = seed ? seed : 1; turns = 0; wait = 4; transition = 0;
    style = {DiscConfig::RPM,0,0,1}; from = target = style;
  }
  void update(float seconds) {
    seconds = fmaxf(0, fminf(seconds, .1f));
    if (DiscConfig::RANDOM_CHANGES) {
      wait -= seconds;
      if (wait<=0) {
        // Keep angles bounded while preserving a smooth path to the next hue.
        style.hue = fmodf(style.hue, 6.2831853f);
        from = style;
        target = {range(1.1f,4.2f)*(random()%2 ? 1 : -1),
                  style.hue+range(-2.7f,2.7f),range(-.23f,.23f),range(.35f,1.55f)};
        transition = 0;
        wait = range(DiscConfig::MIN_CHANGE_SECONDS,DiscConfig::MAX_CHANGE_SECONDS);
      }
      transition += seconds;
      float t = fminf(1,transition/fmaxf(.1f,DiscConfig::TRANSITION_SECONDS));
      t = t*t*(3-2*t);
      style.rpm = from.rpm+(target.rpm-from.rpm)*t;
      style.hue = from.hue+(target.hue-from.hue)*t;
      style.zoom = from.zoom+(target.zoom-from.zoom)*t;
      style.curve = from.curve+(target.curve-from.curve)*t;
    }
    turns = fmod(turns+seconds*style.rpm/60.0, 1.0);
    if (turns < 0) turns += 1.0;
  }
  void draw(lgfx::LGFXBase& canvas) const {
    // Evaluate the stationary pattern at angle minus rotation, which makes
    // positive RPM rotate the artwork clockwise without sliding it radially.
    const int offsetU = lround(turns*DiscConfig::ANGULAR_TILES*256+style.zoom*3.3f*256);
    const int offsetV = lround(turns*DiscConfig::CROSS_SPIRALS*256-style.zoom*4.2f*256);
    const int curveScale = lroundf(style.curve*256);
    // Rotate RGB about the neutral-gray axis. One matrix per frame, no
    // trigonometry or floating-point color arithmetic in the pixel loop.
    const float cosine = cosf(style.hue), sine = sinf(style.hue);
    const int a = lroundf((1+2*cosine)*256/3);
    const int b = lroundf(((1-cosine)/3-sine*.577350269f)*256);
    const int c = 256-a-b;
    const bool originalColor = fabsf(style.hue)<.001f;
    uint16_t row[240];
    canvas.setSwapBytes(true);
    for (int y=0; y<240; ++y) {
      for (int x=0; x<240; ++x) {
        const uint32_t coordinate = DiscTables::COORDINATES[y*240+x];
        if (coordinate & 0x80000000u) {
          row[x] = static_cast<uint16_t>(coordinate);
          continue;
        }
        const int v = (static_cast<int>((coordinate>>9)&511)+offsetV)&511;
        const int u = (static_cast<int>(coordinate&511)-offsetU+
                       (DiscTables::CURVE[v&255]*curveScale)/256)&511;
        const int tile = ((u^v)>>8)&1;
        uint16_t color = DiscTables::TILES[(tile<<16)|((v&255)<<8)|(u&255)];
        if (!originalColor) color = recolor(color,a,b,c);
        const unsigned shade = (coordinate>>18)&255;
        if (shade != 255) {
          color = (((color>>11)*shade/255)<<11) |
                  (((((color>>5)&63)*shade/255))<<5) | ((color&31)*shade/255);
        }
        row[x] = color;
      }
      canvas.pushImage(0,y,240,1,row);
    }
  }
};
}
