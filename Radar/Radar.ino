#include <Arduino.h>
#include <math.h>
#include <lvgl.h>
#include "BoardDisplay.h"
#include "RadarConfig.h"

// A stylized WWII phosphor scope. Contacts are simulated, not sensor readings.
// LVGL owns the scene; LovyanGFX transfers its partial RGB565 draw buffer.
static BoardDisplay lcd;
alignas(4) static uint16_t drawBuffer[240 * 40];
static lv_obj_t *rays[RadarConfig::TRAIL_RAYS];
static lv_point_precise_t rayPoints[RadarConfig::TRAIL_RAYS][2];
static lv_point_precise_t tickPoints[36][2];
static lv_point_precise_t axes[2][2] = {{{8,120},{232,120}},{{120,8},{120,232}}};
static float previousAngle = 0;
static uint32_t lastFrame = 0;
static bool paused = false;
static uint32_t sweepTime = 0;

struct Contact {
  float bearing, distance, drift;
  uint32_t age;
  bool detected;
  lv_obj_t *halo, *dot;
};
static Contact contacts[] = {
  {28, 0.64f, 0.6f}, {83, 0.39f, -0.4f}, {139, 0.81f, 0.25f},
  {197, 0.55f, -0.35f}, {258, 0.76f, 0.45f}, {322, 0.44f, -0.3f}
};

static float wrap(float degrees) {
  degrees = fmodf(degrees,360.0f);
  return degrees < 0 ? degrees+360.0f : degrees;
}
// Bearings are clockwise from north (zero at the top of the scope).
static lv_point_precise_t polar(float bearing, float radius) {
  float a = bearing * 0.01745329252f;
  return {int32_t(roundf(120 + sinf(a)*radius)), int32_t(roundf(120 - cosf(a)*radius))};
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data) {
  lcd.pushImage(area->x1,area->y1,area->x2-area->x1+1,area->y2-area->y1+1,
                reinterpret_cast<uint16_t *>(data));
  lv_display_flush_ready(display); // pushImage is synchronous.
}
static lv_obj_t *circle(lv_obj_t *parent, int diameter, uint32_t color, int border) {
  auto o=lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o,diameter,diameter);
  lv_obj_set_style_radius(o,LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
  lv_obj_set_style_bg_opa(o,border?LV_OPA_TRANSP:LV_OPA_COVER,0);
  lv_obj_set_style_border_color(o,lv_color_hex(color),0);
  lv_obj_set_style_border_width(o,border,0);
  lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(o);
  return o;
}
static lv_obj_t *line(lv_obj_t *parent, const lv_point_precise_t *points,
                      uint32_t color, int width) {
  auto o=lv_line_create(parent);
  lv_line_set_points(o,points,2); // Point storage must outlive the widget.
  lv_obj_set_style_line_color(o,lv_color_hex(color),0);
  lv_obj_set_style_line_width(o,width,0);
  lv_obj_set_style_line_rounded(o,true,0);
  return o;
}
static void caption(lv_obj_t *parent, const char *text, int x, int y) {
  auto o=lv_label_create(parent);
  lv_label_set_text(o,text);
  lv_obj_set_style_text_color(o,lv_color_hex(0x25854a),0);
  lv_obj_align(o,LV_ALIGN_CENTER,x,y);
}
static void createScope() {
  auto screen=lv_screen_active();
  lv_obj_set_style_bg_color(screen,lv_color_hex(0x010904),0);
  lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
  // Concentric range rings and compass marks form the stationary graticule.
  for(int radius : {28,56,84,112}) circle(screen,radius*2,0x124a27,1);
  line(screen,axes[0],0x104024,1); line(screen,axes[1],0x104024,1);
  for(int i=0;i<36;i++) {
    tickPoints[i][0]=polar(i*10, i%3==0?102:107);
    tickPoints[i][1]=polar(i*10,112);
    line(screen,tickPoints[i],i%3==0?0x247c43:0x15472b,1);
  }
  caption(screen,"N",0,-99); caption(screen,"E",99,0);
  caption(screen,"S",0,98); caption(screen,"W",-99,0);
  caption(screen,"RANGE  20 MI",0,77);
  // Older rays are dimmer; their angular fan leaves a phosphor-like trail.
  for(int i=RadarConfig::TRAIL_RAYS-1;i>=0;i--) {
    rayPoints[i][0]={120,120}; rayPoints[i][1]=polar(0,RadarConfig::RADIUS-2);
    rays[i]=line(screen,rayPoints[i],i==0?0x8eff9a:0x1aff60,i==0?2:3);
    lv_obj_set_style_line_opa(rays[i],i==0?255:int(90*expf(-float(i)/(RadarConfig::TRAIL_RAYS * 0.29f))),0);
  }
  // Echoes sit above the scan rays and initially remain dark.
  for(auto &c : contacts) {
    c.halo=circle(screen,15,0x18e850,1);
    c.dot=circle(screen,5,0x9aff95,0);
    lv_obj_set_align(c.halo,LV_ALIGN_TOP_LEFT); lv_obj_set_align(c.dot,LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_opa(c.halo,0,0); lv_obj_set_style_opa(c.dot,0,0);
  }
  circle(screen,5,0x49cf72,0);
}
static void animate(uint32_t delta) {
  sweepTime += delta;
  float angle=float(sweepTime % RadarConfig::SWEEP_PERIOD_MS)*360.0f/RadarConfig::SWEEP_PERIOD_MS;
  float advanced=wrap(angle-previousAngle);
  for(int i=0;i<RadarConfig::TRAIL_RAYS;i++) {
    float a=angle-i*RadarConfig::TRAIL_DEGREES/RadarConfig::TRAIL_RAYS;
    lv_obj_invalidate(rays[i]); // Erase the previous beam before updating its points.
    rayPoints[i][1]=polar(a,RadarConfig::RADIUS-2);
    lv_line_set_points(rays[i],rayPoints[i],2);
  }
  for(auto &c : contacts) {
    float crossed=wrap(c.bearing-previousAngle);
    if(crossed<=advanced || delta>=RadarConfig::SWEEP_PERIOD_MS) {
      c.detected=true; c.age=0;
      // Contacts change bearing slowly after each scan, like distant aircraft.
      c.bearing=wrap(c.bearing+c.drift);
    } else if(c.detected) {
      c.age=min(c.age+delta,RadarConfig::ECHO_FADE_MS);
    }
    auto p=polar(c.bearing,c.distance*RadarConfig::RADIUS);
    lv_obj_set_pos(c.dot,int(p.x)-2,int(p.y)-2);
    lv_obj_set_pos(c.halo,int(p.x)-7,int(p.y)-7);
    float fade=c.detected?expf(-float(c.age)/1100.0f):0.0f;
    if(c.age>=RadarConfig::ECHO_FADE_MS) fade=0;
    lv_obj_set_style_opa(c.dot,int(255*fade),0);
    lv_obj_set_style_opa(c.halo,int(150*fade),0);
    int size=10+min(int(c.age/55),10);
    lv_obj_set_size(c.halo,size,size);
    lv_obj_set_pos(c.halo,int(p.x)-size/2,int(p.y)-size/2);
  }
  previousAngle=angle;
}
void setup() {
  Serial.begin(115200);
  pinMode(3,OUTPUT); digitalWrite(3,HIGH);
  lcd.init(); lcd.setRotation(0); lcd.setSwapBytes(true);
  lv_init(); lv_tick_set_cb(millis);
  auto display=lv_display_create(240,240);
  lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display,drawBuffer,nullptr,sizeof(drawBuffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display,flush);
  // Match display refresh to animation updates instead of the 33 ms default.
  lv_timer_set_period(lv_display_get_refr_timer(display),RadarConfig::FRAME_MS);
  createScope(); animate(0); lastFrame=millis();
  Serial.println("Radar: simulated contacts. Send SPACE to pause/resume.");
}
void loop() {
  while(Serial.available()) if(Serial.read()==' ') paused=!paused;
  uint32_t now=millis();
  if(now-lastFrame>=RadarConfig::FRAME_MS) {
    uint32_t delta=now-lastFrame; lastFrame=now;
    if(!paused) animate(delta);
  }
  lv_timer_handler(); delay(2);
}



