#pragma once
#include <stdint.h>
#include <stdlib.h>

// One navigation event per contact; release debounce separates taps from swipes.
class SwipeGesture {
public:
  enum Event { NONE, NEXT, PREVIOUS, TAP };
  bool down=false;
  int x=-1,y=-1;
  Event sample(bool pressed,int px,int py,uint32_t now) {
    if(pressed) {
      x=px;y=py;lastContact=now;
      if(!down){down=true;startX=x;startY=y;started=now;consumed=false;maxDx=maxDy=0;}
      int dx=x-startX,dy=y-startY;
      if(abs(dx)>maxDx)maxDx=abs(dx);if(abs(dy)>maxDy)maxDy=abs(dy);
      if(!consumed && abs(dx)>=65 && abs(dx)>abs(dy)*3/2) {
        consumed=true;return dx<0?NEXT:PREVIOUS;
      }
    } else if(down && now-lastContact>=30) {
      down=false;
      if(!consumed && maxDx<20 && maxDy<20 && now-started<700)return TAP;
    }
    return NONE;
  }
private:
  int startX=0,startY=0,maxDx=0,maxDy=0;
  uint32_t started=0,lastContact=0;
  bool consumed=false;
};
