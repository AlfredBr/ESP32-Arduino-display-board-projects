#ifndef LED_H
#define LED_H

#include "Arduino.h"
#include "Debug.h"

class LED {
public:
  LED(int pin) {
    pinMode(pin, OUTPUT);
    _pin = pin;
  }

  void on() {
    set(true);
  }

  void off() {
    set(false);
  }

  void high() {
    on();
  }

  void low() {
    off();
  }

  void toggle() {
    set(!value());
  }

  void set(bool value) {
    digitalWrite(_pin, value ? HIGH : LOW);
  }

  bool value() {
    return digitalRead(_pin) == HIGH;
  }

private:
  int _pin;
};

#endif