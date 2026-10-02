#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdint.h>
#include "EyeConfig.h"
#include "EyeTextures.h"

namespace BlinkingEye {
class Animation {
  uint32_t rng = 1;
  float phase = 0, blinkWait = 1.8f, blinkTime = -1, closure = 0;
  bool secondBlink = false;
  float gazeWait = 1, gazeTime = 1, gazeDuration = .12f;
  float gazeX = 0, gazeY = 0, fromX = 0, fromY = 0, targetX = 0, targetY = 0;
  float shape[240] = {}, openTop[240] = {}, openBottom[240] = {};
  int16_t left[240] = {}, right[240] = {};
  uint32_t nextRandom() {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
  }
  float range(float low, float high) {
    return low + (high-low)*(nextRandom() & 65535)/65535.0f;
  }
  static float clamp(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
  static float ease(float x) { x = clamp(x); return x*x*(3-2*x); }
  static uint16_t mix(uint16_t a, uint16_t b, int alpha) {
    const int inv = 256-alpha;
    return (((((a>>11)*inv+(b>>11)*alpha)>>8)&31)<<11) |
           ((((((a>>5)&63)*inv+((b>>5)&63)*alpha)>>8)&63)<<5) |
           ((((a&31)*inv+(b&31)*alpha)>>8)&31);
  }
  static uint16_t shade(uint16_t color, int light) {
    return ((((color>>11)*light)>>8)<<11) |
           (((((color>>5)&63)*light)>>8)<<5) |
           (((color&31)*light)>>8);
  }
public:
  void begin(uint32_t seed) {
    rng = seed ? seed : 1;
    phase = closure = 0; blinkWait = 1.8f; blinkTime = -1; secondBlink = false;
    gazeWait = .8f; gazeTime = 1; gazeDuration = .12f;
    gazeX = gazeY = fromX = fromY = targetX = targetY = 0;
    for (int x = 0; x < 240; ++x) {
      const float u = (x-119.5f)/103;
      shape[x] = powf(fmaxf(0, 1-u*u), .65f);
      openTop[x] = 118-47*shape[x]+3*u;
      openBottom[x] = 118+36*shape[x]+3*u;
    }
    for (int y = 0; y < 240; ++y) {
      const float dy = y-119.5f;
      const float halfWidth = sqrtf(fmaxf(0, 119.5f*119.5f-dy*dy));
      left[y] = static_cast<int16_t>(ceilf(119.5f-halfWidth));
      right[y] = static_cast<int16_t>(floorf(119.5f+halfWidth));
    }
  }
  void update(float seconds) {
    seconds = fmaxf(0, fminf(seconds, .1f));
    phase = fmodf(phase+seconds, 62.831853f);
    if (blinkTime < 0) {
      blinkWait -= seconds;
      if (blinkWait <= 0) blinkTime = 0;
    } else {
      blinkTime += seconds;
      if (blinkTime < .10f) closure = ease(blinkTime/.10f);
      else if (blinkTime < .14f) closure = 1;
      else if (blinkTime < .34f) closure = 1-ease((blinkTime-.14f)/.20f);
      else {
        closure = 0; blinkTime = -1;
        if (!secondBlink && nextRandom()%6 == 0) {
          blinkWait = .16f; secondBlink = true;
        } else {
          blinkWait = range(EyeConfig::MIN_BLINK_SECONDS, EyeConfig::MAX_BLINK_SECONDS);
          secondBlink = false;
        }
      }
    }
    gazeWait -= seconds;
    if (gazeWait <= 0 && closure < .1f) {
      fromX = gazeX; fromY = gazeY;
      // Regularly return to looking at the viewer.
      const bool centered = nextRandom()%3 == 0;
      targetX = centered ? 0 : range(-EyeConfig::GAZE_RANGE_X, EyeConfig::GAZE_RANGE_X);
      targetY = centered ? 0 : range(-EyeConfig::GAZE_RANGE_Y, EyeConfig::GAZE_RANGE_Y);
      gazeTime = 0; gazeDuration = range(.085f, .15f); gazeWait = range(1.0f, 2.8f);
    }
    gazeTime += seconds;
    const float t = ease(gazeTime/gazeDuration);
    gazeX = fromX+(targetX-fromX)*t;
    gazeY = fromY+(targetY-fromY)*t;
  }
  float blinkAmount() const { return closure; }
  void draw(lgfx::LGFXBase& canvas) const {
    float top[240], bottom[240];
    for (int x = 0; x < 240; ++x) {
      const float seam = 118+18*shape[x]+3*(x-119.5f)/103;
      top[x] = openTop[x]+(seam-openTop[x])*closure+gazeY*.16f*(1-closure);
      bottom[x] = openBottom[x]+(seam-openBottom[x])*closure+gazeY*.12f*(1-closure);
    }
    const int cx = lroundf(120+gazeX), cy = lroundf(116+gazeY-3*closure);
    const float pupil = 18.5f+1.2f*sinf(phase*.7f)+.35f*sinf(phase*1.9f);
    const int pupil2 = static_cast<int>(pupil*pupil);
    const int highlightX = lroundf(106+gazeX*.55f);
    const int highlightY = lroundf(100+gazeY*.55f);
    uint16_t row[240];
    canvas.setSwapBytes(true); // Texture arrays and scanline use native RGB565.
    for (int y = 0; y < 240; ++y) {
      for (int x = 0; x < 240; ++x) {
        if (x < left[y] || x > right[y]) { row[x] = 0; continue; }
        const float upperDistance = y-top[x], lowerDistance = bottom[x]-y;
        uint16_t skin = EyeTextures::SKIN[y*240+x];
        if (shape[x] > 0) {
          // Soft upper fold follows the moving lid. A bright lower waterline
          // and a tiny shadow beneath it make the tissue feel rounded.
          const float fold = fabsf(upperDistance+10+4*(1-closure));
          if (fold < 2.5f) skin = shade(skin, 256-static_cast<int>((2.5f-fold)*13));
          if (upperDistance > -2 && upperDistance <= 0)
            skin = mix(skin, 0x3945, static_cast<int>((upperDistance+2)*100));
          if (lowerDistance > -3 && lowerDistance <= 0)
            skin = mix(skin, 0xC48E, static_cast<int>((lowerDistance+3)*37));
        }
        uint16_t ink = skin;
        if (shape[x] > 0 && upperDistance > 0 && lowerDistance > 0) {
          ink = EyeTextures::SCLERA[y*240+x];
          const int ix = x-cx+48, iy = y-cy+48;
          const int dx = x-cx, dy = y-cy, radius2 = dx*dx+dy*dy;
          if (ix >= 0 && ix < 96 && iy >= 0 && iy < 96 && radius2 < 46*46) {
            const uint16_t iris = EyeTextures::IRIS[iy*96+ix];
            ink = radius2 > 45*45 ? mix(ink, iris, (46*46-radius2)*256/91) : iris;
            // Smooth pupil edge with subtle changes in diameter.
            if (radius2 < pupil2+38) {
              const int alpha = radius2 <= pupil2 ? 256 : (pupil2+38-radius2)*256/38;
              ink = mix(ink, 0x0020, alpha);
            }
            // Broad soft corneal reflection plus a crisp small catchlight.
            const int hx = x-highlightX, hy = y-highlightY;
            const int reflection = hx*hx+4*hy*hy;
            if (reflection < 70) ink = mix(ink, 0xF7DE, (70-reflection)*210/70);
            if ((hx+2)*(hx+2)+(hy+1)*(hy+1) < 5) ink = 0xFFFF;
            const int sx = x-(cx+16), sy = y-(cy+20);
            const int reflected = sx*sx+sy*sy;
            if (reflected < 14) ink = mix(ink, 0xAE5B, (14-reflected)*90/14);
          }
          // The upper lid casts a soft shadow onto iris and sclera.
          const int shadow = upperDistance < 13 ? 116+static_cast<int>(upperDistance*140/13) : 256;
          ink = shade(ink, shadow);
          const float edge = fminf(upperDistance, lowerDistance);
          if (edge < 1) ink = mix(skin, ink, static_cast<int>(edge*256));
        }
        if (shape[x] > 0) {
          const int lashY = lroundf(upperDistance)+24;
          if (lashY >= 0 && lashY < 28) {
            const int alpha = EyeTextures::LASHES[lashY*240+x];
            if (alpha) ink = mix(ink, 0x18A2, alpha);
          }
        }
        row[x] = ink;
      }
      canvas.pushImage(0, y, 240, 1, row);
    }
  }
};
}
