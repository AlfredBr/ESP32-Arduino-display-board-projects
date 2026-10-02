#include "BoardDisplay.h"
#include "HyperspaceConfig.h"
#include "HyperspaceAnimation.h"
#include <esp_system.h>
#include <esp_timer.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
Hyperspace::Animation hyperspace;
bool frameReady=false;
int64_t previousFrameUs=0;

void setup() {
  pinMode(HyperspaceConfig::ENABLE_PIN,OUTPUT);
  digitalWrite(HyperspaceConfig::ENABLE_PIN,HIGH);
  screen.init();
  screen.setRotation(HyperspaceConfig::ROTATION);
  screen.fillScreen(0);
  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady=frame.createSprite(240,240)!=nullptr;
  if(!frameReady) {
    frame.setColorDepth(8);
    frameReady=frame.createSprite(240,240)!=nullptr;
  }
  // The GC9A01 has no readback line. Additive lighting needs a sprite.
  if(!frameReady) {
    screen.setTextColor(0xFFFF,0);
    screen.setCursor(42,110);
    screen.print("Not enough display RAM");
    return;
  }
  hyperspace.begin(esp_random());
  previousFrameUs=esp_timer_get_time();
}

void loop() {
  if(!frameReady) { delay(100); return; }
  const int64_t now=esp_timer_get_time();
  const int64_t elapsed=now-previousFrameUs;
  if(elapsed<1000000/HyperspaceConfig::FPS) { delay(1); return; }
  previousFrameUs=now;
  hyperspace.update(elapsed/1000000.0f);
  hyperspace.draw(frame);
  frame.pushSprite(0,0);
}
