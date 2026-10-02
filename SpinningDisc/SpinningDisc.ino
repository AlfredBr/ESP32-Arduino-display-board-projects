#include "BoardDisplay.h"
#include "DiscConfig.h"
#include "DiscAnimation.h"
#include <esp_timer.h>
#include <esp_system.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
SpinningDisc::Animation disc;
bool frameReady = false;
int64_t previousFrameUs = 0;

void setup() {
  pinMode(DiscConfig::ENABLE_PIN,OUTPUT);
  digitalWrite(DiscConfig::ENABLE_PIN,HIGH);
  screen.init();
  screen.setRotation(DiscConfig::ROTATION);
  screen.fillScreen(0);
  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady = frame.createSprite(240,240) != nullptr;
  disc.begin(esp_random());
  previousFrameUs = esp_timer_get_time();
}

void loop() {
  const int64_t now = esp_timer_get_time();
  const int64_t elapsed = now-previousFrameUs;
  if (elapsed < 1000000/DiscConfig::FPS) { delay(1); return; }
  previousFrameUs = now;
  disc.update(elapsed/1000000.0f);
  if (frameReady) {
    disc.draw(frame);
    frame.pushSprite(0,0);
  } else {
    screen.startWrite();
    disc.draw(screen);
    screen.endWrite();
  }
}
