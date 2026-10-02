#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdint.h>
#include "HyperspaceConfig.h"

namespace Hyperspace {
constexpr float TAU_F = 6.28318530718f;
constexpr float FAR_Z = 360;
constexpr float FOCAL = 110;
struct Color { uint8_t r,g,b; };
struct Point { float x,y; };

class Animation {
  struct Star { float x,y,z,light; Color color; };
  struct Warp { float x,y,c,s; };
  Star stars[HyperspaceConfig::STARS] = {};
  Warp warp[128] = {};
  uint16_t glow[256] = {};
  int16_t left[240] = {}, right[240] = {};
  uint32_t rng = 1;
  float waveA=0, waveB=1.2f, waveC=0;
  float speed=160, targetSpeed=160, speedWait=3;
  float centerX=120, centerY=120;

  uint32_t random() {
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng;
  }
  float range(float low,float high) {
    return low+(high-low)*(random()&65535)/65535.0f;
  }
  static float clamp(float value) { return value<0?0:(value>1?1:value); }
  static uint16_t pack(int r,int g,int b) {
    return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
  }
  void spawn(Star& star,bool initial) {
    const float angle=range(0,TAU_F);
    const float radius=sqrtf(range(0,1))*123+3;
    star.x=cosf(angle)*radius;
    star.y=sinf(angle)*radius;
    // Initially distribute visible stars in depth, avoiding an empty start.
    const float nearZ=radius*FOCAL/135;
    star.z=initial?range(nearZ,FAR_Z):FAR_Z;
    star.light=range(.65f,1.0f);
    switch(random()%5) {
      case 0: star.color={171,126,255}; break;
      case 1: star.color={80,226,255}; break;
      default: star.color={120,190,255}; break;
    }
  }
  void updateWarp() {
    const float wobble=HyperspaceConfig::WOBBLE;
    centerX=119.5f+wobble*(17*sinf(waveA)+7*sinf(waveB));
    centerY=119.5f+wobble*(13*cosf(waveB)+6*sinf(waveC));
    const float roll=wobble*(.25f*sinf(waveC)+.12f*sinf(waveB));
    for(int i=0;i<128;++i) {
      const float depth=static_cast<float>(i)/127;
      const float near=1-depth;
      const float twist=roll+wobble*.32f*near*sinf(waveA+depth*5);
      warp[i]={centerX+wobble*13*near*sinf(waveB+depth*6),
               centerY+wobble*11*near*cosf(waveA+depth*5),cosf(twist),sinf(twist)};
    }
  }
  Point project(const Star& star,float depth) const {
    const float z=fmaxf(8,depth);
    const int index=static_cast<int>(fminf(127,z*127/FAR_Z));
    const Warp& w=warp[index];
    const float scale=FOCAL/z;
    return {w.x+(star.x*w.c-star.y*w.s)*scale,
            w.y+(star.x*w.s+star.y*w.c)*scale};
  }
  static void addPixel(lgfx::LGFXBase& canvas,int x,int y,Color color,float amount) {
    if(x<0 || x>=240 || y<0 || y>=240) return;
    const float dx=x-119.5f,dy=y-119.5f;
    const float distance2=dx*dx+dy*dy;
    if(distance2>=119*119) return;
    amount=clamp(amount)*clamp((119*119-distance2)/930.0f);
    if(amount<.008f) return;
    // LovyanGFX readPixel returns RGB565. Add light for overlapping streaks.
    const uint16_t background=canvas.readPixel(x,y);
    int red=(background>>11)*255/31+static_cast<int>(color.r*amount);
    int green=((background>>5)&63)*255/63+static_cast<int>(color.g*amount);
    int blue=(background&31)*255/31+static_cast<int>(color.b*amount);
    if(red>255) red=255;
    if(green>255) green=255;
    if(blue>255) blue=255;
    canvas.drawPixel(x,y,pack(red,green,blue));
  }
  static void point(lgfx::LGFXBase& canvas,Point p,Color color,float light) {
    const int x=static_cast<int>(floorf(p.x)),y=static_cast<int>(floorf(p.y));
    const float fx=p.x-x,fy=p.y-y;
    addPixel(canvas,x,y,color,light*(1-fx)*(1-fy));
    addPixel(canvas,x+1,y,color,light*fx*(1-fy));
    addPixel(canvas,x,y+1,color,light*(1-fx)*fy);
    addPixel(canvas,x+1,y+1,color,light*fx*fy);
  }
  static void streak(lgfx::LGFXBase& canvas,Point from,Point to,Color color,float light) {
    const float dx=to.x-from.x,dy=to.y-from.y;
    int steps=static_cast<int>(ceilf(fmaxf(fabsf(dx),fabsf(dy))));
    if(steps<1) steps=1;
    if(steps>160) steps=160; // Bound rendering even near the projection plane.
    for(int i=0;i<steps;++i) {
      const float t=static_cast<float>(i)/steps;
      const Point p={from.x+dx*t,from.y+dy*t};
      // Wide dim halo under the sharper antialiased colored core.
      addPixel(canvas,lroundf(p.x)-1,lroundf(p.y),color,light*.07f);
      addPixel(canvas,lroundf(p.x)+1,lroundf(p.y),color,light*.07f);
      point(canvas,p,color,light);
    }
  }
public:
  void begin(uint32_t seed) {
    rng=seed?seed:1;
    waveA=0; waveB=1.2f; waveC=0;
    speed=targetSpeed=(HyperspaceConfig::MIN_SPEED+HyperspaceConfig::MAX_SPEED)*.5f;
    speedWait=3;
    for(auto& star:stars) spawn(star,true);
    for(int i=0;i<256;++i) {
      const float light=expf(-i/33.0f);
      glow[i]=pack(static_cast<int>(2*light),static_cast<int>(9*light),static_cast<int>(24*light));
    }
    for(int y=0;y<240;++y) {
      const float dy=y-119.5f;
      const float half=sqrtf(fmaxf(0,119.5f*119.5f-dy*dy));
      left[y]=static_cast<int16_t>(ceilf(119.5f-half));
      right[y]=static_cast<int16_t>(floorf(119.5f+half));
    }
    updateWarp();
  }
  void update(float seconds) {
    seconds=fmaxf(0,fminf(seconds,.1f));
    waveA=fmodf(waveA+seconds*.73f,TAU_F);
    waveB=fmodf(waveB+seconds*1.13f,TAU_F);
    waveC=fmodf(waveC+seconds*.31f,TAU_F);
    speedWait-=seconds;
    if(speedWait<=0) {
      targetSpeed=range(HyperspaceConfig::MIN_SPEED,HyperspaceConfig::MAX_SPEED);
      speedWait=range(4,8);
    }
    speed+=(targetSpeed-speed)*fminf(1,seconds*.8f);
    for(auto& star:stars) {
      star.z-=speed*seconds;
      const float radius2=(star.x*star.x+star.y*star.y)*FOCAL*FOCAL;
      if(star.z<8 || radius2>170*170*star.z*star.z) spawn(star,false);
    }
    updateWarp();
  }
  void draw(lgfx::LGFXBase& canvas) const {
    uint16_t row[240];
    canvas.setSwapBytes(true);
    const int cx=lroundf(centerX),cy=lroundf(centerY);
    for(int y=0;y<240;++y) {
      for(int x=0;x<240;++x) {
        if(x<left[y] || x>right[y]) { row[x]=0; continue; }
        const int dx=x-cx,dy=y-cy;
        const int lookup=(dx*dx+dy*dy)>>5;
        row[x]=lookup<256?glow[lookup]:0;
      }
      canvas.pushImage(0,y,240,1,row);
    }
    for(const auto& star:stars) {
      const float born=clamp((FAR_Z-star.z)/22);
      if(born<=0) continue;
      const float depthLight=.25f+.75f*(1-star.z/FAR_Z);
      const float light=born*depthLight*star.light;
      const float length=speed*HyperspaceConfig::TRAIL_SECONDS*(.8f+.3f*star.light);
      Point previous=project(star,star.z+length);
      for(int segment=5;segment>=0;--segment) {
        Point next=project(star,star.z+length*segment/6);
        const float strength=(6-segment)/6.0f;
        streak(canvas,previous,next,star.color,light*strength*strength*2.2f);
        previous=next;
      }
      point(canvas,previous,{214,243,255},light*3.0f);
    }
  }
};
}
