#pragma once
#include <Arduino.h>
struct WtxtMessage {
  char value[513] = "";
  char timestamp[65] = "";
  char status[80] = "Waiting for Wi-Fi/time";
  bool hasValue = false;
  uint32_t updatedMs = 0;
};
void beginWtxt();
void requestWtxt();
void readWtxt(WtxtMessage& result);
bool wtxtBusy();
