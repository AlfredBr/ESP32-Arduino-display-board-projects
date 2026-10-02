#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "DialTexture.h"

namespace BreitlingFace {
constexpr float PI_F=3.14159265358979323846f;
// Keep live overlays aligned with the edge-to-edge crop of the dial texture.
inline int mapX(float x) { return lroundf(x*DialTexture::SCALE+DialTexture::OFFSET_X); }
inline int mapY(float y) { return lroundf(y*DialTexture::SCALE+DialTexture::OFFSET_Y); }
inline const char* weekday(int day) {
  static const char* const names[]={"SU","MO","TU","WE","TH","FR","SA"};
  return day>=0 && day<7?names[day]:"--";
}
inline uint16_t segments(char c) {
  // A B C D E F G-left G-right upper-left upper-center upper-right
  // lower-left lower-center lower-right: fourteen LCD segments.
  constexpr uint16_t A=1<<0,B=1<<1,C=1<<2,D=1<<3,E=1<<4,F=1<<5;
  constexpr uint16_t G=(1<<6)|(1<<7),H=1<<8,I=1<<9,J=1<<10,K=1<<11,L=1<<12,M=1<<13;
  switch(c) {
    case '0':case 'O':return A|B|C|D|E|F;
    case '1':return B|C;
    case '2':return A|B|G|E|D;
    case '3':return A|B|C|D|G;
    case '4':return F|G|B|C;
    case '5':case 'S':return A|F|G|C|D;
    case '6':return A|F|G|E|C|D;
    case '7':return A|B|C;
    case '8':return A|B|C|D|E|F|G;
    case '9':return A|B|C|D|F|G;
    case 'U':return F|E|D|C|B;
    case 'M':return F|E|B|C|H|J;
    case 'T':return A|I|L;
    case 'W':return F|E|B|C|K|M;
    case 'E':return A|F|G|E|D;
    case 'H':return F|E|G|B|C;
    case 'F':return A|F|E|G;
    case 'R':return A|F|E|B|G|M;
    case 'A':return A|B|C|E|F|G;
    default:return G;
  }
}
inline void lcdGlyph(lgfx::LGFXBase& canvas,char letter,int x,int y) {
  constexpr int endpoints[14][4]={
    {2,0,9,0},{11,2,11,8},{11,12,11,18},{2,20,9,20},
    {0,12,0,18},{0,2,0,8},{2,10,5,10},{7,10,9,10},
    {2,2,5,8},{6,2,6,8},{10,2,7,8},
    {2,18,5,12},{6,12,6,18},{10,18,7,12}
  };
  const uint16_t active=segments(letter);
  for(int i=0;i<14;++i) {
    const bool on=active&(1<<i);
    const uint16_t color=on?0x8C0D:0x2123;
    const auto& s=endpoints[i];
    canvas.drawLine(mapX(x+s[0]),mapY(y+s[1]),mapX(x+s[2]),mapY(y+s[3]),color);
    if(on && i<8) canvas.drawLine(mapX(x+s[0]+1),mapY(y+s[1]),mapX(x+s[2]+1),mapY(y+s[3]),color);
  }
}
inline void hand(lgfx::LGFXBase& canvas,float angle,float length,float width,uint16_t fill) {
  const float sx=sinf(angle),sy=-cosf(angle),px=cosf(angle),py=sinf(angle);
  const int tx=mapX(120+sx*length),ty=mapY(120+sy*length);
  const int bx=mapX(120-sx*8),by=mapY(120-sy*8);
  const int lx=mapX(120+sx*length*.15f+px*width),ly=mapY(120+sy*length*.15f+py*width);
  const int rx=mapX(120+sx*length*.15f-px*width),ry=mapY(120+sy*length*.15f-py*width);
  canvas.fillTriangle(bx+1,by+1,lx+1,ly+1,tx+1,ty+1,0x1082);
  canvas.fillTriangle(bx+1,by+1,rx+1,ry+1,tx+1,ty+1,0x1082);
  canvas.fillTriangle(bx,by,lx,ly,tx,ty,fill);
  canvas.fillTriangle(bx,by,rx,ry,tx,ty,fill);
  canvas.drawLine(bx,by,lx,ly,0xBCA9);canvas.drawLine(lx,ly,tx,ty,0xFF33);
  canvas.drawLine(tx,ty,rx,ry,0xBCA9);canvas.drawLine(rx,ry,bx,by,0x9B86);
  canvas.drawLine(mapX(120),mapY(120),mapX(120+sx*length*.87f),mapY(120+sy*length*.87f),0xEF16);
}
inline void draw(lgfx::LGFXBase& canvas,const tm& clock,bool timeReady) {
  canvas.setSwapBytes(true);
  canvas.pushImage(0,0,240,240,DialTexture::PIXELS);
  const char* day=timeReady?weekday(clock.tm_wday):"--";
  char date[3]="--";
  if(timeReady) snprintf(date,sizeof(date),"%02d",clock.tm_mday);
  // Only the lower LCD is active; the upper LCD remains blank in the artwork.
  lcdGlyph(canvas,day[0],78,151);lcdGlyph(canvas,day[1],92,151);
  lcdGlyph(canvas,date[0],134,151);lcdGlyph(canvas,date[1],148,151);
  if(timeReady) {
    const float minute=clock.tm_min+clock.tm_sec/60.0f;
    const float hour=(clock.tm_hour%12)+minute/60.0f;
    hand(canvas,hour*PI_F/6,51,2.8f,0xDE70);
    hand(canvas,minute*PI_F/30,77,1.9f,0xF733);
    canvas.fillCircle(mapX(120),mapY(120),3,0xBCA9);
    canvas.fillCircle(mapX(119),mapY(119),1,0xFF53);
  }
}
}
