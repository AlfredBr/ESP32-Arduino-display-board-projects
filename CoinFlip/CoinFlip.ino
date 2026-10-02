#include "BoardDisplay.h"
#include "CoinConfig.h"
#include "CoinAnimation.h"
#include "TouchInput.h"
#include <esp_system.h>
#include <esp_timer.h>

BoardDisplay screen;
lgfx::LGFX_Sprite frame(&screen);
GoldCoin::Animation coin;
TouchInput touch;
bool frameReady=false;
int64_t previousFrameUs=0;

void setup() {
  Serial.begin(921600);
  Serial.setTxTimeoutMs(0);
  pinMode(CoinConfig::ENABLE_PIN,OUTPUT);
  digitalWrite(CoinConfig::ENABLE_PIN,HIGH);
  screen.init(); screen.setRotation(CoinConfig::ROTATION); screen.fillScreen(0);
  frame.setPsram(false); frame.setColorDepth(16);
  frameReady=frame.createSprite(240,240)!=nullptr;
  touch.begin(); coin.begin();
  previousFrameUs=esp_timer_get_time();
}

void loop() {
  const auto press=touch.poll(millis());
  if(press.pressed) coin.flip(esp_random());
  // Optional diagnostic query; never starts a flip or blocks waiting for USB.
  if(Serial.available() && Serial.read()=='?') {
    if(Serial.availableForWrite()>=64)
      Serial.printf("COINFLIP TOUCH=%s FRAME=%s STATE=%s\n",touch.ready()?"READY":"MISSING",
                    frameReady?"RGB565":"DIRECT",coin.spinning()?"FLIPPING":"IDLE");
  }
  const int64_t now=esp_timer_get_time(),elapsed=now-previousFrameUs;
  if(elapsed<1000000/CoinConfig::FPS) { delay(1); return; }
  previousFrameUs=now; coin.update(elapsed/1000000.0f);
  lgfx::LGFXBase& canvas=frameReady?static_cast<lgfx::LGFXBase&>(frame):screen;
  coin.draw(canvas);
  if(!touch.ready()) UiText::draw(canvas,"TOUCH NOT DETECTED",120,225,1,0xFAAA);
  if(frameReady) frame.pushSprite(0,0);
}
