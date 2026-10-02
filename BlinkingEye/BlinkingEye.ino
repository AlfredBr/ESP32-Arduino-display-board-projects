#include <Arduino.h>
#include "BoardDisplay.h"
#include "EyeConfig.h"
#include "EyeAnimation.h"
#include <esp_system.h>
#include <esp_timer.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
BlinkingEye::Animation eye;
bool frameReady = false;
int64_t previousFrameUs = 0;

void setup() {
  pinMode(EyeConfig::ENABLE_PIN, OUTPUT);
  digitalWrite(EyeConfig::ENABLE_PIN, HIGH);
  screen.init();
  screen.setRotation(EyeConfig::ROTATION);
  screen.fillScreen(0);
  // Preserve skin/iris color with RGB565; no PSRAM required.
  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady = frame.createSprite(240, 240) != nullptr;
  eye.begin(esp_random());
  previousFrameUs = esp_timer_get_time();
}

void loop() {
  const int64_t now = esp_timer_get_time();
  const int64_t elapsed = now-previousFrameUs;
  if (elapsed < 1000000/EyeConfig::FPS) { delay(1); return; }
  previousFrameUs = now;
  eye.update(elapsed/1000000.0f);
  if (frameReady) {
    eye.draw(frame);
    frame.pushSprite(0, 0);
  } else {
    screen.startWrite();
    eye.draw(screen);
    screen.endWrite();
  }
}
