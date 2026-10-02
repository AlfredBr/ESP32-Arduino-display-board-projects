#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdio.h>
#include "CoinAnimation.h"
#include "DiceRenderer.h"

namespace Dice {
class App {
  int selected=-1,page=0,first=0,second=0;
  bool rolling=false,hasResult=false;
  float elapsed=0,duration=1.8f;
  GoldCoin::Animation coin;
  static const char* label(int index) {
    static const char* const labels[]={"D4","D6","D8","D10","D12","D20","D%","COIN"};
    return labels[index];
  }
public:
  void begin() { selected=-1;page=first=second=0;rolling=hasResult=false;elapsed=0;coin.begin(); }
  int choice() const { return selected; }
  bool menu() const { return selected<0; }
  bool busy() const { return selected==7?coin.spinning():rolling; }
  int range() const {
    static const int bounds[]={4,6,8,10,12,20,10,2};
    return selected<0?1:bounds[selected];
  }
  int outcome() const {
    if(selected==7) return coin.outcome();
    if(selected==6) { const int value=first*10+second; return value?value:100; }
    return selected==3?first:first+1;
  }
  bool resultReady() const { return selected==7?coin.settled():hasResult; }
  // Returns true only when a new roll is requested. Navigation never rolls.
  bool tap(int x,int y) {
    if(selected<0) {
      if(y>=195 && y<=230 && x>=55 && x<=185) { page=1-page; return false; }
      for(int i=0;i<4;++i) {
        const int left=40+(i%2)*87,top=48+(i/2)*74;
        if(x>=left && x<left+73 && y>=top && y<top+65) {
          selected=page*4+i;first=second=0;hasResult=rolling=false;coin.begin();return false;
        }
      }
      return false;
    }
    if(x<98 && y<58) { selected=-1;rolling=false;return false; }
    return true;
  }
  void roll(unsigned a,unsigned b,uint32_t animationSeed) {
    if(selected<0) return;
    first=a%range();second=b%10;elapsed=0;hasResult=false;
    if(selected==7) { coin.flip((animationSeed&~1u)|first);return; }
    duration=1.65f+((animationSeed>>3)&255)*.35f/255;
    rolling=true;
  }
  void update(float seconds) {
    coin.update(seconds);
    if(!rolling) return;
    elapsed+=fmaxf(0,fminf(seconds,.1f));
    if(elapsed>=duration) { rolling=false;hasResult=true; }
  }
  void draw(lgfx::LGFXBase& canvas) {
    if(selected<0) {
      canvas.fillScreen(0);
      UiText::draw(canvas,"CHOOSE A DIE",120,25,1,0xEF3A);
      for(int i=0;i<4;++i) {
        const int item=page*4+i,left=40+(i%2)*87,top=48+(i/2)*74,cx=left+36;
        canvas.fillRect(left,top,73,65,0x2948);
        canvas.fillRect(left+1,top+1,71,63,0x1063);
        if(item<7) DiceRenderer::draw(canvas,item,0,cx,top+25,21,.4f,false,false);
        else {
          canvas.fillCircle(cx,top+25,18,0xB3C5);
          canvas.fillCircle(cx-1,top+24,16,0xFE6A);
          canvas.drawCircle(cx-1,top+24,13,0xA365);
          UiText::draw(canvas,"C",cx-1,top+17,2,0x9AC3);
        }
        UiText::draw(canvas,label(item),cx,top+49,1,0xF6D4);
      }
      UiText::draw(canvas,page?"< FIRST PAGE":"MORE CHOICES >",120,209,1,0xAD55);
      UiText::draw(canvas,page?"2 OF 2":"1 OF 2",120,225,1,0x6B4D);
      return;
    }
    if(selected==7) coin.draw(canvas,false);
    else {
      canvas.fillScreen(0x0842);
      const float t=rolling?fminf(1,elapsed/duration):1;
      const float motion=rolling?26*(1-t)*(1-t)*(1-t):0;
      const int cy=120-lroundf(rolling?7*sinf(t*3.14159265f):0);
      if(selected==6) {
        DiceRenderer::draw(canvas,6,first,76,cy,45,motion,true);
        DiceRenderer::draw(canvas,6,second,165,cy,45,motion*.83f,false);
        UiText::draw(canvas,"TENS",76,177,1,0xA514);
        UiText::draw(canvas,"ONES",165,177,1,0xA514);
      } else DiceRenderer::draw(canvas,selected,first,120,cy,70,motion);
    }
    UiText::draw(canvas,"< MENU",62,26,1,0xAD55);
    UiText::draw(canvas,label(selected),158,24,2,0xF6D4);
    if(selected==3) UiText::draw(canvas,"0-9",158,43,1,0x8C71);
    if(busy()) UiText::draw(canvas,selected==7?"FLIPPING":"ROLLING",120,213,1,0xD5B0);
    else if(resultReady()) {
      if(selected==7) UiText::draw(canvas,outcome()?"TAILS":"HEADS",120,204,2,0xFEAA);
      else {
        char result[5];snprintf(result,sizeof(result),"%d",outcome());
        UiText::draw(canvas,result,120,190,4,0xFF36);
      }
      UiText::draw(canvas,"TAP TO REROLL",120,225,1,0x8C71);
    } else UiText::draw(canvas,selected==7?"TAP TO FLIP":"TAP TO ROLL",120,214,1,0xAD55);
  }
};
}
