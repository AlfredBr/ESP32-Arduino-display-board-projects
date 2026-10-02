#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include "BoardDisplay.h"

#include "lv_piemenu.h"
BoardDisplay lcd;
static uint16_t pixels[240 * 24]; // Partial rendering: only 11.5 KB.
static bool touchPresent = false;
static lv_obj_t *menu;
static int page=2, pending=0;
static bool ringLayout=false;
static bool tour=false;
static uint32_t lastPage;
static constexpr unsigned pageCount=7;
static const char *texts[]={"Lights","Music","Temp","Timer","Tools","Back","Info","Power"};
static const char *icons[]={LV_SYMBOL_HOME,LV_SYMBOL_AUDIO,LV_SYMBOL_SETTINGS,LV_SYMBOL_BELL,
                            LV_SYMBOL_WIFI,LV_SYMBOL_LEFT,LV_SYMBOL_LIST,LV_SYMBOL_POWER};
static const uint32_t colors[]={0x185B82,0x744482,0xA65028,0x267767,0x7D642B,0x3D518E,0x8B3959,0x386E38};
static void clicked(lv_event_t *e) {
  int index=(int)(intptr_t)lv_event_get_user_data(e);
  if(index<0) { pending=1; return; }
  Serial.printf("Clicked slice %d: %s\n",index+1,texts[index]);
}
static void buildPage() {
  if(menu) lv_obj_delete(menu);
  menu=lv_piemenu_create(lv_screen_active()); lv_obj_center(menu);
  // Two through eight slices, alternating full pie and ring layouts.
  unsigned n=page+2; ringLayout=page%2;
  lv_piemenu_set_rotation(menu,-90-180.0f/n);
  lv_piemenu_set_gap(menu,6);
  for(unsigned i=0;i<n;i++) {
    auto b=lv_piemenu_add_button(menu);
    lv_piemenu_button_set_text(b,page>=3?icons[i]:texts[i]);
    lv_obj_set_style_bg_color(b,lv_color_hex(colors[i]),0);
    lv_obj_add_event_cb(b,clicked,LV_EVENT_CLICKED,(void *)(intptr_t)i);
    if(i==0) lv_obj_set_checkable(b,true);
    if(n>=6 && i==n-2) lv_obj_add_state(b,LV_STATE_DISABLED);
    if(!ringLayout && i==n-1) {
      lv_piemenu_button_set_text(b,n>=5?LV_SYMBOL_RIGHT:"Next");
      lv_obj_remove_event_cb(b,clicked);
      lv_obj_add_event_cb(b,clicked,LV_EVENT_CLICKED,(void *)(intptr_t)-1);
    }
  }
  lv_piemenu_set_inner_radius(menu,38);
  {
    auto b=lv_piemenu_get_center_button(menu);
    lv_piemenu_button_set_text(b,LV_SYMBOL_RIGHT "\nNext");
    lv_obj_set_style_bg_color(b,lv_color_hex(0x202D3C),0);
    lv_obj_add_event_cb(b,clicked,LV_EVENT_CLICKED,(void *)(intptr_t)-1);
  }
  lv_piemenu_set_inner_radius(menu,ringLayout?38:0);
  lv_obj_update_layout(menu);
  Serial.printf("Page %d/%u: %u slices, %s. First slice toggles; penultimate disabled on 6+ slices.\n",page+1,pageCount,n,ringLayout?"ring":"full pie");
  lastPage=millis();
}
void setup() {
  Serial.begin(115200); pinMode(3,OUTPUT); digitalWrite(3,HIGH);
  lcd.init(); lcd.setRotation(0); lcd.setSwapBytes(true);
  Wire.begin(4,5); Wire.setTimeOut(20);
  pinMode(0,INPUT_PULLUP); pinMode(1,OUTPUT); digitalWrite(1,LOW); delay(10); digitalWrite(1,HIGH); delay(300);
  Wire.beginTransmission(0x15); touchPresent=Wire.endTransmission()==0;
  if(touchPresent) { Wire.beginTransmission(0x15); Wire.write(0xFE); Wire.write(0xFF); Wire.endTransmission(); }
  lv_init(); lv_tick_set_cb(millis);
  auto display=lv_display_create(240,240); lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display,flush);
  auto input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input,readTouch);
  lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x080E16),0);
  Serial.printf("PieMenu: touch %s. Serial: n/p pages, a tour, r ring/pie, 2-8 slice count.\n",touchPresent?"detected":"absent");
  tour=!touchPresent;
  buildPage();
}
void loop() {
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='n') pending=1; if(c=='p') pending=-1; if(c=='a') tour=!tour;
    if(c>='2' && c<='8') { page=c-'2'; buildPage(); }
    if(c=='r') { ringLayout=!ringLayout; lv_piemenu_set_inner_radius(menu,ringLayout?38:0); }
  }
  if(tour && millis()-lastPage>8000) pending=1;
  if(pending) { page=(page+pending+pageCount)%pageCount; pending=0; buildPage(); }
  lv_timer_handler(); delay(5);
}
static void flush(lv_display_t *display, const lv_area_t *a, uint8_t *data) {
  // Blocking push: LVGL may reuse its buffer immediately after flush_ready.
  lcd.pushImage(a->x1, a->y1, a->x2-a->x1+1, a->y2-a->y1+1,
                reinterpret_cast<uint16_t *>(data));
  lv_display_flush_ready(display);
}
static void readTouch(lv_indev_t *, lv_indev_data_t *data) {
  data->state = LV_INDEV_STATE_RELEASED;
  if (!touchPresent) return;
  Wire.beginTransmission(0x15); Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return;
  if (Wire.requestFrom(uint8_t(0x15), uint8_t(5)) != 5) return;
  uint8_t fingers = Wire.read();
  uint16_t x = (Wire.read() & 15) << 8; x |= Wire.read();
  uint16_t y = (Wire.read() & 15) << 8; y |= Wire.read();
  if (fingers && x < 240 && y < 240) {
    data->point.x = x; data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
}



