#include "BoardDisplay.h"
#include "FireworksConfig.h"
#include "FireworksAnimation.h"
#include <esp_system.h>
#include <esp_timer.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
Fireworks::Animation fireworks;
bool frameReady = false;
int64_t previousFrameUs = 0;

void setup() {
  pinMode(FireworksConfig::ENABLE_PIN, OUTPUT);
  digitalWrite(FireworksConfig::ENABLE_PIN, HIGH);
  screen.init();
  screen.setRotation(FireworksConfig::ROTATION);
  screen.fillScreen(0);
  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady = frame.createSprite(FireworksConfig::WIDTH, FireworksConfig::HEIGHT) != nullptr;
  if (!frameReady) {
    frame.setColorDepth(8);
    frameReady = frame.createSprite(FireworksConfig::WIDTH, FireworksConfig::HEIGHT) != nullptr;
  }
  fireworks.begin(esp_random());
  previousFrameUs = esp_timer_get_time();
}

void loop() {
  const int64_t now = esp_timer_get_time();
  const int64_t elapsed = now - previousFrameUs;
  if (elapsed < 1000000 / FireworksConfig::FPS) { delay(1); return; }
  previousFrameUs = now;
  fireworks.update(elapsed / 1000000.0f);
  if (frameReady) {
    fireworks.draw(frame);
    frame.pushSprite(0, 0);
  } else {
    screen.startWrite();
    fireworks.draw(screen);
    screen.endWrite();
  }
}
