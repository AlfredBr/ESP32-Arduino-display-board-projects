#include "BoardDisplay.h"
#include "HamsterConfig.h"
#include "HamsterAnimation.h"
#include <esp_system.h>
#include <esp_timer.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
Hamster::Animation hamster;
bool frameReady = false;
int64_t previousFrameUs = 0;

void setup() {
  pinMode(HamsterConfig::ENABLE_PIN, OUTPUT);
  digitalWrite(HamsterConfig::ENABLE_PIN, HIGH);
  screen.init();
  screen.setRotation(HamsterConfig::ROTATION);
  screen.fillScreen(0);
  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady = frame.createSprite(240, 240) != nullptr;
  if (!frameReady) {
    frame.setColorDepth(8);
    frameReady = frame.createSprite(240, 240) != nullptr;
  }
  // Compositing requires a readable framebuffer; the panel has no MISO line.
  if (!frameReady) {
    screen.setTextColor(0xFFFF, 0);
    screen.setCursor(42, 110);
    screen.print("Not enough display RAM");
    return;
  }
  hamster.begin(esp_random());
  previousFrameUs = esp_timer_get_time();
}

void loop() {
  if (!frameReady) { delay(100); return; }
  const int64_t now = esp_timer_get_time();
  const int64_t elapsed = now-previousFrameUs;
  if (elapsed < 1000000/HamsterConfig::FPS) { delay(1); return; }
  previousFrameUs = now;
  hamster.update(elapsed/1000000.0f);
  hamster.draw(frame);
  frame.pushSprite(0, 0);
}
