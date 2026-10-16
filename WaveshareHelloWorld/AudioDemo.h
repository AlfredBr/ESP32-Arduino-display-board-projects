#pragma once
#include <Arduino.h>
#include <ESP_I2S.h>
#include <es8311.h>

// Cooperative audio: DMA reads/writes have zero timeout, never wait on USB.
class AudioDemo {
  I2SClass i2s;
  es8311_handle_t codec = nullptr;
  uint8_t* recording = nullptr;
  size_t recorded = 0, playPosition = 0;
  uint32_t toneUntil = 0, recordUntil = 0;
  unsigned phase = 0;
  int16_t tx[256] = {};
  size_t txPending = 0, txOffset = 0;
public:
  bool ready = false, capturing = false, playing = false;
  unsigned rms = 0, peak = 0;
  uint32_t lastInputMs = 0;
  const char* status = "Tap to initialize audio";
  bool begin();
  void tick();
  void stop();
  void tone();
  void record();
  void playback();
  size_t recordingBytes() const { return recorded; }
  ~AudioDemo();
};

