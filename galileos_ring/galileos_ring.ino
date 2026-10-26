#include <Arduino.h>
#include <math.h>
#include "BoardDisplay.h"
#include "RingConfig.h"

// Galileo's Ring II seen from above: 48 radial pendulums in three identical
// groups of 16. Pendulum k completes (BASE_OSC + k % 16) swings per cycle, so
// all balls realign every CYCLE_MS and show 3-fold symmetric patterns between.
static BoardDisplay lcd;
static LGFX_Sprite canvas(&lcd);
static uint32_t lastFrame = 0;
static uint32_t simTime = 0;
static bool paused = false;
static float dirX[RingConfig::COUNT], dirY[RingConfig::COUNT];
static uint16_t ballColor[RingConfig::COUNT];

static void drawScene() {
  const float c = RingConfig::CENTER;
  const float phaseBase = 6.28318530718f * float(simTime % RingConfig::CYCLE_MS) / RingConfig::CYCLE_MS;
  canvas.fillScreen(TFT_BLACK);
  // Pivot ring: the rest marks at the rim.
  for (int k = 0; k < RingConfig::COUNT; k++)
    canvas.fillCircle(int(c + dirX[k] * RingConfig::RIM_R + 0.5f),
                      int(c + dirY[k] * RingConfig::RIM_R + 0.5f), 1, RingConfig::RIM_COLOR);
  // Ball offset along its spoke; cos() starts every pendulum at maximum swing.
  for (int k = 0; k < RingConfig::COUNT; k++) {
    int swings = RingConfig::BASE_OSC + (k % RingConfig::GROUP);
    float r = RingConfig::MID_R + RingConfig::AMPLITUDE * cosf(phaseBase * swings);
    int x = int(c + dirX[k] * r + 0.5f), y = int(c + dirY[k] * r + 0.5f);
    canvas.fillCircle(x, y, 3, RingConfig::GLOW_COLOR);
    canvas.fillCircle(x, y, 2, ballColor[k]);
  }
  canvas.fillCircle(int(c), int(c), RingConfig::HUB_R, RingConfig::HUB_COLOR);
  canvas.pushSprite(0, 0);
}

void setup() {
  Serial.begin(115200);
  pinMode(3, OUTPUT); digitalWrite(3, HIGH);
  lcd.init(); lcd.setRotation(0); lcd.setSwapBytes(true);
  lcd.fillScreen(TFT_BLACK);
  canvas.setColorDepth(16);
  if (!canvas.createSprite(240, 240)) {
    Serial.println("Galileo's Ring: sprite allocation failed.");
    while (true) delay(1000);
  }
  for (int k = 0; k < RingConfig::COUNT; k++) {
    float a = 6.28318530718f * k / RingConfig::COUNT - 1.57079632679f; // pendulum 0 at top
    dirX[k] = cosf(a); dirY[k] = sinf(a);
    // Hue-free tint per group position gives a slight depth cue.
    uint8_t g = 190 + (k % RingConfig::GROUP) * 4;
    ballColor[k] = lcd.color565(150, g, 225 - (k % RingConfig::GROUP) * 3);
  }
  drawScene(); lastFrame = millis();
  Serial.println("Galileo's Ring II: 48 pendulums. Send SPACE to pause/resume.");
}

void loop() {
  while (Serial.available()) if (Serial.read() == ' ') paused = !paused;
  uint32_t now = millis();
  if (now - lastFrame >= RingConfig::FRAME_MS) {
    uint32_t delta = now - lastFrame; lastFrame = now;
    if (!paused) { simTime += delta; drawScene(); }
  }
  delay(1);
}
