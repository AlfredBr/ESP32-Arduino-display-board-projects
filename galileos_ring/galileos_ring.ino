#include <Arduino.h>
#include <math.h>
#include "BoardDisplay.h"
#include "RingConfig.h"

// Galileo's Ring II: radial pendulums in three identical groups. Pendulum k
// completes (BASE_OSC + k % GROUP) swings per cycle, so all balls realign every
// CYCLE_MS and show 3-fold symmetric patterns between.
// Optional looks (see Build.ps1): GALILEO_TRAILS, GALILEO_RAINBOW,
// GALILEO_BLOOM, GALILEO_CARTESIAN (side-to-side row instead of radial spokes).
static BoardDisplay lcd;
static LGFX_Sprite canvas(&lcd);
static uint32_t lastFrame = 0;
static uint32_t simTime = 0;
static bool paused = false;

struct RGB { uint8_t r, g, b; };
// Radial: unit spoke direction. Cartesian: dirX is the column x, dirY unused.
static float dirX[RingConfig::COUNT], dirY[RingConfig::COUNT];
static RGB tint[RingConfig::COUNT];
#ifdef GALILEO_TRAILS
static float trailFade[RingConfig::TRAIL_STEPS]; // brightness of segment j steps behind the ball
#endif

constexpr float TWO_PI_F = 6.28318530718f;

static RGB hueToRgb(float h) {
  h -= floorf(h);
  h *= 6.0f;
  int i = int(h);
  uint8_t up = uint8_t(255.0f * (h - i)), dn = 255 - up;
  switch (i) {
    case 0: return {255, up, 0};
    case 1: return {dn, 255, 0};
    case 2: return {0, 255, up};
    case 3: return {0, dn, 255};
    case 4: return {up, 0, 255};
    default: return {255, 0, dn};
  }
}

// Colour of pendulum k when its swing angle is `angle`.
static RGB baseColor(int k, float angle) {
#ifdef GALILEO_RAINBOW
  return hueToRgb(angle / TWO_PI_F);
#else
  (void)angle;
  return tint[k];
#endif
}

static uint16_t shade(RGB c, float f) {
  return lcd.color565(uint8_t(c.r * f), uint8_t(c.g * f), uint8_t(c.b * f));
}

static void ballPosition(int k, float angle, int &x, int &y) {
  float s = cosf(angle); // cos() starts every pendulum at maximum swing
#ifdef GALILEO_CARTESIAN
  x = int(dirX[k] + 0.5f);
  y = int(RingConfig::CENTER + RingConfig::CART_AMPLITUDE * s + 0.5f);
#else
  float r = RingConfig::MID_R + RingConfig::AMPLITUDE * s;
  x = int(RingConfig::CENTER + dirX[k] * r + 0.5f);
  y = int(RingConfig::CENTER + dirY[k] * r + 0.5f);
#endif
}

static void drawScene() {
  const float c = RingConfig::CENTER;
  const float phaseBase = TWO_PI_F * float(simTime % RingConfig::CYCLE_MS) / RingConfig::CYCLE_MS;
  canvas.fillScreen(TFT_BLACK);

#ifdef GALILEO_CARTESIAN
  canvas.drawFastHLine(int(dirX[0]), int(c), int(dirX[RingConfig::COUNT - 1] - dirX[0]), RingConfig::RIM_COLOR);
#else
  for (int k = 0; k < RingConfig::COUNT; k++)
    canvas.fillCircle(int(c + dirX[k] * RingConfig::RIM_R + 0.5f),
                      int(c + dirY[k] * RingConfig::RIM_R + 0.5f), 1, RingConfig::RIM_COLOR);
#endif

#ifdef GALILEO_TRAILS
  // Trails are replayed analytically from earlier phases, oldest segment first.
  const float stepPhase = TWO_PI_F * RingConfig::TRAIL_STEP_MS / RingConfig::CYCLE_MS;
  for (int k = 0; k < RingConfig::COUNT; k++) {
    int swings = RingConfig::BASE_OSC + (k % RingConfig::GROUP);
    int px = 0, py = 0;
    for (int j = RingConfig::TRAIL_STEPS; j >= 0; j--) {
      float angle = (phaseBase - stepPhase * j) * swings;
      int x, y;
      ballPosition(k, angle, x, y);
      if (j < RingConfig::TRAIL_STEPS) canvas.drawLine(px, py, x, y, shade(baseColor(k, angle), trailFade[j]));
      px = x; py = y;
    }
  }
#endif

#ifdef GALILEO_BLOOM
  // Speed peaks as a ball crosses mid-swing (|sin| = 1); halos go first so
  // neighbouring balls are never overpainted.
  for (int k = 0; k < RingConfig::COUNT; k++) {
    float angle = phaseBase * (RingConfig::BASE_OSC + (k % RingConfig::GROUP));
    float s = sinf(angle);
    int x, y;
    ballPosition(k, angle, x, y);
    canvas.fillCircle(x, y, RingConfig::BALL_R + 2, shade(baseColor(k, angle), RingConfig::BLOOM_HALO * s * s));
  }
#endif

  for (int k = 0; k < RingConfig::COUNT; k++) {
    float angle = phaseBase * (RingConfig::BASE_OSC + (k % RingConfig::GROUP));
    int x, y;
    ballPosition(k, angle, x, y);
    RGB col = baseColor(k, angle);
#ifdef GALILEO_BLOOM
    float s = sinf(angle);
    float w = RingConfig::BLOOM_WHITE * s * s; // blend toward white at peak speed
    col = {uint8_t(col.r + (255 - col.r) * w), uint8_t(col.g + (255 - col.g) * w), uint8_t(col.b + (255 - col.b) * w)};
#else
    if (RingConfig::BALL_GLOW) canvas.fillCircle(x, y, RingConfig::BALL_R + 1, RingConfig::GLOW_COLOR);
#endif
    canvas.fillCircle(x, y, RingConfig::BALL_R, shade(col, 1.0f));
  }

#ifndef GALILEO_CARTESIAN
  canvas.fillCircle(int(c), int(c), RingConfig::HUB_R, RingConfig::HUB_COLOR);
#endif
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
#ifdef GALILEO_CARTESIAN
    dirX[k] = RingConfig::CENTER - RingConfig::CART_HALF_WIDTH +
              2.0f * RingConfig::CART_HALF_WIDTH * k / (RingConfig::COUNT - 1);
    dirY[k] = 0;
#else
    float a = TWO_PI_F * k / RingConfig::COUNT - 1.57079632679f; // pendulum 0 at top
    dirX[k] = cosf(a); dirY[k] = sinf(a);
#endif
    // Slight tint shift per group position gives a depth cue.
    int t = (k % RingConfig::GROUP) * 48 / RingConfig::GROUP; // 0..47 across a group
    tint[k] = {150, uint8_t(190 + t), uint8_t(225 - t)};
  }
#ifdef GALILEO_TRAILS
  for (int j = 0; j < RingConfig::TRAIL_STEPS; j++) {
    // Quadratic falloff keeps the head of the trail bright and the tail subtle.
    float f = RingConfig::TRAIL_BRIGHTNESS * (1.0f - float(j) / RingConfig::TRAIL_STEPS);
    trailFade[j] = f * f;
  }
#endif
  drawScene(); lastFrame = millis();
  Serial.printf("Galileo's Ring II: %d pendulums. Send SPACE to pause/resume.\n", RingConfig::COUNT);
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
