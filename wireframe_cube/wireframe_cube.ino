#include "BoardDisplay.h"
#include "CubeAnimation.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>

BoardDisplay screen;
Cube::Animation cube;
uint16_t* frame = nullptr;
int64_t previousFrameUs = 0, reportUs = 0;
uint32_t frameCount = 0;
int64_t renderTotalUs = 0, transferTotalUs = 0;

void setup() {
  Serial.begin(921600);
  Serial.setTxTimeoutMs(0);
  if (!screen.begin()) {
    Serial.println("Cube: display initialization failed");
    return;
  }
  const size_t bytes = CubeConfig::WIDTH * CubeConfig::HEIGHT * sizeof(uint16_t);
  frame = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!frame) frame = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!frame) {
    screen.graphics().setTextColor(0xFFFF);
    screen.graphics().setCursor(40, 200);
    screen.graphics().print("Not enough display RAM");
    Serial.println("Cube: framebuffer allocation failed");
    return;
  }
  cube.draw(frame);
  screen.present(frame, cube.shape());
  previousFrameUs = reportUs = esp_timer_get_time();
  Serial.printf("Cube: ready, ESP32-S3, %dx%d RGB565, PSRAM=%u, target=%d FPS\n",
    CubeConfig::WIDTH, CubeConfig::HEIGHT, static_cast<unsigned>(ESP.getPsramSize()), CubeConfig::FPS);
}

void loop() {
  if (!frame) { delay(100); return; }
  const int64_t now = esp_timer_get_time();
  const int64_t elapsed = now - previousFrameUs;
  if (elapsed < 1000000 / CubeConfig::FPS) { delay(1); return; }
  previousFrameUs = now;
  cube.update(elapsed / 1000000.0f);
  cube.draw(frame);
  const int64_t rendered = esp_timer_get_time();
  screen.present(frame, cube.shape());
  const int64_t presented = esp_timer_get_time();
  renderTotalUs += rendered - now;
  transferTotalUs += presented - rendered;
  ++frameCount;
  if (presented - reportUs >= 5000000) {
    Serial.printf("Cube: %.1f FPS, render=%.2f ms, transfer=%.2f ms, free=%u\n",
      frameCount * 1000000.0 / (presented - reportUs),
      renderTotalUs / (1000.0 * frameCount), transferTotalUs / (1000.0 * frameCount),
      static_cast<unsigned>(ESP.getFreeHeap()));
    reportUs = presented;
    frameCount = 0;
    renderTotalUs = transferTotalUs = 0;
  }
  delay(1);
}
