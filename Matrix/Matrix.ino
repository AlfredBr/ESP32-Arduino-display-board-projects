#include "BoardDisplay.h"
#include "MatrixConfig.h"
#include "MatrixRain.h"
#include <esp_system.h>
#include <esp_timer.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
MatrixRain::Animation rain;
bool frameReady = false;
int64_t previousFrameUs = 0;

void setup() {
  pinMode(MatrixConfig::ENABLE_PIN, OUTPUT);
  digitalWrite(MatrixConfig::ENABLE_PIN, HIGH);
  screen.init();
  screen.setRotation(MatrixConfig::ROTATION);
  screen.fillScreen(0);

  frame.setPsram(false);
  frame.setColorDepth(16);
  frameReady = frame.createSprite(MatrixConfig::WIDTH, MatrixConfig::HEIGHT) != nullptr;
  if (!frameReady) {
    frame.setColorDepth(8);
    frameReady = frame.createSprite(MatrixConfig::WIDTH, MatrixConfig::HEIGHT) != nullptr;
  }
  rain.begin(esp_random());
  previousFrameUs = esp_timer_get_time();
}

void loop() {
  const int64_t now = esp_timer_get_time();
  const int64_t elapsed = now - previousFrameUs;
  if (elapsed < 1000000 / MatrixConfig::FPS) {
    delay(1);
    return;
  }
  previousFrameUs = now;
  rain.update(elapsed / 1000000.0f);
  if (frameReady) {
    rain.draw(frame);
    frame.pushSprite(0, 0);
  } else {
    screen.startWrite();
    rain.draw(screen);
    screen.endWrite();
  }
}
