#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include "BoardDisplay.h"

// LVGL 9: each page creates a widget, configures it and attaches events.
BoardDisplay lcd;
static uint16_t pixels[240 * 24]; // Partial rendering: only 11.5 KB.
static lv_obj_t *body, *title, *status;
static int page = 0, pending = 0;
static bool touchPresent = false, autoplay = false;
static uint32_t lastPage;
static const char *names[] = {
  "Object", "Label", "Button", "Button matrix", "Checkbox", "Switch",
  "Slider", "Bar", "Arc", "Roller", "Dropdown", "Text area", "Keyboard",
  "Spinbox", "LED", "Spinner", "Chart", "Table", "List", "Calendar",
  "Scale", "Line", "Canvas", "Image", "Image button", "Animated image",
  "Span", "Arc label", "Tab view", "Tile view", "Menu", "Message box", "Window"
};
static constexpr int count = sizeof(names) / sizeof(names[0]);
static uint16_t imagePixels[32 * 32], canvasPixels[64 * 48];
static lv_image_dsc_t img;
static const void *frames[2];
static const lv_point_precise_t points[] = {{0,50}, {30,5}, {65,45}, {110,0}};

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
static lv_obj_t *label(lv_obj_t *parent, const char *text) {
  lv_obj_t *o = lv_label_create(parent); lv_label_set_text(o, text); return o;
}
static void changed(lv_event_t *e) {
  lv_obj_t *o = lv_event_get_target_obj(e);
  if (lv_obj_check_type(o, &lv_slider_class))
    lv_label_set_text_fmt(status, "Value: %ld", (long)lv_slider_get_value(o));
  else if (lv_obj_check_type(o, &lv_arc_class))
    lv_label_set_text_fmt(status, "Value: %ld", (long)lv_arc_get_value(o));
  else lv_label_set_text(status, "Event received!");
}
static void navigate(lv_event_t *e) {
  pending = (int)(intptr_t)lv_event_get_user_data(e);
}
static void spin(lv_event_t *e) {
  lv_obj_t *o = (lv_obj_t *)lv_event_get_user_data(e);
  lv_spinbox_increment(o);
}
// Pages are destroyed before rebuilding; memory usage stays bounded.
static void showPage() {
  lv_obj_clean(body);
  lv_label_set_text_fmt(title, "%d/%d %s", page+1, count, names[page]);
  lv_label_set_text(status, "Touch to explore");
  Serial.printf("%d/%d: %s\n", page+1, count, names[page]);
  lv_obj_t *o = nullptr;
  switch (page) {
    case 0: o=lv_obj_create(body); lv_obj_set_size(o,100,65); label(o,"Container"); break;
    case 1: o=label(body,"Hello LVGL!\nLabels display text."); break;
    case 2: o=lv_button_create(body); lv_obj_center(label(o,"Tap me")); break;
    case 3: { static const char *map[]={"One","Two","\n","Three",""};
      o=lv_buttonmatrix_create(body); lv_buttonmatrix_set_map(o,map); lv_obj_set_size(o,145,85); break; }
    case 4: o=lv_checkbox_create(body); lv_checkbox_set_text(o,"Enable"); break;
    case 5: o=lv_switch_create(body); break;
    case 6: o=lv_slider_create(body); lv_obj_set_width(o,135); lv_slider_set_value(o,40,LV_ANIM_OFF); break;
    case 7: o=lv_bar_create(body); lv_obj_set_size(o,135,22); lv_bar_set_value(o,65,LV_ANIM_OFF); break;
    case 8: o=lv_arc_create(body); lv_obj_set_size(o,105,105); lv_arc_set_value(o,65); break;
    case 9: o=lv_roller_create(body); lv_roller_set_options(o,"Red\nGreen\nBlue",LV_ROLLER_MODE_NORMAL); lv_roller_set_visible_row_count(o,3); break;
    case 10: o=lv_dropdown_create(body); lv_dropdown_set_options(o,"Red\nGreen\nBlue"); lv_obj_set_width(o,125); break;
    case 11: o=lv_textarea_create(body); lv_obj_set_size(o,145,90); lv_textarea_set_text(o,"Editable text"); break;
    case 12: { lv_obj_t *ta=lv_textarea_create(body); lv_obj_set_size(ta,145,30); lv_obj_align(ta,LV_ALIGN_TOP_MID,0,0);
      o=lv_keyboard_create(body); lv_obj_set_size(o,155,80); lv_obj_align(o,LV_ALIGN_BOTTOM_MID,0,0); lv_keyboard_set_textarea(o,ta); break; }
    case 13: o=lv_spinbox_create(body); lv_spinbox_set_digit_format(o,3,0); lv_spinbox_set_range(o,0,999); lv_spinbox_set_value(o,42); lv_obj_set_width(o,95); lv_obj_add_event_cb(o,spin,LV_EVENT_CLICKED,o); break;
    case 14: o=lv_led_create(body); lv_obj_set_size(o,55,55); lv_led_set_color(o,lv_palette_main(LV_PALETTE_GREEN)); lv_led_on(o); break;
    case 15: o=lv_spinner_create(body); lv_obj_set_size(o,80,80); lv_spinner_set_anim_params(o,1000,200); break;
    case 16: { o=lv_chart_create(body); lv_obj_set_size(o,145,95); lv_chart_set_point_count(o,6);
      auto s=lv_chart_add_series(o,lv_palette_main(LV_PALETTE_CYAN),LV_CHART_AXIS_PRIMARY_Y);
      for(int v : {20,60,35,80,45,70}) lv_chart_set_next_value(o,s,v); break; }
    case 17: o=lv_table_create(body); lv_table_set_column_count(o,2); lv_table_set_column_width(o,0,70); lv_table_set_column_width(o,1,70); lv_table_set_cell_value(o,0,0,"Item"); lv_table_set_cell_value(o,0,1,"Value"); lv_table_set_cell_value(o,1,0,"Temp"); lv_table_set_cell_value(o,1,1,"24 C"); lv_obj_set_size(o,145,100); break;
    case 18: o=lv_list_create(body); lv_obj_set_size(o,145,105); lv_list_add_button(o,LV_SYMBOL_HOME,"Home"); lv_list_add_button(o,LV_SYMBOL_SETTINGS,"Settings"); lv_list_add_button(o,LV_SYMBOL_OK,"Done"); break;
    case 19: o=lv_calendar_create(body); lv_obj_set_size(o,155,115); lv_calendar_set_today_date(o,2026,9,30); lv_calendar_set_showed_date(o,2026,9); break;
    case 20: o=lv_scale_create(body); lv_obj_set_size(o,130,85); lv_scale_set_mode(o,LV_SCALE_MODE_HORIZONTAL_BOTTOM); lv_scale_set_range(o,0,100); lv_scale_set_total_tick_count(o,11); lv_scale_set_major_tick_every(o,5); break;
    case 21: o=lv_line_create(body); lv_line_set_points(o,points,4); break;
    case 22: o=lv_canvas_create(body); lv_canvas_set_buffer(o,canvasPixels,64,48,LV_COLOR_FORMAT_RGB565); lv_canvas_fill_bg(o,lv_palette_main(LV_PALETTE_BLUE),LV_OPA_COVER); break;
    case 23: o=lv_image_create(body); lv_image_set_src(o,&img); break;
    case 24: o=lv_imagebutton_create(body); lv_imagebutton_set_src(o,LV_IMAGEBUTTON_STATE_RELEASED,nullptr,&img,nullptr); lv_imagebutton_set_src(o,LV_IMAGEBUTTON_STATE_PRESSED,nullptr,&img,nullptr); lv_obj_set_size(o,32,32); break;
    case 25: o=lv_animimg_create(body); lv_animimg_set_src(o,frames,2); lv_animimg_set_duration(o,900); lv_animimg_set_repeat_count(o,LV_ANIM_REPEAT_INFINITE); lv_animimg_start(o); break;
    case 26: { o=lv_spangroup_create(body); lv_obj_set_width(o,140); auto s=lv_spangroup_new_span(o); lv_span_set_text(s,"Styled "); auto t=lv_spangroup_new_span(o); lv_span_set_text(t,"text"); lv_style_set_text_color(lv_span_get_style(t),lv_palette_main(LV_PALETTE_ORANGE)); break; }
    case 27: o=lv_arclabel_create(body); lv_obj_set_size(o,130,110); lv_arclabel_set_radius(o,50); lv_arclabel_set_text(o,"Around the arc"); break;
    case 28: { o=lv_tabview_create(body); lv_obj_set_size(o,150,105); lv_tabview_set_tab_bar_size(o,30); auto t=lv_tabview_add_tab(o,"A"); lv_obj_center(label(t,"First tab")); t=lv_tabview_add_tab(o,"B"); lv_obj_center(label(t,"Second tab")); break; }
    case 29: { o=lv_tileview_create(body); lv_obj_set_size(o,145,100); auto t=lv_tileview_add_tile(o,0,0,LV_DIR_RIGHT); lv_obj_center(label(t,"Swipe left")); t=lv_tileview_add_tile(o,1,0,LV_DIR_LEFT); lv_obj_center(label(t,"Second tile")); break; }
    case 30: { o=lv_menu_create(body); lv_obj_set_size(o,150,110); auto p=lv_menu_page_create(o,nullptr); auto c=lv_menu_cont_create(p); label(c,"Settings"); c=lv_menu_cont_create(p); label(c,"Display / sound"); lv_menu_set_page(o,p); break; }
    case 31: o=lv_msgbox_create(body); lv_obj_set_size(o,150,110); lv_msgbox_add_title(o,"Message"); lv_msgbox_add_text(o,"A small dialog."); lv_msgbox_add_footer_button(o,"OK"); break;
    case 32: { o=lv_win_create(body); lv_obj_set_size(o,150,110); lv_win_add_title(o,"Window"); label(lv_win_get_content(o),"Window content"); break; }
  }
  if(o) {
    if(page!=12) lv_obj_center(o);
    lv_obj_add_event_cb(o,changed,LV_EVENT_VALUE_CHANGED,nullptr);
    lv_obj_add_event_cb(o,changed,LV_EVENT_CLICKED,nullptr);
  }
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
  lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL); lv_display_set_flush_cb(display,flush);
  auto input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input,readTouch);
  auto screen=lv_screen_active(); lv_obj_set_style_bg_color(screen,lv_color_hex(0x101820),0); lv_obj_set_style_text_color(screen,lv_color_white(),0);
  title=label(screen,""); lv_obj_set_width(title,170); lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0); lv_obj_align(title,LV_ALIGN_TOP_MID,0,26);
  body=lv_obj_create(screen); lv_obj_remove_style_all(body); lv_obj_set_size(body,160,120); lv_obj_center(body); lv_obj_remove_flag(body,LV_OBJ_FLAG_SCROLLABLE);
  status=label(screen,""); lv_obj_align(status,LV_ALIGN_BOTTOM_MID,0,-43);
  for(int direction : {-1,1}) { auto b=lv_button_create(screen); lv_obj_set_size(b,50,32); lv_obj_align(b,LV_ALIGN_BOTTOM_MID,direction*35,-18); lv_obj_center(label(b,direction<0?LV_SYMBOL_LEFT:LV_SYMBOL_RIGHT)); lv_obj_add_event_cb(b,navigate,LV_EVENT_CLICKED,(void *)(intptr_t)direction); }
  for(int y=0;y<32;y++) for(int x=0;x<32;x++) imagePixels[y*32+x]=((x/8+y/8)%2)?0x07E0:0xF800;
  img.header.magic=LV_IMAGE_HEADER_MAGIC; img.header.cf=LV_COLOR_FORMAT_RGB565; img.header.w=32; img.header.h=32; img.header.stride=64; img.data_size=sizeof(imagePixels); img.data=(const uint8_t *)imagePixels;
  frames[0]=&img; frames[1]=LV_SYMBOL_OK;
  Serial.println("LVGL gallery: n/p = next/previous; a = toggle 6-second tour");
  showPage();
}
void loop() {
  while(Serial.available()) { char c=Serial.read(); if(c=='n') pending=1; if(c=='p') pending=-1; if(c=='a') autoplay=!autoplay; }
  if(autoplay && millis()-lastPage>6000) pending=1;
  if(pending) { page=(page+pending+count)%count; pending=0; showPage(); }
  lv_timer_handler(); delay(5);
}


