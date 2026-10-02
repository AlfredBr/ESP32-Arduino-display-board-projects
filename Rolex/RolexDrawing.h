#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <time.h>
#include "DialTexture.h"

namespace RolexFace {
constexpr float PI_F=3.14159265358979323846f;
constexpr uint16_t SILVER=0xCE79, LIGHT=0xF7BE, STEEL=0x632C, LUME=0xEF5A;
struct Point { int x,y; };
inline Point point(float angle,float distance,float side=0) {
  return {static_cast<int>(lroundf(120+sinf(angle)*distance+cosf(angle)*side)),
          static_cast<int>(lroundf(120-cosf(angle)*distance+sinf(angle)*side))};
}
inline void line(lgfx::LGFXBase& c,Point a,Point b,uint16_t color) {
  c.drawLine(a.x,a.y,b.x,b.y,color);
}
inline void triangle(lgfx::LGFXBase& c,Point a,Point b,Point d,uint16_t color) {
  c.fillTriangle(a.x,a.y,b.x,b.y,d.x,d.y,color);
}
inline void blade(lgfx::LGFXBase& c,float angle,float start,float end,float width,uint16_t color) {
  const Point a=point(angle,start,width),b=point(angle,end),d=point(angle,start,-width);
  const Point tail=point(angle,start-5);
  triangle(c,a,b,d,color);triangle(c,a,d,tail,color);
  line(c,tail,a,LIGHT);line(c,a,b,LIGHT);line(c,b,d,STEEL);line(c,d,tail,STEEL);
}
inline void dateGlyph(lgfx::LGFXBase& c,int digit,int x,int y) {
  // Compact rounded stroke numerals, not LCD segments. -1 lifts the pen.
  static const int8_t paths[10][14][2]={
    {{1,1},{3,0},{5,1},{6,3},{6,9},{5,11},{3,12},{1,11},{0,9},{0,3},{1,1},{-2,0}},
    {{1,2},{3,0},{3,12},{-1,0},{1,12},{5,12},{-2,0}},
    {{0,2},{1,1},{3,0},{5,1},{6,3},{5,5},{0,11},{0,12},{6,12},{-2,0}},
    {{0,1},{3,0},{5,1},{6,3},{5,5},{3,6},{5,7},{6,9},{5,11},{3,12},{0,11},{-2,0}},
    {{5,12},{5,0},{0,8},{6,8},{-2,0}},
    {{6,0},{0,0},{0,5},{4,5},{6,7},{6,10},{4,12},{1,12},{0,11},{-2,0}},
    {{6,1},{4,0},{2,1},{0,4},{0,10},{2,12},{4,12},{6,10},{6,7},{4,5},{2,5},{0,7},{-2,0}},
    {{0,0},{6,0},{2,12},{-2,0}},
    {{3,0},{0,2},{0,4},{6,8},{6,10},{3,12},{0,10},{0,8},{6,4},{6,2},{3,0},{-2,0}},
    {{6,5},{4,7},{2,7},{0,5},{0,2},{2,0},{4,0},{6,2},{6,8},{4,11},{2,12},{0,11},{-2,0}}
  };
  Point prev={0,0};bool connected=false;
  for(const auto& p:paths[digit]) {
    if(p[0]==-2) break;
    if(p[0]==-1) {connected=false;continue;}
    const Point next={x+p[0],y+p[1]};
    if(connected) {
      line(c,prev,next,0x1082);
      line(c,{prev.x+1,prev.y},{next.x+1,next.y},0x1082);
    }
    prev=next;connected=true;
  }
}
inline void drawDate(lgfx::LGFXBase& c,int day) {
  if(day<1 || day>31) {c.drawLine(173,120,179,120,0x632C);return;}
  if(day>=10) {dateGlyph(c,day/10,168,114);dateGlyph(c,day%10,178,114);}
  else dateGlyph(c,day,173,114);
}
inline void draw(lgfx::LGFXBase& c,const tm& clock,bool timeReady,float fractionalSecond=0) {
  c.setSwapBytes(true);c.pushImage(0,0,240,240,DialTexture::PIXELS);
  drawDate(c,timeReady?clock.tm_mday:0);
  if(!timeReady) return;
  const float seconds=clock.tm_sec+fractionalSecond;
  const float minutes=clock.tm_min+seconds/60;
  const float hours=clock.tm_hour+minutes/60;
  const float hourAngle=hours*PI_F/6,minuteAngle=minutes*PI_F/30;
  const float secondAngle=seconds*PI_F/30,gmtAngle=hours*PI_F/12;
  // Red arrow reads local time on the fixed 24-hour bezel.
  const Point gmtBase=point(gmtAngle,66),gmtTip=point(gmtAngle,79);
  line(c,point(gmtAngle,-9),gmtBase,0xD248);
  triangle(c,gmtTip,point(gmtAngle,64,5),point(gmtAngle,64,-5),SILVER);
  triangle(c,point(gmtAngle,75),point(gmtAngle,67,3),point(gmtAngle,67,-3),LUME);
  line(c,point(gmtAngle,64,5),gmtTip,LIGHT);
  // Mercedes hour hand: metal stem, round luminous head and three spokes.
  blade(c,hourAngle,-5,52,3.2f,SILVER);
  const Point mercedes=point(hourAngle,38);
  c.fillCircle(mercedes.x+1,mercedes.y+1,8,0x1082);
  c.fillCircle(mercedes.x,mercedes.y,8,STEEL);
  c.fillCircle(mercedes.x,mercedes.y,7,LIGHT);
  c.fillCircle(mercedes.x,mercedes.y,5,LUME);
  for(int i=0;i<3;++i) {
    const float a=hourAngle+i*2*PI_F/3;
    line(c,mercedes,{mercedes.x+static_cast<int>(lroundf(6*sinf(a))),
                    mercedes.y-static_cast<int>(lroundf(6*cosf(a)))},STEEL);
  }
  // Long sword minute hand with a luminous center and polished edges.
  blade(c,minuteAngle,-6,78,2.8f,SILVER);
  triangle(c,point(minuteAngle,15,1),point(minuteAngle,72),point(minuteAngle,15,-1),LUME);
  // Slender seconds hand with its small luminous disc and counterweight.
  line(c,point(secondAngle,-17),point(secondAngle,83),SILVER);
  const Point pip=point(secondAngle,61),counter=point(secondAngle,-14);
  c.fillCircle(pip.x,pip.y,3,STEEL);c.fillCircle(pip.x,pip.y,2,LUME);
  c.fillCircle(counter.x,counter.y,2,SILVER);
  c.fillCircle(120,120,4,STEEL);c.fillCircle(120,120,3,SILVER);
  c.fillCircle(119,119,1,LIGHT);
}
}
