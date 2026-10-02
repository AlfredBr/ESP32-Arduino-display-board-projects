#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdint.h>
#include "CoinAssets.h"
#include "UiText.h"

namespace GoldCoin {
constexpr float PI_F=3.14159265358979323846f;
class Animation {
  float elapsed=0,duration=1.9f,angle=0,startAngle=0,endAngle=0;
  bool active=false,hasResult=false;
  unsigned result=0;
  static uint16_t blend(uint16_t a,uint16_t b,unsigned alpha) {
    const unsigned inverse=255-alpha;
    return ((((a>>11)*inverse+(b>>11)*alpha)/255)<<11) |
      (((((a>>5)&63)*inverse+((b>>5)&63)*alpha)/255)<<5) |
      (((a&31)*inverse+(b&31)*alpha)/255);
  }
  static uint16_t shade(uint16_t color,unsigned level) {
    return ((((color>>11)*level)/255)<<11)|
      (((((color>>5)&63)*level)/255)<<5)|((color&31)*level/255);
  }
public:
  void begin() { elapsed=angle=startAngle=endAngle=0; active=hasResult=false; result=0; }
  void flip(uint32_t randomBits) {
    result=randomBits&1;
    startAngle=fmodf(angle,2*PI_F);
    duration=1.75f+((randomBits>>2)&255)*.3f/255;
    endAngle=(4+((randomBits>>10)&1))*2*PI_F+result*PI_F;
    elapsed=0; active=true; hasResult=false;
  }
  void update(float seconds) {
    if(!active) return;
    elapsed+=fmaxf(0,fminf(seconds,.1f));
    const float t=fminf(1,elapsed/duration),remaining=1-t;
    angle=startAngle+(endAngle-startAngle)*(1-remaining*remaining*remaining);
    if(t>=1) { active=false; hasResult=true; angle=result*PI_F; }
  }
  bool spinning() const { return active; }
  bool settled() const { return hasResult; }
  unsigned outcome() const { return result; } // 0 heads, 1 tails.
  void draw(lgfx::LGFXBase& canvas,bool showLabels=true) const {
    const float t=active?fminf(1,elapsed/duration):0;
    const float lift=active?sinf(PI_F*t):0;
    const float radius=80+5*lift,cy=116-13*lift;
    const float cosine=cosf(angle),flat=fmaxf(.015f,fabsf(cosine));
    const float rim=4.5f*fabsf(sinf(angle));
    const int face=cosine<0?1:0;
    const int base=face*CoinAssets::SIZE*CoinAssets::SIZE;
    int textureX[240]; float outer[240];
    for(int x=0;x<240;++x) {
      const float nx=(x-119.5f)/radius;
      textureX[x]=static_cast<int>((nx+1)*CoinAssets::SIZE*.5f);
      outer[x]=fabsf(nx)<1?sqrtf(1-nx*nx)*radius*flat+rim:-1;
    }
    uint16_t row[240];
    canvas.setSwapBytes(true);
    for(int y=0;y<240;++y) {
      const float dy=y-119.5f;
      const int textureY=static_cast<int>(((y-cy+rim*.4f)/(radius*flat)+1)*CoinAssets::SIZE*.5f);
      for(int x=0;x<240;++x) {
        const float dx=x-119.5f;
        if(dx*dx+dy*dy>=119.5f*119.5f) { row[x]=0; continue; }
        uint16_t ink=0x0842;
        if(dx*dx+dy*dy<90*90) ink=0x1063;
        const float shadow=(x-120)*(x-120)/(58.0f*58.0f)+(y-196)*(y-196)/36.0f;
        if(shadow<1) ink=0;
        if(outer[x]>=0 && fabsf(y-cy)<outer[x]) {
          const int stripe=(x%4==0)?25:0;
          const int value=150+static_cast<int>(55*(1-(y-cy)/(outer[x]+1)))-stripe;
          const int gold=value>255?255:value;
          ink=((gold>>3)<<11)|(((gold*3/5)>>2)<<5)|(gold/9>>3);
        }
        const int tx=textureX[x];
        if(tx>=0 && tx<CoinAssets::SIZE && textureY>=0 && textureY<CoinAssets::SIZE) {
          const int offset=base+textureY*CoinAssets::SIZE+tx;
          const unsigned alpha=CoinAssets::ALPHA[offset];
          if(alpha) ink=blend(ink,shade(CoinAssets::COLOR[offset],static_cast<unsigned>(190+65*flat)),alpha);
        }
        row[x]=ink;
      }
      canvas.pushImage(0,y,240,1,row);
    }
    if(showLabels) {
      UiText::draw(canvas,active?"FLIPPING":"TAP TO FLIP",120,20,1,0xD5D1);
      if(hasResult) UiText::draw(canvas,result?"TAILS":"HEADS",120,211,2,0xFEAA);
    }
  }
};
}
