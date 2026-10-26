#include <Arduino.h>
#include <math.h>
#include "BoardDisplay.h"
#include "RingConfig.h"

// Galileo's Ring II: pendulums in identical groups. Pendulum k completes
// (baseOsc + k % groupSize) swings per cycle, so all balls realign every
// CYCLE_MS and show symmetric patterns between.
// Optional looks (see Build.ps1): GALILEO_TRAILS, GALILEO_RAINBOW,
// GALILEO_BLOOM, GALILEO_CARTESIAN, GALILEO_MIRROR, GALILEO_WAVE,
// GALILEO_AUTO (preset show), GALILEO_STATS (frame timing on serial).
//
// The ESP32-C3 has no FPU, so the per-frame path is integer only: swing angles
// are 16-bit "turns" (65536 = one full revolution) and cos() is a Q10 table.
static BoardDisplay lcd;
static LGFX_Sprite canvas(&lcd);
static uint32_t lastFrame = 0;
static uint32_t simTime = 0;
static bool paused = false;

struct RGB { uint8_t r, g, b; };
constexpr float TWO_PI_F = 6.28318530718f;
constexpr int BACKLIGHT_PIN = 3;
constexpr int LUT_SIZE = 1024;

// Float config value -> fixed point with 8 fractional bits (a 0..1 factor becomes 0..256).
constexpr int q8(float v) { return int(v * 256.0f + 0.5f); }

// Runtime scene state: Auto changes these between presets.
#ifdef GALILEO_CARTESIAN
static bool cartesianView = true;
#else
static bool cartesianView = false;
#endif
static int groups = RingConfig::DEFAULT_GROUPS;
static int groupSize = RingConfig::COUNT / RingConfig::DEFAULT_GROUPS;
static int baseOsc = RingConfig::BASE_OSC;

static int16_t cosLut[LUT_SIZE];                                // cos * 1024
static int16_t dirXq[RingConfig::COUNT], dirYq[RingConfig::COUNT]; // radial unit spokes, Q10
static int cartXi[RingConfig::COUNT];                           // Cartesian columns
static RGB tint[RingConfig::COUNT];
static int bx[RingConfig::COUNT], by[RingConfig::COUNT];        // this frame's ball positions
static uint16_t bAngle[RingConfig::COUNT];                      // swing angle in turns
static int cartCYq, cartAmpi;
#ifdef GALILEO_TRAILS
static int trailFade[RingConfig::TRAIL_STEPS + 1];              // segment brightness, 0..256
static uint32_t trailTurns[RingConfig::TRAIL_STEPS + 1];        // cycle phase j steps ago, in turns
#endif

#ifdef GALILEO_AUTO
struct Preset { bool cartesian; uint8_t groups; uint8_t baseOsc; };
static const Preset presets[] = {
  {false, 3, 12}, {true, 3, 12}, {false, 2, 10},
  {true, 4, 10},  {false, 4, 12}, {true, 2, 14},
};
constexpr int PRESET_COUNT = sizeof(presets) / sizeof(presets[0]);
static int currentPreset = -1;
static int lastBacklight = -1;
#endif

#ifdef GALILEO_STATS
static uint32_t secUs[7], secMark;
#define SECTION(i) do { uint32_t n_ = micros(); secUs[i] += n_ - secMark; secMark = n_; } while (0)
#else
#define SECTION(i) do {} while (0)
#endif

static inline int cosQ(uint16_t turns) { return cosLut[turns >> 6]; }
static inline int sinQ(uint16_t turns) { return cosLut[((turns >> 6) - LUT_SIZE / 4) & (LUT_SIZE - 1)]; }
// sin^2 in Q10 (0..1024): 1 at peak speed (mid-swing), 0 at the turning points.
static inline int speedQ(uint16_t turns) { int s = sinQ(turns); return (s * s) >> 10; }

static RGB hueToRgb(uint16_t turns) {
  uint32_t h6 = uint32_t(turns) * 6;
  int sector = h6 >> 16;
  uint8_t up = (h6 & 0xFFFF) >> 8, dn = 255 - up;
  switch (sector) {
    case 0: return {255, up, 0};
    case 1: return {dn, 255, 0};
    case 2: return {0, 255, up};
    case 3: return {0, dn, 255};
    case 4: return {up, 0, 255};
    default: return {255, 0, dn};
  }
}

// Colour of pendulum k when its swing angle is `turns`.
static inline RGB baseColor(int k, uint16_t turns) {
#ifdef GALILEO_RAINBOW
  (void)k;
  return hueToRgb(turns);
#else
  (void)turns;
  return tint[k];
#endif
}

// f256: brightness factor, 0..256.
static inline uint16_t shade(RGB c, int f256) {
  return lcd.color565(uint8_t((c.r * f256) >> 8), uint8_t((c.g * f256) >> 8), uint8_t((c.b * f256) >> 8));
}

static inline int swingsOf(int k) { return baseOsc + (k % groupSize); }
static inline bool mirrorActive() { return RingConfig::MIRROR && cartesianView; }
static inline int mirrorY(int y) { return int(2.0f * RingConfig::MIRROR_FLOOR_Y) - y; }

static void ballPosition(int k, uint16_t turns, int &x, int &y) {
  int s = cosQ(turns); // cos() starts every pendulum at maximum swing
  if (cartesianView) {
    x = cartXi[k];
    y = (cartCYq + cartAmpi * s + 512) >> 10;
  } else {
    constexpr int centerQ = int(RingConfig::CENTER * 1024.0f);
    int r = int(RingConfig::MID_R) * 1024 + int(RingConfig::AMPLITUDE) * s; // Q10 radius
    x = (centerQ + ((dirXq[k] * r) >> 10) + 512) >> 10;
    y = (centerQ + ((dirYq[k] * r) >> 10) + 512) >> 10;
  }
}

static void applyLayout(bool cartesian, int newGroups, int newBaseOsc) {
  cartesianView = cartesian;
  groups = newGroups;
  groupSize = RingConfig::COUNT / groups;
  baseOsc = newBaseOsc;
  for (int k = 0; k < RingConfig::COUNT; k++) {
    // Slight tint shift per group position gives a depth cue.
    int t = (k % groupSize) * 48 / groupSize; // 0..47 across a group
    tint[k] = {150, uint8_t(190 + t), uint8_t(225 - t)};
  }
}

static uint32_t cycleTurns(uint32_t ms) { return (ms % RingConfig::CYCLE_MS) * 65536u / RingConfig::CYCLE_MS; }

static void drawScene() {
#ifdef GALILEO_STATS
  secMark = micros();
#endif
  const int c = int(RingConfig::CENTER);
  const uint32_t phase = cycleTurns(simTime);
  const bool mirror = mirrorActive();
  canvas.fillScreen(TFT_BLACK);

  for (int k = 0; k < RingConfig::COUNT; k++) {
    bAngle[k] = uint16_t(phase * swingsOf(k));
    ballPosition(k, bAngle[k], bx[k], by[k]);
  }

  SECTION(0);
  if (cartesianView) {
    int x0 = cartXi[0], w = cartXi[RingConfig::COUNT - 1] - cartXi[0];
    if (mirror) canvas.drawFastHLine(x0, int(RingConfig::MIRROR_FLOOR_Y), w, RingConfig::FLOOR_COLOR);
    else canvas.drawFastHLine(x0, c, w, RingConfig::RIM_COLOR);
  } else {
    for (int k = 0; k < RingConfig::COUNT; k++)
      canvas.fillCircle((c * 1024 + ((dirXq[k] * int(RingConfig::RIM_R)))) >> 10,
                        (c * 1024 + ((dirYq[k] * int(RingConfig::RIM_R)))) >> 10, 1, RingConfig::RIM_COLOR);
  }

  SECTION(1);
#ifdef GALILEO_TRAILS
  // Trails are replayed analytically from earlier phases, oldest segment first.
  for (int j = 0; j <= RingConfig::TRAIL_STEPS; j++)
    trailTurns[j] = cycleTurns(simTime + RingConfig::CYCLE_MS - j * RingConfig::TRAIL_STEP_MS);
  for (int k = 0; k < RingConfig::COUNT; k++) {
    int swings = swingsOf(k);
    int px = 0, py = 0;
    for (int j = RingConfig::TRAIL_STEPS; j >= 0; j--) {
      uint16_t angle = uint16_t(trailTurns[j] * swings);
      int x, y;
      ballPosition(k, angle, x, y);
      if (j < RingConfig::TRAIL_STEPS) {
        RGB col = baseColor(k, angle);
        canvas.drawLine(px, py, x, y, shade(col, trailFade[j]));
        if (mirror) canvas.drawLine(px, mirrorY(py), x, mirrorY(y), shade(col, (trailFade[j] * q8(RingConfig::MIRROR_FADE)) >> 8));
      }
      px = x; py = y;
    }
  }
#endif

  SECTION(2);
#ifdef GALILEO_WAVE
  // Ribbon through neighbouring balls: open in the row view, a closed loop radially.
  for (int k = 0; k < RingConfig::COUNT; k++) {
    int n = k + 1;
    if (n == RingConfig::COUNT) {
      if (cartesianView) break;
      n = 0;
    }
    RGB col = baseColor(k, bAngle[k]);
    canvas.drawLine(bx[k], by[k], bx[n], by[n], shade(col, q8(RingConfig::WAVE_BRIGHTNESS)));
    if (mirror)
      canvas.drawLine(bx[k], mirrorY(by[k]), bx[n], mirrorY(by[n]),
                      shade(col, (q8(RingConfig::WAVE_BRIGHTNESS) * q8(RingConfig::MIRROR_FADE)) >> 8));
  }
#endif

  SECTION(3);
#ifdef GALILEO_BLOOM
  // Speed peaks as a ball crosses mid-swing; halos go first so neighbouring
  // balls are never overpainted.
  for (int k = 0; k < RingConfig::COUNT; k++) {
    int f = (q8(RingConfig::BLOOM_HALO) * speedQ(bAngle[k])) >> 10;
    canvas.fillCircle(bx[k], by[k], RingConfig::BALL_R + 2, shade(baseColor(k, bAngle[k]), f));
  }
#endif

  SECTION(4);
  if (mirror) {
    for (int k = 0; k < RingConfig::COUNT; k++)
      canvas.fillCircle(bx[k], mirrorY(by[k]), RingConfig::BALL_R, shade(baseColor(k, bAngle[k]), q8(RingConfig::MIRROR_FADE)));
  }

  for (int k = 0; k < RingConfig::COUNT; k++) {
    RGB col = baseColor(k, bAngle[k]);
#ifdef GALILEO_BLOOM
    int w = (q8(RingConfig::BLOOM_WHITE) * speedQ(bAngle[k])) >> 10; // blend toward white at peak speed
    col = {uint8_t(col.r + (((255 - col.r) * w) >> 8)), uint8_t(col.g + (((255 - col.g) * w) >> 8)),
           uint8_t(col.b + (((255 - col.b) * w) >> 8))};
#else
    if (RingConfig::BALL_GLOW) canvas.fillCircle(bx[k], by[k], RingConfig::BALL_R + 1, RingConfig::GLOW_COLOR);
#endif
    canvas.fillCircle(bx[k], by[k], RingConfig::BALL_R, shade(col, 256));
  }

  if (!cartesianView) canvas.fillCircle(c, c, RingConfig::HUB_R, RingConfig::HUB_COLOR);
  SECTION(5);
  canvas.pushSprite(0, 0);
#ifdef GALILEO_STATS
  secUs[6] += micros() - secMark;
#endif
}

#ifdef GALILEO_AUTO
static void applyPreset(int index) {
  currentPreset = index;
  const Preset &p = presets[index];
  applyLayout(p.cartesian, p.groups, p.baseOsc);
  Serial.printf("Preset %d/%d: %s view, %d groups, base %d swings\n", index + 1, PRESET_COUNT,
                p.cartesian ? "row" : "ring", p.groups, p.baseOsc);
}

// Dip the backlight at preset boundaries so the layout swap happens in the dark.
static void updateAuto() {
  uint32_t t = simTime % RingConfig::AUTO_PRESET_MS;
  int index = int((simTime / RingConfig::AUTO_PRESET_MS) % PRESET_COUNT);
  if (index != currentPreset) applyPreset(index);
  float level = 1.0f;
  if (t < RingConfig::AUTO_FADE_MS) level = float(t) / RingConfig::AUTO_FADE_MS;
  else if (t > RingConfig::AUTO_PRESET_MS - RingConfig::AUTO_FADE_MS)
    level = float(RingConfig::AUTO_PRESET_MS - t) / RingConfig::AUTO_FADE_MS;
  int duty = int(255.0f * level * level + 0.5f); // squared for a perceptually even fade
  if (duty != lastBacklight) { lastBacklight = duty; analogWrite(BACKLIGHT_PIN, duty); }
}

static void nextPreset() {
  simTime = (simTime / RingConfig::AUTO_PRESET_MS + 1) * RingConfig::AUTO_PRESET_MS;
}
#endif

void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT); digitalWrite(BACKLIGHT_PIN, HIGH);
  lcd.init(); lcd.setRotation(0); lcd.setSwapBytes(true);
  lcd.fillScreen(TFT_BLACK);
  canvas.setColorDepth(16);
  if (!canvas.createSprite(240, 240)) {
    Serial.println("Galileo's Ring: sprite allocation failed.");
    while (true) delay(1000);
  }
  for (int i = 0; i < LUT_SIZE; i++) cosLut[i] = int16_t(lroundf(1024.0f * cosf(TWO_PI_F * i / LUT_SIZE)));
  const float halfWidth = RingConfig::MIRROR ? RingConfig::MIRROR_HALF_WIDTH : RingConfig::CART_HALF_WIDTH;
  cartCYq = int((RingConfig::MIRROR ? RingConfig::MIRROR_CENTER_Y : RingConfig::CENTER) * 1024.0f);
  cartAmpi = int(RingConfig::MIRROR ? RingConfig::MIRROR_AMPLITUDE : RingConfig::CART_AMPLITUDE);
  for (int k = 0; k < RingConfig::COUNT; k++) {
    cartXi[k] = int(RingConfig::CENTER - halfWidth + 2.0f * halfWidth * k / (RingConfig::COUNT - 1) + 0.5f);
    float a = TWO_PI_F * k / RingConfig::COUNT - 1.57079632679f; // pendulum 0 at top
    dirXq[k] = int16_t(lroundf(1024.0f * cosf(a)));
    dirYq[k] = int16_t(lroundf(1024.0f * sinf(a)));
  }
#ifdef GALILEO_TRAILS
  for (int j = 0; j < RingConfig::TRAIL_STEPS; j++) {
    // Quadratic falloff keeps the head of the trail bright and the tail subtle.
    float f = RingConfig::TRAIL_BRIGHTNESS * (1.0f - float(j) / RingConfig::TRAIL_STEPS);
    trailFade[j] = q8(f * f);
  }
#endif
  applyLayout(cartesianView, groups, baseOsc);
#ifdef GALILEO_AUTO
  updateAuto();
#endif
  drawScene(); lastFrame = millis();
  Serial.printf("Galileo's Ring II: %d pendulums. SPACE pauses/resumes%s.\n", RingConfig::COUNT,
#ifdef GALILEO_AUTO
                ", N skips to the next preset"
#else
                ""
#endif
  );
}

void loop() {
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch == ' ') paused = !paused;
#ifdef GALILEO_AUTO
    else if (ch == 'n' || ch == 'N') nextPreset();
#endif
  }
  uint32_t now = millis();
  if (now - lastFrame >= RingConfig::FRAME_MS) {
    uint32_t delta = now - lastFrame; lastFrame = now;
    if (!paused) {
      simTime += delta;
#ifdef GALILEO_AUTO
      updateAuto();
#endif
#ifdef GALILEO_STATS
      uint32_t t0 = micros();
#endif
      drawScene();
#ifdef GALILEO_STATS
      static uint32_t sum = 0, worst = 0, frames = 0, reportAt = 0;
      uint32_t took = micros() - t0;
      sum += took; frames++;
      if (took > worst) worst = took;
      if (now - reportAt >= 10000) {
        Serial.printf("draw avg %lu us, worst %lu us over %lu frames\n",
                      (unsigned long)(sum / frames), (unsigned long)worst, (unsigned long)frames);
        Serial.printf("  us/frame: clear+pos %lu, rim %lu, trails %lu, wave %lu, bloom %lu, balls %lu, push %lu\n",
                      (unsigned long)(secUs[0]/frames),(unsigned long)(secUs[1]/frames),(unsigned long)(secUs[2]/frames),
                      (unsigned long)(secUs[3]/frames),(unsigned long)(secUs[4]/frames),(unsigned long)(secUs[5]/frames),
                      (unsigned long)(secUs[6]/frames));
        for (auto &u : secUs) u = 0;
        sum = worst = frames = 0; reportAt = now;
      }
#endif
    }
  }
  delay(1);
}
