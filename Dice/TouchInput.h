#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "CoinConfig.h"

// CST816D pinout and register map from the supplied factory touch example.
// Every transfer is bounded; absent touch hardware cannot freeze the sketch.
class TouchInput {
  static constexpr uint8_t ADDRESS=0x15;
  uint32_t lastPoll=0, lastRetry=0, releasedAt=0;
  bool available=false, held=false, releasing=false;
  bool writeRegister(uint8_t reg,uint8_t value) {
    Wire.beginTransmission(ADDRESS); Wire.write(reg); Wire.write(value);
    return Wire.endTransmission()==0;
  }
public:
  struct Press { uint16_t x=0,y=0; bool pressed=false; };
  void begin() {
    pinMode(CoinConfig::TOUCH_INTERRUPT,INPUT_PULLUP);
    pinMode(CoinConfig::TOUCH_RESET,OUTPUT);
    digitalWrite(CoinConfig::TOUCH_RESET,LOW); delay(10);
    digitalWrite(CoinConfig::TOUCH_RESET,HIGH); delay(300);
    Wire.begin(CoinConfig::TOUCH_SDA,CoinConfig::TOUCH_SCL,100000);
    Wire.setTimeOut(12);
    available=writeRegister(0xFE,0xFF); // Disable automatic sleep.
  }
  bool ready() const { return available; }
  Press poll(uint32_t now) {
    Press result;
    if(now-lastPoll<8) return result;
    lastPoll=now;
    if(!available) {
      if(now-lastRetry>=1000) { lastRetry=now; available=writeRegister(0xFE,0xFF); }
      return result;
    }
    Wire.beginTransmission(ADDRESS); Wire.write(0x02);
    if(Wire.endTransmission(false)!=0) { available=false; return result; }
    if(Wire.requestFrom(ADDRESS,static_cast<uint8_t>(5))!=5) {
      while(Wire.available()) Wire.read();
      available=false; return result;
    }
    const uint8_t fingers=Wire.read()&0x0F;
    const uint8_t xh=Wire.read(),xl=Wire.read(),yh=Wire.read(),yl=Wire.read();
    if(fingers) {
      releasing=false;
      if(!held) {
        held=true;
        int x=((xh&15)<<8)|xl, y=((yh&15)<<8)|yl;
        if(x>=240 || y>=240) return result;
        if(CoinConfig::ROTATION==1) { const int oldX=x; x=y; y=239-oldX; }
        else if(CoinConfig::ROTATION==2) { x=239-x; y=239-y; }
        else if(CoinConfig::ROTATION==3) { const int oldX=x; x=239-y; y=oldX; }
        result={static_cast<uint16_t>(x),static_cast<uint16_t>(y),true};
      }
    } else if(held) {
      if(!releasing) { releasing=true; releasedAt=now; }
      else if(now-releasedAt>=40) { held=false; releasing=false; }
    }
    return result;
  }
};
