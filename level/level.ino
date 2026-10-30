#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <lvgl.h>
#include "SensorQMI8658.hpp"
#include "BoardDisplay.h"
#include "LevelMath.h"
using namespace level;

BoardDisplay lcd;
SensorQMI8658 imu;
Preferences prefs;
// Partial rendering: 40 rows, 29 KB. LVGL silently rejects a draw buffer not
// aligned to LV_DRAW_BUF_ALIGN.
alignas(LV_DRAW_BUF_ALIGN) static uint16_t pixels[LCD_WIDTH * 40];
static constexpr uint8_t touchAddress = 0x15; // CST816T
static bool touchPresent = false, imuPresent = false;

// Layout on the 368 x 448 portrait screen.
static constexpr int32_t cx = LCD_WIDTH / 2, cy = 240;
static constexpr int32_t dialR = 150, bubbleR = 26, dialRange = dialR - bubbleR - 6;
static constexpr int32_t tubeLen = 336, tubeLenTall = 400, tubeThick = 80;
static constexpr int32_t pillLen = 104, pillThick = 56;
static constexpr int32_t tallX = 268, tallY = LCD_HEIGHT / 2, sideTextX = 104;
static constexpr float levelDegrees = 0.25f;  // Readout turns green inside this.
static constexpr uint32_t zeroDelay = 1000;   // Lets the tap's wobble settle first.

static constexpr uint32_t cBackground = 0x05080C, cDial = 0x0C1822, cLine = 0x2A4458,
                          cDim = 0x58707F, cText = 0xE8EEF2, cBubble = 0xF2C94C,
                          cLevel = 0x6FE36F, cError = 0xFF5A4E;

static lv_obj_t *flatView, *tubeView, *flatBubble, *tube, *tubeBubble, *marks[2];
static lv_obj_t *bigLabel, *infoLabel;
static Mode mode = Mode::Flat;
static int32_t tubeRange, tubeX, tubeY, bubbleX = INT32_MIN, bubbleY;
static bool shownLevel = false, haveSample = false, streaming = false;
static Vec3 rawAcc, fastUp{0, 0, 1}, slowUp{0, 0, 1};
static uint32_t lastSample, lastText, lastStream, statusUntil, zeroAt;
static float zeros[kSlots][2];
static char status[32];

static lv_obj_t *makeBox(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                         int32_t radius, uint32_t color, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, x - w / 2, y - h / 2);  // x, y is the center.
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  return o;
}
static void outline(lv_obj_t *o, uint32_t color, int32_t width, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_set_style_border_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_border_width(o, width, 0);
  lv_obj_set_style_border_opa(o, opa, 0);
}
static void ring(lv_obj_t *parent, int32_t r, uint32_t color, int32_t width) {
  outline(makeBox(parent, cx, cy, 2 * r, 2 * r, LV_RADIUS_CIRCLE, 0, LV_OPA_TRANSP), color, width);
}
static lv_obj_t *makeBubble(lv_obj_t *parent, int32_t w, int32_t h) {
  lv_obj_t *b = makeBox(parent, cx, cy, w, h, LV_RADIUS_CIRCLE, cBubble, LV_OPA_80);
  outline(b, 0xFFFFFF, 2, LV_OPA_50);
  return b;
}

// Bullseye: rings mark where the bubble's edge sits at 1, 3 and 6 degrees.
static void buildFlatView(lv_obj_t *screen) {
  flatView = makeBox(screen, LCD_WIDTH / 2, LCD_HEIGHT / 2, LCD_WIDTH, LCD_HEIGHT, 0, 0, LV_OPA_TRANSP);
  outline(makeBox(flatView, cx, cy, 2 * dialR, 2 * dialR, LV_RADIUS_CIRCLE, cDial), cLine, 2);
  makeBox(flatView, cx, cy, 2 * dialR - 8, 2, 0, cLine);
  makeBox(flatView, cx, cy, 2, 2 * dialR - 8, 0, cLine);
  for (float degrees : {3.0f, 6.0f})
    ring(flatView, bubbleR + 2 + lroundf(bubblePixels(degrees, dialRange)), cLine, 1);
  ring(flatView, bubbleR + 2 + lroundf(bubblePixels(1, dialRange)), cDim, 2);
  flatBubble = makeBubble(flatView, 2 * bubbleR, 2 * bubbleR);
}

// One vial, laid out horizontally (portrait) or vertically (landscape).
static void buildTubeView(lv_obj_t *screen) {
  tubeView = makeBox(screen, LCD_WIDTH / 2, LCD_HEIGHT / 2, LCD_WIDTH, LCD_HEIGHT, 0, 0, LV_OPA_TRANSP);
  tube = makeBox(tubeView, cx, cy, tubeLen, tubeThick, LV_RADIUS_CIRCLE, cDial);
  outline(tube, cLine, 2);
  for (auto &mark : marks) mark = makeBox(tubeView, cx, cy, 2, tubeThick - 12, 0, cDim);
  tubeBubble = makeBubble(tubeView, pillLen, pillThick);
}

static void placeLabel(lv_obj_t *label, int32_t x, int32_t y, int32_t w) {
  int32_t h = lv_font_get_line_height(lv_obj_get_style_text_font(label, LV_PART_MAIN));
  lv_obj_set_width(label, w);
  lv_obj_set_pos(label, x - w / 2, y - h / 2);
}

static void showMode(Mode m) {
  mode = m;
  bool tall = m == Mode::Landscape;
  if (m == Mode::Flat) {
    lv_obj_add_flag(tubeView, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(flatView, LV_OBJ_FLAG_HIDDEN);
  } else {
    int32_t len = tall ? tubeLenTall : tubeLen, gap = pillLen / 2 + 4;
    tubeX = tall ? tallX : cx;
    tubeY = tall ? tallY : cy;
    tubeRange = (len - pillLen) / 2 - 8;
    int32_t tw = tall ? tubeThick : len, th = tall ? len : tubeThick;
    lv_obj_set_size(tube, tw, th);
    lv_obj_set_pos(tube, tubeX - tw / 2, tubeY - th / 2);
    for (int i = 0; i < 2; ++i) {
      int32_t offset = i ? gap : -gap, w = tall ? tubeThick - 12 : 2, h = tall ? 2 : tubeThick - 12;
      lv_obj_set_size(marks[i], w, h);
      lv_obj_set_pos(marks[i], tubeX + (tall ? 0 : offset) - w / 2, tubeY + (tall ? offset : 0) - h / 2);
    }
    lv_obj_set_size(tubeBubble, tall ? pillThick : pillLen, tall ? pillLen : pillThick);
    lv_obj_add_flag(flatView, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(tubeView, LV_OBJ_FLAG_HIDDEN);
  }
  if (tall) {
    placeLabel(bigLabel, sideTextX, tallY - 28, 2 * sideTextX - 8);
    placeLabel(infoLabel, sideTextX, tallY + 30, 2 * sideTextX - 8);
  } else {
    placeLabel(bigLabel, cx, 48, LCD_WIDTH - 32);
    placeLabel(infoLabel, cx, 418, LCD_WIDTH - 32);
  }
  bubbleX = INT32_MIN;  // Force the bubble to be placed.
  lastText = 0;
  Serial.printf("Level: %s view\n", m == Mode::Flat ? "flat" : tall ? "landscape" : "portrait");
}

static Reading current(Vec3 up) {
  Reading r = measure(up, mode);
  return applyZero(r, zeros[r.slot][0], zeros[r.slot][1]);
}

static void updateBubble() {
  Reading r = current(fastUp);
  lv_obj_t *b;
  int32_t x, y, w, h;
  if (mode == Mode::Flat) {
    // Map the total tilt radially so the bubble eases into the dial wall in every direction.
    float t = sqrtf(r.x * r.x + r.y * r.y);
    float scale = t > 1e-3f ? bubblePixels(t, dialRange) / t : dialRange / 8.0f;
    x = cx + lroundf(r.x * scale);
    y = cy + lroundf(r.y * scale);
    b = flatBubble; w = h = 2 * bubbleR;
  } else {
    bool tall = mode == Mode::Landscape;
    x = tubeX + (tall ? 0 : lroundf(bubblePixels(r.x, tubeRange)));
    y = tubeY + (tall ? lroundf(bubblePixels(r.y, tubeRange)) : 0);
    b = tubeBubble; w = tall ? pillThick : pillLen; h = tall ? pillLen : pillThick;
  }
  if (x == bubbleX && y == bubbleY) return;
  bubbleX = x; bubbleY = y;
  lv_obj_set_pos(b, x - w / 2, y - h / 2);
}

static void setText(lv_obj_t *label, const char *text) {
  if (strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
}
static float shown(float degrees) { return fabsf(degrees) < 0.05f ? 0 : degrees; }

static void updateText() {
  Reading r = current(slowUp);
  char big[24], info[48];
  snprintf(big, sizeof big, "%.1f\xC2\xB0", shown(r.tilt));
  if (millis() < statusUntil) snprintf(info, sizeof info, "%s", status);
  else if (mode == Mode::Flat)
    // Y is positive when the top edge is high, matching how the screen reads.
    snprintf(info, sizeof info, "X %+.1f\xC2\xB0    Y %+.1f\xC2\xB0", shown(r.x), shown(-r.y));
  else snprintf(info, sizeof info, "Slope %.1f%%", shown(tanf(r.tilt / kDegrees) * 100));
  setText(bigLabel, big);
  setText(infoLabel, info);
  bool level = r.tilt < levelDegrees;
  if (level == shownLevel) return;
  shownLevel = level;
  lv_color_t bubble = lv_color_hex(level ? cLevel : cBubble);
  lv_obj_set_style_bg_color(flatBubble, bubble, 0);
  lv_obj_set_style_bg_color(tubeBubble, bubble, 0);
  lv_obj_set_style_text_color(bigLabel, lv_color_hex(level ? cLevel : cText), 0);
}

static void showStatus(const char *text, uint32_t ms) {
  snprintf(status, sizeof status, "%s", text);
  statusUntil = millis() + ms;
  lastText = 0;
}
static void requestZero() {
  zeroAt = millis() + zeroDelay;
  showStatus("Hold still...", zeroDelay + 100);
}
static void finishZero() {
  zeroAt = 0;
  Reading r = measure(slowUp, mode);
  zeros[r.slot][0] = r.x;
  zeros[r.slot][1] = r.y;
  prefs.putBytes("zeros", zeros, sizeof zeros);
  Serial.printf("Level: zeroed slot %d at %+.2f, %+.2f degrees\n", r.slot, r.x, r.y);
  showStatus("Zeroed", 1500);
}
static void clearZeros() {
  zeroAt = 0;
  memset(zeros, 0, sizeof zeros);
  prefs.putBytes("zeros", zeros, sizeof zeros);
  Serial.println("Level: zero cleared for every orientation");
  showStatus("Zero cleared", 1500);
}
static void screenEvent(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED) requestZero();
  else clearZeros();
}

static void readImu() {
  if (!imuPresent || !imu.getDataReady()) return;
  Vec3 a;
  if (!imu.getAccelerometer(a.x, a.y, a.z) || !plausible(a)) return;
  rawAcc = a;
  Vec3 up = toScreen(a);
  uint32_t now = micros();
  if (!haveSample) {
    fastUp = slowUp = up;
    haveSample = true;
  } else {
    float dt = min((now - lastSample) * 1e-6f, 0.5f);
    smooth(fastUp, up, dt, 0.12f);
    smooth(slowUp, up, dt, 0.5f);
  }
  lastSample = now;
}

// The CO5300 needs redraw windows that start on even pixels and have even sizes.
static void roundArea(lv_event_t *e) {
  auto a = static_cast<lv_area_t *>(lv_event_get_param(e));
  a->x1 &= ~1; a->y1 &= ~1; a->x2 |= 1; a->y2 |= 1;
}
static void flush(lv_display_t *display, const lv_area_t *a, uint8_t *data) {
  // Blocking push: LVGL may reuse its buffer immediately after flush_ready.
  lcd.push(a->x1, a->y1, a->x2 - a->x1 + 1, a->y2 - a->y1 + 1, reinterpret_cast<uint16_t *>(data));
  lv_display_flush_ready(display);
}
static void readTouch(lv_indev_t *, lv_indev_data_t *data) {
  data->state = LV_INDEV_STATE_RELEASED;
  if (!touchPresent) return;
  Wire.beginTransmission(touchAddress); Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return;
  if (Wire.requestFrom(touchAddress, uint8_t(5)) != 5) return;
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
  if (!lcd.begin()) Serial.println("Level: display initialization failed");
  delay(200); // Touch controller start-up after the expander reset.
  Wire.beginTransmission(touchAddress); touchPresent = Wire.endTransmission() == 0;
  // Disable auto-sleep so the first tap is not lost.
  if (touchPresent) { Wire.beginTransmission(touchAddress); Wire.write(0xFE); Wire.write(0xFF); Wire.endTransmission(); }
  imuPresent = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (imuPresent) {
    // 2 g for the finest resolution; the on-chip filter at 2.66% of 250 Hz removes most jitter.
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_2G, SensorQMI8658::ACC_ODR_250Hz, SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
  }
  prefs.begin("level");
  if (prefs.getBytes("zeros", zeros, sizeof zeros) != sizeof zeros) memset(zeros, 0, sizeof zeros);

  lv_init(); lv_tick_set_cb(millis);
  auto display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, pixels, nullptr, sizeof(pixels), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  lv_display_add_event_cb(display, roundArea, LV_EVENT_INVALIDATE_AREA, nullptr);
  auto input = lv_indev_create(); lv_indev_set_type(input, LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input, readTouch);

  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(cBackground), 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(screen, screenEvent, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(screen, screenEvent, LV_EVENT_LONG_PRESSED, nullptr);
  buildFlatView(screen);
  buildTubeView(screen);
  for (auto label : {&bigLabel, &infoLabel}) {
    *label = lv_label_create(screen);
    lv_label_set_text(*label, "");
    lv_obj_set_style_text_align(*label, LV_TEXT_ALIGN_CENTER, 0);
  }
  lv_obj_set_style_text_font(bigLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_font(infoLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(bigLabel, lv_color_hex(cText), 0);
  lv_obj_set_style_text_color(infoLabel, lv_color_hex(cDim), 0);
  showMode(Mode::Flat);

  Serial.printf("Level: touch %s, IMU %s. Serial: s stream readings, z zero, c clear zero.\n",
                touchPresent ? "detected" : "absent", imuPresent ? "detected" : "absent");
  if (!imuPresent) {
    lv_obj_add_flag(flatView, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(bigLabel, lv_color_hex(cError), 0);
    lv_label_set_text(bigLabel, "No IMU");
    lv_label_set_text(infoLabel, "QMI8658 not found at 0x6B");
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 's') streaming = !streaming;
    if (c == 'z') requestZero();
    if (c == 'c') clearZeros();
  }
  readImu();
  uint32_t now = millis();
  if (imuPresent && haveSample) {
    if (zeroAt && now >= zeroAt) finishZero();
    Mode m = chooseMode(slowUp, mode);
    if (m != mode) showMode(m);
    updateBubble();
    if (now - lastText >= 125) { lastText = now; updateText(); }
    if (streaming && now - lastStream >= 200) {
      lastStream = now;
      Reading r = current(slowUp);
      Serial.printf("acc %+.3f %+.3f %+.3f  up %+.3f %+.3f %+.3f  slot %d  x %+.2f y %+.2f tilt %.2f\n",
                    rawAcc.x, rawAcc.y, rawAcc.z, slowUp.x, slowUp.y, slowUp.z, r.slot, r.x, r.y, r.tilt);
    }
  }
  lv_timer_handler();
  delay(2);
}
