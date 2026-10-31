#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <driver/i2s_std.h>
#include <es8311.h>
#include <lvgl.h>
#include "SensorQMI8658.hpp"
#include "BoardDisplay.h"
#include "LevelMath.h"
using namespace level;

BoardDisplay lcd;
SensorQMI8658 imu;
alignas(LV_DRAW_BUF_ALIGN) static uint16_t pixels[LCD_WIDTH * 40];
static constexpr uint8_t touchAddress = 0x15;
static bool touchPresent = false, imuPresent = false;

static constexpr int32_t cx = LCD_WIDTH / 2, cy = 242, half = 160;
static constexpr int32_t boardLeft = cx - half, boardRight = cx + half;
static constexpr int32_t boardTop = cy - half, boardBottom = cy + half;
static constexpr int32_t baseBallRadius = 13, maxBallRadius = 27, hubRadius = 42, monsterRadius = 12;
static constexpr float accelerationGain = 700.0f, drag = 1.8f, maxSpeed = 230.0f;
static constexpr uint32_t cBackground = 0x0B1116, cBlue = 0x1687F8,
                          cOrange = 0xFF653D, cRail = 0xE7EDF2,
                          cFrame = 0x9EADB6, cText = 0xE8EEF2,
                          cDim = 0x82919B, cGold = 0xF5C84C,
                          cGoal = 0x75F0A5, cRed = 0xFF5A4E,
                          cMonster = 0xC84E73, cEye = 0xFFF2D8;

enum class ToyMode : uint8_t { Maze, Pinwheel, Balance, Monsters, Tunnel };
enum class Sound : uint8_t { Caught, Chime, Scare, Sad };
static constexpr const char *modeNames[] = {
  "MAZE", "PINWHEEL", "CENTER BALANCE", "MONSTER DODGE", "TUNNEL SWIM"
};
static constexpr const char *modeHints[] = {
  "Find the way through the labyrinth to the green goal",
  "Drift the bead around the four colors",
  "Keep the bead inside the center ring",
  "Dodge monsters crossing from every side",
  "Stay between the waves; avoid top and bottom"
};
static constexpr int32_t mazeN = 8, mazeCell = 40;
static constexpr uint32_t soundRate = 16000, soundFrames = 12000, catchEffectMs = 700;
static constexpr uint8_t maxMonsters = 6;
static constexpr uint8_t tunnelSegments = 20;
static constexpr uint8_t tunnelEyes = 4;
static constexpr int32_t gemRadius = 9;
static constexpr uint32_t gemLifetimeMs = 5200;
static constexpr uint32_t cTunnel = 0x100F19, cTunnelWall = 0x302238,
                          cTunnelShadow = 0x211727, cTunnelEdge = 0xA6D84F,
                          cTunnelFang = 0xB9AA87, cTunnelEye = 0xD5ED86,
                          cTunnelPupil = 0xE94E62, cGem = 0x28BFFF,
                          cGemHighlight = 0xB7F5FF;
static constexpr int32_t goalX[] = {boardLeft + 31, boardRight - 31, boardRight - 31, boardLeft + 31};
static constexpr int32_t goalY[] = {boardTop + 31, boardTop + 31, boardBottom - 31, boardBottom - 31};

static lv_obj_t *titleLabel, *hintLabel, *footerLabel, *frame;
static lv_obj_t *ball, *goalOuter, *goalInner, *balanceRing;
struct Monster {
  lv_obj_t *body, *eyeA, *eyeB;
  float x, y, vx, vy;
  uint32_t spawnAt;
  bool active;
};
static Monster monsters[maxMonsters];
static ToyMode mode = ToyMode::Maze;
static Vec3 fastUp{0, 0, 1};
static float ballX = cx, ballY = cy, velocityX = 0, velocityY = 0;
static int32_t ballRadius = baseBallRadius;
static uint32_t lastSample = 0, lastPhysics = 0, lastHud = 0, lastStream = 0;
static uint32_t balanceMs = 0, goalReachedAt = 0, monsterStartedAt = 0, monsterSurvivalMs = 0;
static uint32_t tunnelStartedAt = 0, tunnelSurvivalMs = 0;
static uint32_t gemSpawnAt = 0, gemExpiresAt = 0;
static uint16_t gemsCollected = 0, gemsMissed = 0;
static float gemX = 0, gemY = 0;
static uint8_t mazeGoals = 0;
static bool mazeH[mazeN + 1][mazeN], mazeV[mazeN][mazeN + 1];
static int32_t mazeGoalX = cx, mazeGoalY = cy, mazeStartX = cx, mazeStartY = cy;
static bool haveSample = false, streaming = false, monsterGameOver = false;
static bool tunnelGameOver = false;
static bool gemActive = false;
static Vec3 rawAcc;
static uint32_t randomState = 1;
static lv_obj_t *catchFlash, *catchRing;
static uint32_t catchAt = 0;
static I2SClass i2s;
static int16_t soundBuffer[2 * soundFrames];
static size_t soundPos = 0, soundBytes = 0;
static uint32_t soundIdleAt = 0;
static bool soundReady = false, soundFailed = false, soundPlaying = false;
static float tunnelScroll = 0, tunnelPhaseA = 0, tunnelPhaseB = 0,
             tunnelPhaseC = 0, tunnelRate = 0.7f, tunnelDrift = 0.45f;

static lv_obj_t *makeBox(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                         int32_t radius, uint32_t color, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(obj, w, h);
  lv_obj_set_pos(obj, x - w / 2, y - h / 2);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, opa, 0);
  return obj;
}

static void drawTriangle(lv_layer_t *layer, lv_point_precise_t a, lv_point_precise_t b,
                         lv_point_precise_t c, uint32_t color) {
  lv_draw_triangle_dsc_t dsc;
  lv_draw_triangle_dsc_init(&dsc);
  dsc.p[0] = a; dsc.p[1] = b; dsc.p[2] = c;
  dsc.color = lv_color_hex(color);
  dsc.opa = LV_OPA_COVER;
  lv_draw_triangle(layer, &dsc);
}

static void drawLine(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                     uint32_t color, int32_t width) {
  lv_draw_line_dsc_t dsc;
  lv_draw_line_dsc_init(&dsc);
  dsc.p1 = {x1, y1}; dsc.p2 = {x2, y2};
  dsc.color = lv_color_hex(color);
  dsc.width = width;
  dsc.round_start = dsc.round_end = 1;
  lv_draw_line(layer, &dsc);
}

static float tunnelCenter(float y);
static float tunnelHalfWidth(float y);
static float tunnelTooth(float y);

static void drawCircle(lv_layer_t *layer, int32_t x, int32_t y, uint16_t radius,
                       int32_t width, uint32_t color) {
  lv_draw_arc_dsc_t dsc;
  lv_draw_arc_dsc_init(&dsc);
  dsc.center = {x, y};
  dsc.radius = radius;
  dsc.width = width;
  dsc.start_angle = 0;
  dsc.end_angle = 360;
  dsc.color = lv_color_hex(color);
  dsc.opa = LV_OPA_COVER;
  lv_draw_arc(layer, &dsc);
}

static void drawDiamond(lv_layer_t *layer, int32_t x, int32_t y) {
  lv_point_precise_t top{x, y - gemRadius}, right{x + gemRadius, y};
  lv_point_precise_t bottom{x, y + gemRadius}, left{x - gemRadius, y};
  lv_point_precise_t center{x, y};
  drawTriangle(layer, top, right, center, cGemHighlight);
  drawTriangle(layer, top, center, left, cGem);
  drawTriangle(layer, left, center, bottom, cGem);
  drawTriangle(layer, center, right, bottom, cBlue);
  drawLine(layer, x - 3, y - 4, x, y - 7, cGemHighlight, 2);
}

static void drawBoard(lv_event_t *event) {
  lv_layer_t *layer = lv_event_get_layer(event);
  if (mode == ToyMode::Maze) {
    for (int32_t r = 0; r <= mazeN; ++r) {
      for (int32_t c = 0; c < mazeN; ++c) {
        if (mazeH[r][c]) {
          int32_t y = boardTop + r * mazeCell, x = boardLeft + c * mazeCell;
          drawLine(layer, x, y, x + mazeCell, y, cBlue, 4);
        }
      }
    }
    for (int32_t r = 0; r < mazeN; ++r) {
      for (int32_t c = 0; c <= mazeN; ++c) {
        if (mazeV[r][c]) {
          int32_t x = boardLeft + c * mazeCell, y = boardTop + r * mazeCell;
          drawLine(layer, x, y, x, y + mazeCell, cOrange, 4);
        }
      }
    }
    return;
  }
  if (mode == ToyMode::Tunnel) {
    drawTriangle(layer, {boardLeft, boardTop}, {boardRight, boardTop},
                 {boardRight, boardBottom}, cTunnel);
    drawTriangle(layer, {boardLeft, boardTop}, {boardRight, boardBottom},
                 {boardLeft, boardBottom}, cTunnel);
    for (uint8_t i = 0; i < tunnelSegments; ++i) {
      int32_t y1 = boardTop + (boardBottom - boardTop) * i / tunnelSegments;
      int32_t y2 = boardTop + (boardBottom - boardTop) * (i + 1) / tunnelSegments;
      int32_t left1 = lroundf(tunnelCenter(y1) - tunnelHalfWidth(y1));
      int32_t left2 = lroundf(tunnelCenter(y2) - tunnelHalfWidth(y2));
      int32_t right1 = lroundf(tunnelCenter(y1) + tunnelHalfWidth(y1));
      int32_t right2 = lroundf(tunnelCenter(y2) + tunnelHalfWidth(y2));
      uint32_t wallColor = (i / 2) % 2 ? cTunnelWall : cTunnelShadow;
      drawTriangle(layer, {boardLeft, y1}, {left1, y1}, {left2, y2}, wallColor);
      drawTriangle(layer, {boardLeft, y1}, {left2, y2}, {boardLeft, y2}, wallColor);
      drawTriangle(layer, {right1, y1}, {boardRight, y1}, {boardRight, y2}, wallColor);
      drawTriangle(layer, {right1, y1}, {boardRight, y2}, {right2, y2}, wallColor);
      drawLine(layer, left1, y1, left2, y2, cTunnelEdge, 2);
      drawLine(layer, right1, y1, right2, y2, cTunnelEdge, 2);
    }

    float toothOffset = tunnelPhaseC * 7.0f;
    int32_t firstTooth = static_cast<int32_t>(floorf((boardTop + tunnelScroll + toothOffset) / 56.0f));
    int32_t lastTooth = static_cast<int32_t>(ceilf((boardBottom + tunnelScroll + toothOffset) / 56.0f));
    for (int32_t i = firstTooth; i <= lastTooth; ++i) {
      float y = i * 56.0f - tunnelScroll - toothOffset;
      if (y - 9 < boardTop || y + 9 > boardBottom) continue;
      int32_t leftTop = lroundf(tunnelCenter(y - 9) - tunnelHalfWidth(y - 9));
      int32_t leftTip = lroundf(tunnelCenter(y) - tunnelHalfWidth(y));
      int32_t leftBottom = lroundf(tunnelCenter(y + 9) - tunnelHalfWidth(y + 9));
      int32_t rightTop = lroundf(tunnelCenter(y - 9) + tunnelHalfWidth(y - 9));
      int32_t rightTip = lroundf(tunnelCenter(y) + tunnelHalfWidth(y));
      int32_t rightBottom = lroundf(tunnelCenter(y + 9) + tunnelHalfWidth(y + 9));
      drawTriangle(layer, {leftTop, lroundf(y - 9)}, {leftTip, lroundf(y)},
                   {leftBottom, lroundf(y + 9)}, cTunnelFang);
      drawTriangle(layer, {rightTop, lroundf(y - 9)}, {rightTip, lroundf(y)},
                   {rightBottom, lroundf(y + 9)}, cTunnelFang);
    }

    for (uint8_t i = 0; i < tunnelEyes; ++i) {
      float y = boardTop + 42.0f + i * 72.0f - fmodf(tunnelScroll * 0.75f, 72.0f);
      if (y < boardTop + 18 || y > boardBottom - 18) continue;
      int32_t leftEye = lroundf(tunnelCenter(y) - tunnelHalfWidth(y) - 13);
      int32_t rightEye = lroundf(tunnelCenter(y) + tunnelHalfWidth(y) + 13);
      int32_t eyeY = lroundf(y);
      drawCircle(layer, leftEye, eyeY, 7, 7, cTunnelEye);
      drawCircle(layer, leftEye + 2, eyeY, 3, 3, cTunnelPupil);
      drawCircle(layer, rightEye, eyeY, 7, 7, cTunnelEye);
      drawCircle(layer, rightEye - 2, eyeY, 3, 3, cTunnelPupil);
    }
    if (gemActive) drawDiamond(layer, lroundf(gemX), lroundf(gemY));
    return;
  }

  lv_point_precise_t center{cx, cy};
  drawTriangle(layer, {boardLeft, boardTop}, {boardRight, boardTop}, center, cBlue);
  drawTriangle(layer, {boardRight, boardTop}, {boardRight, boardBottom}, center, cOrange);
  drawTriangle(layer, {boardRight, boardBottom}, {boardLeft, boardBottom}, center, cBlue);
  drawTriangle(layer, {boardLeft, boardBottom}, {boardLeft, boardTop}, center, cOrange);

  for (int i = 0; i < 4; ++i) {
    int32_t x = goalX[i], y = goalY[i];
    if (mode == ToyMode::Maze) {
      float dx = static_cast<float>(x - cx), dy = static_cast<float>(y - cy);
      float length = sqrtf(dx * dx + dy * dy);
      int32_t endX = cx + lroundf(dx * hubRadius / length);
      int32_t endY = cy + lroundf(dy * hubRadius / length);
      drawLine(layer, x, y, endX, endY, cRail, 3);
    } else {
      drawLine(layer, x, y, cx, cy, cRail, 2);
    }
  }
}

static void placeLabel(lv_obj_t *label, int32_t y, int32_t width) {
  int32_t height = lv_font_get_line_height(lv_obj_get_style_text_font(label, LV_PART_MAIN));
  lv_obj_set_width(label, width);
  lv_obj_set_pos(label, (LCD_WIDTH - width) / 2, y - height / 2);
}

static void setText(lv_obj_t *label, const char *text) {
  if (strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
}

static void positionBall() {
  lv_obj_set_pos(ball, lroundf(ballX) - ballRadius, lroundf(ballY) - ballRadius);
}

static void resetBall() {
  ballX = cx; ballY = cy;
  velocityX = velocityY = 0;
  positionBall();
}

static void positionGoal() {
  lv_obj_set_pos(goalOuter, mazeGoalX - 15, mazeGoalY - 15);
  lv_obj_set_pos(goalInner, mazeGoalX - 7, mazeGoalY - 7);
}

static uint32_t nextRandom() {
  randomState ^= randomState << 13;
  randomState ^= randomState >> 17;
  randomState ^= randomState << 5;
  return randomState;
}

static bool &mazeWall(int32_t r, int32_t c, uint8_t dir) {
  if (dir == 0) return mazeH[r][c];
  if (dir == 1) return mazeH[r + 1][c];
  if (dir == 2) return mazeV[r][c];
  return mazeV[r][c + 1];
}

static void generateMaze() {
  static constexpr int8_t dr[] = {-1, 1, 0, 0}, dc[] = {0, 0, -1, 1};
  memset(mazeH, 1, sizeof mazeH);
  memset(mazeV, 1, sizeof mazeV);
  bool visited[mazeN][mazeN] = {};
  int8_t stack[mazeN * mazeN];
  int32_t top = 0;
  int32_t startR = (nextRandom() & 1) ? mazeN - 1 : 0, startC = (nextRandom() & 1) ? mazeN - 1 : 0;
  stack[top++] = startR * mazeN + startC;
  visited[startR][startC] = true;
  while (top) {
    int32_t r = stack[top - 1] / mazeN, c = stack[top - 1] % mazeN;
    uint8_t options[4], count = 0;
    for (uint8_t d = 0; d < 4; ++d) {
      int32_t nr = r + dr[d], nc = c + dc[d];
      if (nr >= 0 && nr < mazeN && nc >= 0 && nc < mazeN && !visited[nr][nc]) options[count++] = d;
    }
    if (!count) { --top; continue; }
    uint8_t d = options[nextRandom() % count];
    mazeWall(r, c, d) = false;
    int32_t nr = r + dr[d], nc = c + dc[d];
    visited[nr][nc] = true;
    stack[top++] = nr * mazeN + nc;
  }

  // The goal is the cell farthest from the start by walking distance.
  int16_t dist[mazeN * mazeN];
  for (int16_t &value : dist) value = -1;
  int8_t queue[mazeN * mazeN];
  int32_t head = 0, tail = 0, farthest = startR * mazeN + startC;
  dist[farthest] = 0;
  queue[tail++] = farthest;
  while (head < tail) {
    int32_t cell = queue[head++], r = cell / mazeN, c = cell % mazeN;
    if (dist[cell] > dist[farthest]) farthest = cell;
    for (uint8_t d = 0; d < 4; ++d) {
      int32_t nr = r + dr[d], nc = c + dc[d];
      if (nr < 0 || nr >= mazeN || nc < 0 || nc >= mazeN || mazeWall(r, c, d)) continue;
      int32_t next = nr * mazeN + nc;
      if (dist[next] >= 0) continue;
      dist[next] = dist[cell] + 1;
      queue[tail++] = next;
    }
  }
  mazeStartX = boardLeft + startC * mazeCell + mazeCell / 2;
  mazeStartY = boardTop + startR * mazeCell + mazeCell / 2;
  mazeGoalX = boardLeft + (farthest % mazeN) * mazeCell + mazeCell / 2;
  mazeGoalY = boardTop + (farthest / mazeN) * mazeCell + mazeCell / 2;
  ballX = mazeStartX; ballY = mazeStartY;
  velocityX = velocityY = 0;
  positionGoal();
  positionBall();
  lv_obj_invalidate(lv_screen_active());
}

static bool soundBegin() {
  if (soundReady || soundFailed) return soundReady;
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);
  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, soundRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO,
                 I2S_STD_SLOT_BOTH)) {
    soundFailed = true;
    return false;
  }
  es8311_handle_t codec = es8311_create(0, ES8311_ADDRRES_0);
  const es8311_clock_config_t clock = {false, false, true, soundRate * 256, soundRate};
  if (!codec || es8311_init(codec, &clock, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK ||
      es8311_sample_frequency_config(codec, soundRate * 256, soundRate) != ESP_OK ||
      es8311_voice_volume_set(codec, 70, nullptr) != ESP_OK) {
    i2s.end();
    soundFailed = true;
    return false;
  }

  soundReady = true;
  return true;
}

static int16_t soundSample(float value, float envelope) {
  return static_cast<int16_t>(constrain(value * envelope, -1.0f, 1.0f) * 14000.0f);
}

// Sounds are synthesized into the shared buffer right before playback.
static uint32_t soundRender(Sound sound) {
  float phase = 0, phase2 = 0;
  uint32_t noise = 12345;
  uint32_t frames = soundFrames;
  if (sound == Sound::Chime) frames = 7200;
  if (sound == Sound::Sad) frames = 9600;
  for (uint32_t i = 0; i < frames; ++i) {
    float t = static_cast<float>(i) / soundRate;
    noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
    float grit = (static_cast<int32_t>(noise & 0xFFFF) - 32768) / 32768.0f;
    int16_t sample;
    if (sound == Sound::Chime) {
      // Three quick rising notes (A5, E6, A6) with a bell-like overtone.
      static constexpr float notes[] = {880.0f, 1319.0f, 1760.0f};
      uint32_t index = static_cast<uint32_t>(t / 0.09f);
      if (index > 2) index = 2;
      float local = t - index * 0.09f;
      phase += 2.0f * PI * notes[index] / soundRate;
      float value = sinf(phase) + 0.35f * sinf(phase * 2.0f);
      float envelope = expf(-(index == 2 ? 6.0f : 18.0f) * local) * min(local * 800.0f, 1.0f) * 0.7f;
      sample = soundSample(value, envelope);
    } else if (sound == Sound::Sad) {
      // A sad "wah-wah" slide down a minor third (E4 to C4) with a wobbling vibrato.
      float freq = t < 0.28f ? 330.0f - 25.0f * (t / 0.28f) : 262.0f - 40.0f * ((t - 0.28f) / 0.32f);
      freq *= 1.0f + 0.015f * sinf(t * 40.0f);
      phase += 2.0f * PI * freq / soundRate;
      float value = sinf(phase) + 0.3f * sinf(phase * 2.0f) + 0.12f * sinf(phase * 3.0f);
      float local = t < 0.28f ? t : t - 0.28f;
      float envelope = min(local * 120.0f, 1.0f) * expf(-(t < 0.28f ? 2.0f : 4.0f) * local) * 0.7f;
      sample = soundSample(value, envelope);
    } else if (sound == Sound::Scare) {
      // A rising-then-falling shriek over a dissonant tritone drone and hissing noise.
      float shriek = 500.0f + 1500.0f * sinf(PI * min(t / 0.75f, 1.0f)) + 60.0f * sinf(t * 55.0f);
      phase += 2.0f * PI * shriek / soundRate;
      phase2 += 2.0f * PI * (shriek * 1.414f) / soundRate;
      float drone = sinf(2.0f * PI * 82.0f * t) + sinf(2.0f * PI * 116.0f * t);
      float screech = (sinf(phase) >= 0 ? 0.5f : -0.5f) + 0.4f * sinf(phase2);
      float value = 0.55f * screech + 0.25f * drone + 0.25f * grit;
      float envelope = min(t * 120.0f, 1.0f) * (1.0f - 0.7f * t / 0.75f) * (0.8f + 0.2f * sinf(t * 90.0f));
      sample = soundSample(value, envelope);
    } else {
      // A descending, growling sweep over a noise thump: the "caught!" sound.
      float freq = 60.0f + 520.0f * expf(-3.4f * t) * (1.0f + 0.12f * sinf(t * 70.0f));
      phase += 2.0f * PI * freq / soundRate;
      float square = sinf(phase) >= 0 ? 1.0f : -1.0f;
      float value = 0.55f * square + 0.45f * grit * expf(-9.0f * t) + 0.35f * grit * 0.3f;
      sample = soundSample(value, expf(-3.2f * t) * min(t * 400.0f, 1.0f));
    }
    soundBuffer[2 * i] = soundBuffer[2 * i + 1] = sample;
  }
  return frames;
}

static void soundPlay(Sound sound) {
  if (!soundBegin()) return;
  soundBytes = soundRender(sound) * 2 * sizeof(int16_t);
  soundPos = 0;
  soundPlaying = true;
  soundIdleAt = 0;
  digitalWrite(PA, HIGH);
}

static void soundTick(uint32_t now) {
  if (soundPlaying) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(soundBuffer);
    size_t written = 0;
    i2s_channel_write(i2s.txChan(), bytes + soundPos, soundBytes - soundPos, &written, 0);
    soundPos += written;
    if (soundPos >= soundBytes) {
      soundPlaying = false;
      soundIdleAt = now + 250;
    }
  } else if (soundIdleAt && static_cast<int32_t>(now - soundIdleAt) >= 0) {
    soundIdleAt = 0;
    digitalWrite(PA, LOW);
  }
}

static void clearCatchEffect() {
  catchAt = 0;
  if (!catchFlash) return;
  lv_obj_add_flag(catchFlash, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(catchRing, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(ball, lv_color_hex(cGold), 0);
}

static void startCatchEffect(uint32_t now) {
  catchAt = now;
  lv_obj_set_style_bg_color(ball, lv_color_hex(cRed), 0);
  lv_obj_clear_flag(catchFlash, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(catchRing, LV_OBJ_FLAG_HIDDEN);
  soundPlay(Sound::Caught);
}

// Red screen flash, an expanding shockwave ring and a short bead shake.
static void catchEffectTick(uint32_t now) {
  if (!catchAt) return;
  uint32_t t = now - catchAt;
  if (t >= catchEffectMs) {
    lv_obj_add_flag(catchFlash, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(catchRing, LV_OBJ_FLAG_HIDDEN);
    catchAt = 0;
    positionBall();
    return;
  }
  float progress = static_cast<float>(t) / catchEffectMs;
  lv_obj_set_style_bg_opa(catchFlash, static_cast<lv_opa_t>(150.0f * (1.0f - progress)), 0);
  int32_t size = ballRadius * 2 + static_cast<int32_t>(progress * 300.0f);
  lv_obj_set_size(catchRing, size, size);
  lv_obj_set_style_border_opa(catchRing, static_cast<lv_opa_t>(255.0f * (1.0f - progress)), 0);
  int32_t jx = 0, jy = 0;
  if (progress < 0.5f) {
    int32_t amplitude = static_cast<int32_t>(7.0f * (1.0f - progress * 2.0f)) + 1;
    jx = static_cast<int32_t>(nextRandom() % (2 * amplitude + 1)) - amplitude;
    jy = static_cast<int32_t>(nextRandom() % (2 * amplitude + 1)) - amplitude;
  }
  lv_obj_set_pos(catchRing, lroundf(ballX) - size / 2, lroundf(ballY) - size / 2);
  lv_obj_set_pos(ball, lroundf(ballX) - ballRadius + jx, lroundf(ballY) - ballRadius + jy);
}

static float tunnelCenter(float y) {
  float distance = y + tunnelScroll;
  float center = cx
    + 25.0f * sinf(distance * 0.020f + tunnelPhaseA)
    + 15.0f * sinf(distance * 0.043f + tunnelPhaseB)
    + 8.0f * sinf(distance * 0.071f + tunnelPhaseC + tunnelScroll * tunnelDrift * 0.018f);
  return constrain(center, cx - 48.0f, cx + 48.0f);
}

static float tunnelHalfWidth(float y) {
  float distance = y + tunnelScroll;
  float width = 87.0f
    + 20.0f * sinf(distance * 0.014f + tunnelPhaseB)
    + 14.0f * sinf(distance * 0.037f + tunnelPhaseC)
    + 9.0f * sinf(distance * 0.061f + tunnelPhaseA + tunnelScroll * tunnelDrift * 0.014f);
  width -= tunnelTooth(y);
  return constrain(width, 44.0f, 105.0f);
}

static float tunnelTooth(float y) {
  float phase = fmodf(y + tunnelScroll + tunnelPhaseC * 7.0f, 56.0f);
  if (phase < 0) phase += 56.0f;
  float distance = min(phase, 56.0f - phase);
  return 14.0f * max(0.0f, 1.0f - distance / 9.0f);
}

static void scheduleGem(uint32_t now) {
  gemActive = false;
  gemSpawnAt = now + 2400U + nextRandom() % 2800U;
}

static void spawnGem(uint32_t now) {
  // Try anywhere in the water, away from the bead, and clear of the fangs.
  for (uint8_t attempt = 0; attempt < 8; ++attempt) {
    float y = boardTop + 40.0f + nextRandom() % static_cast<uint32_t>(boardBottom - boardTop - 80);
    float halfWidth = min(tunnelHalfWidth(y), min(tunnelHalfWidth(y - 14), tunnelHalfWidth(y + 14)));
    int32_t available = static_cast<int32_t>(halfWidth) - ballRadius - gemRadius - 6;
    if (available < 1) continue;
    int32_t offset = static_cast<int32_t>(nextRandom() % (2 * available + 1)) - available;
    float x = tunnelCenter(y) + offset;
    float dx = x - ballX, dy = y - ballY;
    if (dx * dx + dy * dy < 90.0f * 90.0f) continue;
    gemX = x;
    gemY = y;
    gemExpiresAt = now + gemLifetimeMs;
    gemActive = true;
    return;
  }
  gemSpawnAt = now + 200;
}

static void missGem() {
  ++gemsMissed;
  soundPlay(Sound::Sad);
  if (ballRadius < maxBallRadius) {
    ++ballRadius;
    lv_obj_set_size(ball, ballRadius * 2, ballRadius * 2);
    positionBall();
  }
}

static void positionMonster(Monster &monster) {
  int32_t x = lroundf(monster.x), y = lroundf(monster.y);
  lv_obj_set_pos(monster.body, x - monsterRadius, y - monsterRadius);
  if (fabsf(monster.vy) > fabsf(monster.vx)) {
    int32_t eyeY = y + (monster.vy > 0 ? -2 : 2);
    lv_obj_set_pos(monster.eyeA, x - 7, eyeY - 3);
    lv_obj_set_pos(monster.eyeB, x + 1, eyeY - 3);
  } else {
    int32_t eyeX = x + (monster.vx > 0 ? -2 : 2);
    lv_obj_set_pos(monster.eyeA, eyeX - 3, y - 7);
    lv_obj_set_pos(monster.eyeB, eyeX - 3, y + 1);
  }
}

static void spawnMonster(Monster &monster, uint32_t now, uint8_t lane) {
  int32_t laneMin = boardLeft + 28, laneMax = boardRight - 28;
  int32_t lanePosition = laneMin + nextRandom() % (laneMax - laneMin + 1);
  uint8_t direction = (lane + nextRandom() % 4) % 4;
  uint32_t rampSeconds = monsterSurvivalMs / 1000;
  float speed = 82.0f + (rampSeconds < 60 ? rampSeconds : 60) * 1.1f;
  monster.vx = monster.vy = 0;
  if (direction == 0) {
    monster.x = lanePosition; monster.y = boardTop - monsterRadius;
    monster.vy = speed;
  } else if (direction == 1) {
    monster.x = lanePosition; monster.y = boardBottom + monsterRadius;
    monster.vy = -speed;
  } else if (direction == 2) {
    monster.x = boardLeft - monsterRadius; monster.y = lanePosition;
    monster.vx = speed;
  } else {
    monster.x = boardRight + monsterRadius; monster.y = lanePosition;
    monster.vx = -speed;
  }
  monster.active = true;
  monster.spawnAt = now;
  lv_obj_clear_flag(monster.body, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(monster.eyeA, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(monster.eyeB, LV_OBJ_FLAG_HIDDEN);
  positionMonster(monster);
}

static void startMonsterGame(uint32_t now) {
  monsterStartedAt = now;
  monsterSurvivalMs = 0;
  monsterGameOver = false;
  clearCatchEffect();
  resetBall();
  for (uint8_t i = 0; i < maxMonsters; ++i) {
    monsters[i].active = false;
    monsters[i].spawnAt = now + 700U * i;
    lv_obj_add_flag(monsters[i].body, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(monsters[i].eyeA, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(monsters[i].eyeB, LV_OBJ_FLAG_HIDDEN);
  }
  setText(hintLabel, modeHints[static_cast<uint8_t>(ToyMode::Monsters)]);
}

static void startTunnelGame(uint32_t now) {
  tunnelStartedAt = now;
  tunnelSurvivalMs = 0;
  tunnelScroll = 0;
  tunnelGameOver = false;
  gemActive = false;
  gemsCollected = gemsMissed = 0;
  gemSpawnAt = now + 2400U + nextRandom() % 2800U;
  ballRadius = baseBallRadius;
  lv_obj_set_size(ball, ballRadius * 2, ballRadius * 2);
  tunnelPhaseA = (nextRandom() % 6283) * 0.001f;
  tunnelPhaseB = (nextRandom() % 6283) * 0.001f;
  tunnelPhaseC = (nextRandom() % 6283) * 0.001f;
  tunnelRate = 0.65f + (nextRandom() % 1000) * 0.0007f;
  tunnelDrift = 0.30f + (nextRandom() % 1000) * 0.0005f;
  resetBall();
  setText(hintLabel, modeHints[static_cast<uint8_t>(ToyMode::Tunnel)]);
  lv_obj_invalidate(lv_screen_active());
}

static bool updateMonsters(uint32_t now, float dt) {
  monsterSurvivalMs = now - monsterStartedAt;
  for (uint8_t i = 0; i < maxMonsters; ++i) {
    Monster &monster = monsters[i];
    if (!monster.active) {
      if (static_cast<int32_t>(now - monster.spawnAt) >= 0)
        spawnMonster(monster, now, i);
      continue;
    }

    monster.x += monster.vx * dt;
    monster.y += monster.vy * dt;
    positionMonster(monster);
    float dx = ballX - monster.x, dy = ballY - monster.y;
    float collisionDistance = ballRadius + monsterRadius;
    if (dx * dx + dy * dy < collisionDistance * collisionDistance) return true;

    if (monster.x < boardLeft - monsterRadius || monster.x > boardRight + monsterRadius ||
        monster.y < boardTop - monsterRadius || monster.y > boardBottom + monsterRadius) {
      monster.active = false;
      monster.spawnAt = now + 500U + nextRandom() % 1100U;
      lv_obj_add_flag(monster.body, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(monster.eyeA, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(monster.eyeB, LV_OBJ_FLAG_HIDDEN);
    }
  }
  return false;
}

static void showMode(ToyMode next) {
  if (mode == ToyMode::Tunnel && next != ToyMode::Tunnel) {
    ballRadius = baseBallRadius;
    lv_obj_set_size(ball, ballRadius * 2, ballRadius * 2);
    positionBall();
  }
  mode = next;
  clearCatchEffect();
  if (mode != ToyMode::Monsters) {
    for (Monster &monster : monsters) {
      lv_obj_add_flag(monster.body, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(monster.eyeA, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(monster.eyeB, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (mode == ToyMode::Maze) {
    lv_obj_clear_flag(goalOuter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(goalInner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(balanceRing, LV_OBJ_FLAG_HIDDEN);
    goalReachedAt = 0;
    generateMaze();
  } else if (mode == ToyMode::Monsters) {
    lv_obj_add_flag(goalOuter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goalInner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(balanceRing, LV_OBJ_FLAG_HIDDEN);
    soundBegin();
    startMonsterGame(millis());
  } else if (mode == ToyMode::Tunnel) {
    lv_obj_add_flag(goalOuter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goalInner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(balanceRing, LV_OBJ_FLAG_HIDDEN);
    soundBegin();
    startTunnelGame(millis());
  } else if (mode == ToyMode::Balance) {
    lv_obj_add_flag(goalOuter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goalInner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(balanceRing, LV_OBJ_FLAG_HIDDEN);
    balanceMs = 0;
  } else {
    lv_obj_add_flag(goalOuter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goalInner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(balanceRing, LV_OBJ_FLAG_HIDDEN);
  }
  lv_label_set_text(titleLabel, modeNames[static_cast<uint8_t>(mode)]);
  lv_label_set_text(hintLabel, modeHints[static_cast<uint8_t>(mode)]);
  placeLabel(titleLabel, 28, LCD_WIDTH - 24);
  placeLabel(hintLabel, 58, LCD_WIDTH - 20);
  lastPhysics = millis();
  lv_obj_invalidate(lv_screen_active());
  lastHud = 0;
}

static void nextMode() {
  uint8_t next = (static_cast<uint8_t>(mode) + 1) % 5;
  showMode(static_cast<ToyMode>(next));
}

static void screenEvent(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) return;
  if (mode == ToyMode::Monsters && monsterGameOver) startMonsterGame(millis());
  else if (mode == ToyMode::Tunnel && tunnelGameOver) startTunnelGame(millis());
  else nextMode();
}

static void resolveWall(float ax, float ay, float bx, float by) {
  float sx = bx - ax, sy = by - ay;
  float length2 = sx * sx + sy * sy;
  float t = ((ballX - ax) * sx + (ballY - ay) * sy) / length2;
  t = constrain(t, 0.0f, 1.0f);
  float dx = ballX - (ax + t * sx), dy = ballY - (ay + t * sy);
  float distance2 = dx * dx + dy * dy;
  const float clearance = ballRadius + 2.0f;
  if (distance2 >= clearance * clearance) return;

  float distance = sqrtf(distance2);
  float nx, ny;
  if (distance < 0.001f) {
    float length = sqrtf(length2);
    nx = -sy / length; ny = sx / length;
    if ((ballX - cx) * nx + (ballY - cy) * ny < 0) { nx = -nx; ny = -ny; }
    distance = 0;
  } else {
    nx = dx / distance; ny = dy / distance;
  }
  ballX += nx * (clearance - distance);
  ballY += ny * (clearance - distance);
  float towardWall = velocityX * nx + velocityY * ny;
  if (towardWall < 0) {
    velocityX -= 1.55f * towardWall * nx;
    velocityY -= 1.55f * towardWall * ny;
  }
}

static void updatePhysics(uint32_t now) {
  if (!haveSample) return;
  if (mode == ToyMode::Monsters && monsterGameOver) return;
  if (mode == ToyMode::Tunnel && tunnelGameOver) return;
  if (mode == ToyMode::Maze && goalReachedAt) {
    if (now - goalReachedAt >= 900) {
      goalReachedAt = 0;
      generateMaze();
      lv_label_set_text(hintLabel, modeHints[0]);
    }
    return;
  }
  uint32_t elapsed = lastPhysics ? now - lastPhysics : 0;
  lastPhysics = now;
  float dt = min(elapsed * 0.001f, 0.04f);
  if (dt <= 0 || fabsf(fastUp.z) < 0.72f) return;

  velocityX += -fastUp.x * accelerationGain * dt;
  velocityY += -fastUp.y * accelerationGain * dt;
  float damping = expf(-drag * dt);
  velocityX *= damping; velocityY *= damping;
  float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
  if (speed > maxSpeed) {
    velocityX *= maxSpeed / speed;
    velocityY *= maxSpeed / speed;
  }
  ballX += velocityX * dt;
  ballY += velocityY * dt;

  if (mode == ToyMode::Tunnel) {
    tunnelSurvivalMs = now - tunnelStartedAt;
    // Water speeds up slowly: about 2.5x after two minutes.
    float speedUp = 1.0f + min(tunnelSurvivalMs / 1000.0f, 135.0f) * 0.011f;
    float scrollDelta = 42.0f * tunnelRate * speedUp * dt;
    tunnelScroll += scrollDelta;
    if (gemActive) {
      gemY -= scrollDelta;
      float dx = ballX - gemX, dy = ballY - gemY;
      float collectDistance = ballRadius + gemRadius;
      if (dx * dx + dy * dy < collectDistance * collectDistance) {
        ++gemsCollected;
        soundPlay(Sound::Chime);
        scheduleGem(now);
      } else if (static_cast<int32_t>(now - gemExpiresAt) >= 0 || gemY < boardTop) {
        missGem();
        scheduleGem(now);
      }
    } else if (static_cast<int32_t>(now - gemSpawnAt) >= 0) {
      spawnGem(now);
    }

    float leftWall = 0, rightWall = 0;
    bool firstSample = true;
    for (float sampleY : {ballY - ballRadius, ballY, ballY + ballRadius}) {
      float center = tunnelCenter(sampleY), halfWidth = tunnelHalfWidth(sampleY);
      float sampleLeft = center - halfWidth, sampleRight = center + halfWidth;
      if (firstSample) {
        leftWall = sampleLeft; rightWall = sampleRight;
        firstSample = false;
      } else {
        leftWall = max(leftWall, sampleLeft);
        rightWall = min(rightWall, sampleRight);
      }
    }
    bool hitSide = ballX - ballRadius <= leftWall || ballX + ballRadius >= rightWall;
    bool hitEnd = ballY - ballRadius <= boardTop || ballY + ballRadius >= boardBottom;
    if (hitSide || hitEnd) {
      tunnelGameOver = true;
      soundPlay(Sound::Scare);
      setText(hintLabel, hitEnd ? "CRASHED! Tap to try again" : "WALL! Tap to try again");
    }
    positionBall();
    lv_obj_invalidate(lv_screen_active());
    return;
  }

  float left = boardLeft + ballRadius + 2, right = boardRight - ballRadius - 2;
  float top = boardTop + ballRadius + 2, bottom = boardBottom - ballRadius - 2;
  if (ballX < left) { ballX = left; if (velocityX < 0) velocityX *= -0.45f; }
  if (ballX > right) { ballX = right; if (velocityX > 0) velocityX *= -0.45f; }
  if (ballY < top) { ballY = top; if (velocityY < 0) velocityY *= -0.45f; }
  if (ballY > bottom) { ballY = bottom; if (velocityY > 0) velocityY *= -0.45f; }

  if (mode == ToyMode::Maze) {
    for (int32_t r = 0; r <= mazeN; ++r) {
      for (int32_t c = 0; c < mazeN; ++c) {
        float x = boardLeft + c * mazeCell, y = boardTop + r * mazeCell;
        if (mazeH[r][c]) resolveWall(x, y, x + mazeCell, y);
      }
    }
    for (int32_t r = 0; r < mazeN; ++r) {
      for (int32_t c = 0; c <= mazeN; ++c) {
        float x = boardLeft + c * mazeCell, y = boardTop + r * mazeCell;
        if (mazeV[r][c]) resolveWall(x, y, x, y + mazeCell);
      }
    }
    float gx = ballX - mazeGoalX, gy = ballY - mazeGoalY;
    if (!goalReachedAt && gx * gx + gy * gy < 18.0f * 18.0f) {
      ++mazeGoals;
      goalReachedAt = now;
      lv_label_set_text(hintLabel, "GOAL! A new labyrinth is coming");
    }
  } else if (mode == ToyMode::Balance) {
    float dx = ballX - cx, dy = ballY - cy;
    if (dx * dx + dy * dy < 23.0f * 23.0f) balanceMs += elapsed;
  } else if (mode == ToyMode::Monsters && updateMonsters(now, dt)) {
    monsterGameOver = true;
    lv_label_set_text(hintLabel, "CAUGHT! Tap to try again");
    startCatchEffect(now);
  }
  positionBall();
}

static void updateHud(uint32_t now) {
  if (now - lastHud < 150) return;
  lastHud = now;
  char text[48];
  if (mode == ToyMode::Maze)
    snprintf(text, sizeof text, "GOALS %u     TAP TO SWITCH", mazeGoals);
  else if (mode == ToyMode::Pinwheel)
    snprintf(text, sizeof text, "FOLLOW THE COLORS     TAP: NEXT");
  else if (mode == ToyMode::Balance)
    snprintf(text, sizeof text, "CENTER  %lu.%lu s     TAP: NEXT",
             static_cast<unsigned long>(balanceMs / 1000),
             static_cast<unsigned long>((balanceMs / 100) % 10));
  else if (mode == ToyMode::Monsters)
    snprintf(text, sizeof text, "%s  %lu.%lu s     TAP %s",
             monsterGameOver ? "CAUGHT" : "SURVIVED",
             static_cast<unsigned long>(monsterSurvivalMs / 1000),
             static_cast<unsigned long>((monsterSurvivalMs / 100) % 10),
             monsterGameOver ? "RETRY" : "TO CHANGE");
  else
    snprintf(text, sizeof text, "%s %lu.%lu  GEM %u  MISS %u  D%u",
             tunnelGameOver ? "CRASH" : "SWAM",
             static_cast<unsigned long>(tunnelSurvivalMs / 1000),
             static_cast<unsigned long>((tunnelSurvivalMs / 100) % 10),
             gemsCollected, gemsMissed, static_cast<unsigned>(ballRadius * 2));
  setText(footerLabel, text);
  placeLabel(footerLabel, 430, LCD_WIDTH - 16);
}

static void readImu() {
  if (!imuPresent || !imu.getDataReady()) return;
  Vec3 sample;
  if (!imu.getAccelerometer(sample.x, sample.y, sample.z) || !plausible(sample)) return;
  rawAcc = sample;
  Vec3 up = toScreen(sample);
  uint32_t now = micros();
  if (!haveSample) {
    fastUp = up;
    haveSample = true;
  } else {
    float dt = min((now - lastSample) * 1e-6f, 0.5f);
    smooth(fastUp, up, dt, 0.10f);
  }
  lastSample = now;
}

static void roundArea(lv_event_t *event) {
  auto area = static_cast<lv_area_t *>(lv_event_get_param(event));
  area->x1 &= ~1; area->y1 &= ~1; area->x2 |= 1; area->y2 |= 1;
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data) {
  lcd.push(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
           reinterpret_cast<uint16_t *>(data));
  lv_display_flush_ready(display);
}

static void readTouch(lv_indev_t *, lv_indev_data_t *data) {
  data->state = LV_INDEV_STATE_RELEASED;
  if (!touchPresent) return;
  Wire.beginTransmission(touchAddress); Wire.write(0x02);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(touchAddress, uint8_t(5)) != 5) return;
  uint8_t fingers = Wire.read();
  uint16_t x = (Wire.read() & 15) << 8; x |= Wire.read();
  uint16_t y = (Wire.read() & 15) << 8; y |= Wire.read();
  if (fingers && x < LCD_WIDTH && y < LCD_HEIGHT) {
    data->point.x = x; data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
}

void setup() {
  Serial.begin(115200);
  if (!lcd.begin()) Serial.println("LevelToy: display initialization failed");
  delay(200);
  Wire.beginTransmission(touchAddress); touchPresent = Wire.endTransmission() == 0;
  if (touchPresent) {
    Wire.beginTransmission(touchAddress); Wire.write(0xFE); Wire.write(0xFF); Wire.endTransmission();
  }
  imuPresent = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (imuPresent) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_2G, SensorQMI8658::ACC_ODR_250Hz,
                            SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
  }

  lv_init(); lv_tick_set_cb(millis);
  lv_display_t *display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, pixels, nullptr, sizeof(pixels), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  lv_display_add_event_cb(display, roundArea, LV_EVENT_INVALIDATE_AREA, nullptr);
  lv_indev_t *input = lv_indev_create();
  lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(input, readTouch);

  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(cBackground), 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(screen, screenEvent, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(screen, drawBoard, LV_EVENT_DRAW_MAIN, nullptr);

  frame = makeBox(screen, cx, cy, 2 * half + 8, 2 * half + 8, 0, cBackground, LV_OPA_TRANSP);
  lv_obj_set_style_border_color(frame, lv_color_hex(cFrame), 0);
  lv_obj_set_style_border_width(frame, 3, 0);

  balanceRing = makeBox(screen, cx, cy, 52, 52, LV_RADIUS_CIRCLE, cBackground, LV_OPA_TRANSP);
  lv_obj_set_style_border_color(balanceRing, lv_color_hex(cGoal), 0);
  lv_obj_set_style_border_width(balanceRing, 3, 0);
  lv_obj_add_flag(balanceRing, LV_OBJ_FLAG_HIDDEN);

  for (Monster &monster : monsters) {
    monster.body = makeBox(screen, cx, cy, monsterRadius * 2, monsterRadius * 2,
                           LV_RADIUS_CIRCLE, cMonster);
    lv_obj_set_style_border_color(monster.body, lv_color_hex(0xFF9AAF), 0);
    lv_obj_set_style_border_width(monster.body, 2, 0);
    monster.eyeA = makeBox(screen, cx, cy, 6, 6, LV_RADIUS_CIRCLE, cEye);
    monster.eyeB = makeBox(screen, cx, cy, 6, 6, LV_RADIUS_CIRCLE, cEye);
    monster.active = false;
    lv_obj_add_flag(monster.body, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(monster.eyeA, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(monster.eyeB, LV_OBJ_FLAG_HIDDEN);
  }

  goalOuter = makeBox(screen, mazeGoalX, mazeGoalY, 30, 30,
                      LV_RADIUS_CIRCLE, cGoal, LV_OPA_COVER);
  goalInner = makeBox(screen, mazeGoalX, mazeGoalY, 14, 14,
                      LV_RADIUS_CIRCLE, cBackground, LV_OPA_COVER);

  ball = makeBox(screen, cx, cy, ballRadius * 2, ballRadius * 2, LV_RADIUS_CIRCLE, cGold);
  lv_obj_set_style_border_color(ball, lv_color_hex(0xFFF4C2), 0);
  lv_obj_set_style_border_width(ball, 2, 0);
  lv_obj_set_style_shadow_color(ball, lv_color_hex(0x111820), 0);
  lv_obj_set_style_shadow_width(ball, 8, 0);
  lv_obj_set_style_shadow_opa(ball, LV_OPA_70, 0);

  titleLabel = lv_label_create(screen);
  hintLabel = lv_label_create(screen);
  footerLabel = lv_label_create(screen);
  for (lv_obj_t *label : {titleLabel, hintLabel, footerLabel}) {
    lv_obj_remove_flag(label, lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  }
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_font(hintLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_font(footerLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(cText), 0);
  lv_obj_set_style_text_color(hintLabel, lv_color_hex(cDim), 0);
  lv_obj_set_style_text_color(footerLabel, lv_color_hex(cDim), 0);

  catchRing = makeBox(screen, cx, cy, 26, 26, LV_RADIUS_CIRCLE, cRed, LV_OPA_TRANSP);
  lv_obj_set_style_border_color(catchRing, lv_color_hex(cRed), 0);
  lv_obj_set_style_border_width(catchRing, 5, 0);
  lv_obj_add_flag(catchRing, LV_OBJ_FLAG_HIDDEN);
  catchFlash = makeBox(screen, LCD_WIDTH / 2, LCD_HEIGHT / 2, LCD_WIDTH, LCD_HEIGHT, 0, cRed, LV_OPA_TRANSP);
  lv_obj_add_flag(catchFlash, LV_OBJ_FLAG_HIDDEN);

  positionGoal();
  randomState = micros() ^ 0xA341316CU;
  if (randomState == 0) randomState = 1;
  showMode(ToyMode::Maze);
  updateHud(millis() - 150);
  Serial.printf("LevelToy: touch %s, IMU %s. Tap the screen or send 'm' to switch modes; 's' streams sensor data.\n",
                touchPresent ? "detected" : "absent", imuPresent ? "detected" : "absent");
  if (!imuPresent) {
    lv_label_set_text(titleLabel, "NO IMU");
    lv_label_set_text(hintLabel, "QMI8658 not detected");
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(cRed), 0);
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'm' || c == 'M') nextMode();
    if (c == 's') streaming = !streaming;
  }
  readImu();
  uint32_t now = millis();
  updatePhysics(now);
  if (imuPresent && !((mode == ToyMode::Monsters && monsterGameOver) ||
                      (mode == ToyMode::Tunnel && tunnelGameOver))) {
    if (fabsf(fastUp.z) < 0.72f)
      setText(hintLabel, "Lay the board flat to play");
    else if (mode != ToyMode::Maze)
      setText(hintLabel, modeHints[static_cast<uint8_t>(mode)]);
  }
  if (mode == ToyMode::Tunnel) lv_obj_invalidate(lv_screen_active());
  catchEffectTick(now);
  soundTick(now);
  updateHud(now);

  if (streaming && now - lastStream >= 200) {
    lastStream = now;
    Serial.printf("acc %+.3f %+.3f %+.3f  up %+.3f %+.3f %+.3f  ball %.1f %.1f\n",
                  rawAcc.x, rawAcc.y, rawAcc.z, fastUp.x, fastUp.y, fastUp.z, ballX, ballY);
  }
  lv_timer_handler();
  delay(2);
}
