#include "BoardDisplay.h"
#include "CoinConfig.h"
#include "TouchInput.h"
#include "DiceApp.h"
#include <esp_system.h>
#include <esp_timer.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
TouchInput touch;
Dice::App app;
bool frameReady=false;
int64_t previousFrameUs=0;

uint32_t randomBelow(uint32_t bound) {
  // Reject the incomplete top/bottom interval instead of biasing a modulo roll.
  const uint32_t threshold=(0u-bound)%bound;
  uint32_t value;
  do { value=esp_random(); } while(value<threshold);
  return value%bound;
}
void setup() {
  pinMode(CoinConfig::ENABLE_PIN,OUTPUT);digitalWrite(CoinConfig::ENABLE_PIN,HIGH);
  screen.init();screen.setRotation(CoinConfig::ROTATION);screen.fillScreen(0);
  frame.setPsram(false);frame.setColorDepth(16);
  frameReady=frame.createSprite(240,240)!=nullptr;
  if(!frameReady) { frame.setColorDepth(8);frameReady=frame.createSprite(240,240)!=nullptr; }
  touch.begin();app.begin();previousFrameUs=esp_timer_get_time();
}
void loop() {
  const auto press=touch.poll(millis());
  if(press.pressed && app.tap(press.x,press.y))
    app.roll(randomBelow(app.range()),randomBelow(10),esp_random());
  const int64_t now=esp_timer_get_time(),elapsed=now-previousFrameUs;
  if(elapsed<1000000/CoinConfig::FPS) { delay(1);return; }
  previousFrameUs=now;app.update(elapsed/1000000.0f);
  lgfx::LGFXBase& canvas=frameReady?static_cast<lgfx::LGFXBase&>(frame):screen;
  app.draw(canvas);
  if(!touch.ready()) UiText::draw(canvas,"NO TOUCH",120,228,1,0xF9C7);
  if(frameReady) frame.pushSprite(0,0);
}
