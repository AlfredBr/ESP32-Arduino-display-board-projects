#include "AudioDemo.h"
#include "TestConfig.h"
#include <pin_config.h>
#include <driver/i2s_std.h>
#include <esp_heap_caps.h>
#include <math.h>

AudioDemo::~AudioDemo() { if (codec) es8311_delete(codec); free(recording); }
bool AudioDemo::begin() {
  if (ready) return true;
  pinMode(PA, OUTPUT); digitalWrite(PA, LOW);
  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, AUDIO_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    status = "I2S initialization failed"; return false;
  }
  if (!codec) codec = es8311_create(0, ES8311_ADDRRES_0);
  const es8311_clock_config_t clock = {false, false, true, AUDIO_RATE * 256, AUDIO_RATE};
  if (!codec || es8311_init(codec, &clock, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK ||
      es8311_sample_frequency_config(codec, AUDIO_RATE*256, AUDIO_RATE) != ESP_OK ||
      es8311_microphone_config(codec, false) != ESP_OK ||
      es8311_voice_volume_set(codec, 35, nullptr) != ESP_OK ||
      es8311_microphone_gain_set(codec, ES8311_MIC_GAIN_18DB) != ESP_OK) {
    i2s.end(); status = "ES8311 codec init failed"; return false;
  }
  ready = true; status = "Audio ready"; return true;
}
void AudioDemo::stop() {
  toneUntil = 0; capturing = playing = false; txPending = txOffset = 0;
  digitalWrite(PA, LOW);
  if (ready) { i2s.end(); ready = false; }
  status = "Audio stopped";
}
void AudioDemo::tone() {
  if (!begin()) return;
  capturing = playing = false; toneUntil = millis() + 700;
  txPending = txOffset = 0; phase = 0;
  digitalWrite(PA, HIGH); status = "Playing 440 Hz tone";
}
void AudioDemo::record() {
  if (!begin()) return;
  if (!recording) recording = (uint8_t*)heap_caps_malloc(AUDIO_RATE*4*2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!recording) { status = "No PSRAM for recording"; return; }
  playing = false; toneUntil = 0; digitalWrite(PA, LOW);
  recorded = 0; capturing = true; recordUntil = millis()+2000;
  status = "Recording 2 seconds...";
}
void AudioDemo::playback() {
  if (!recorded) { status = "Record something first"; return; }
  if (!begin()) return;
  capturing = false; toneUntil = 0; playing = true; playPosition = 0;
  txPending = txOffset = 0; digitalWrite(PA, HIGH); status = "Playing your recording";
}
void AudioDemo::tick() {
  if (!ready) return;
  int16_t samples[256]; size_t received = 0;
  // Drain a bounded number of DMA chunks so rendering cannot starve capture.
  uint64_t energy = 0; unsigned maximum = 0; size_t count = 0;
  for (int n=0; n<8; ++n) {
    received = 0;
    i2s_channel_read(i2s.rxChan(), samples, sizeof(samples), &received, 0);
    if (!received) break;
    for (size_t j=0; j<received/2; ++j) {
      int v = samples[j]; unsigned a = abs(v); maximum = max(maximum,a);
      energy += (int64_t)v*v; ++count;
    }
    if (capturing) {
      size_t bytes = min(received, (size_t)(AUDIO_RATE*4*2-recorded));
      memcpy(recording+recorded, samples, bytes); recorded += bytes;
    }
  }
  if (count) { rms = sqrt((double)energy/count); peak = maximum; lastInputMs = millis(); }
  if (capturing && ((int32_t)(millis()-recordUntil)>=0 || recorded>=AUDIO_RATE*4*2)) {
    capturing = false; status = recorded ? "Recording ready" : "No microphone data";
  }
  if (playing) {
    size_t written = 0;
    size_t chunk = min((size_t)512, recorded-playPosition);
    i2s_channel_write(i2s.txChan(), recording+playPosition, chunk, &written, 0);
    playPosition += written;
    if (playPosition >= recorded) {
      playing = false; toneUntil = millis()+80; // Allow queued DMA audio to finish.
      status = "Playback complete";
    }
  } else if (toneUntil && (int32_t)(toneUntil-millis())>0 && strcmp(status,"Playing 440 Hz tone")==0) {
    if (!txPending) {
      for (int j=0; j<128; ++j) {
        int16_t value = 4000*sinf(2*PI*440*(phase++%AUDIO_RATE)/AUDIO_RATE);
        tx[2*j] = tx[2*j+1] = value;
      }
      txPending = sizeof(tx); txOffset=0;
    }
    size_t written=0;
    i2s_channel_write(i2s.txChan(), ((uint8_t*)tx)+txOffset, txPending, &written, 0);
    txOffset+=written; txPending-=written;
  } else if (toneUntil && (int32_t)(millis()-toneUntil)>=0) {
    toneUntil=0; digitalWrite(PA,LOW); status="Audio ready";
  }
}

