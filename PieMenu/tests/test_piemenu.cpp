#include "lv_piemenu.h"
#include <lvgl_private.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
static uint16_t framebuffer[240*240], partial[240*24];
static unsigned checks,clicks;
static lv_point_t touch;
static bool down;
static void require(bool v,const char *m) { ++checks; if(!v) { fprintf(stderr,"FAIL: %s\n",m); exit(1); } }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *data) {
  auto p=(uint16_t *)data;
  for(int y=a->y1;y<=a->y2;y++) for(int x=a->x1;x<=a->x2;x++) framebuffer[y*240+x]=*p++;
  lv_display_flush_ready(d);
}
static void save(const char *path) {
  FILE *f=fopen(path,"wb"); require(f!=nullptr,"preview opens"); fprintf(f,"P6\n240 240\n255\n");
  for(auto pixel:framebuffer) {
    unsigned char rgb[]={(unsigned char)(((pixel>>11)&31)*255/31),(unsigned char)(((pixel>>5)&63)*255/63),(unsigned char)((pixel&31)*255/31)};
    fwrite(rgb,1,3,f);
  } fclose(f);
}
static lv_point_t polar(float radius,float degrees) {
  float a=degrees*3.14159265f/180;
  return {int32_t(lroundf(119.5f+radius*cosf(a))),int32_t(lroundf(119.5f+radius*sinf(a)))};
}
static void clicked(lv_event_t *) { ++clicks; }
static void read(lv_indev_t *,lv_indev_data_t *d) { d->point=touch; d->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; }
static void tap(lv_indev_t *input,lv_point_t p) { touch=p; down=true; lv_indev_read(input); down=false; lv_indev_read(input); }
int main() {
  lv_init(); auto display=lv_display_create(240,240);
  lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display,partial,nullptr,sizeof(partial),LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display,flush);
  lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x080E16),0);
  auto input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input,read);
  lv_mem_monitor_t baseline; lv_mem_monitor(&baseline);
  for(unsigned pass=0;pass<3;pass++) { for(unsigned n=2;n<=8;n++) for(int ring=0;ring<=1;ring++) {
    auto menu=lv_piemenu_create(lv_screen_active()); lv_obj_t *buttons[8];
    lv_piemenu_set_rotation(menu,-90-180.0f/n); lv_piemenu_set_gap(menu,6);
    for(unsigned i=0;i<n;i++) {
      buttons[i]=lv_piemenu_add_button(menu); lv_piemenu_button_set_text(buttons[i],n<=4?"Tap":LV_SYMBOL_HOME);
      lv_obj_set_style_bg_color(buttons[i],lv_palette_main(lv_palette_t(i)),0);
      lv_obj_add_event_cb(buttons[i],clicked,LV_EVENT_CLICKED,nullptr);
    }
    if(n==8) require(lv_piemenu_add_button(menu)==nullptr,"slice limit");
    lv_piemenu_set_inner_radius(menu,ring?38:0); lv_obj_update_layout(menu);
    for(unsigned i=0;i<n;i++) {
      auto p=polar(80,-90+360.0f*i/n); unsigned hits=0;
      for(unsigned j=0;j<n;j++) hits+=lv_obj_hit_test(buttons[j],&p);
      require(hits==1 && lv_obj_hit_test(buttons[i],&p),"exactly correct sector hit");
      lv_area_t a; lv_obj_get_coords(lv_piemenu_button_get_label(buttons[i]),&a);
      lv_point_t center={(a.x1+a.x2)/2,(a.y1+a.y2)/2};
      require(lv_obj_hit_test(buttons[i],&center),"label center in sector");
      p=polar(80,-90-180.0f/n+360.0f*i/n);
      for(unsigned j=0;j<n;j++) require(!lv_obj_hit_test(buttons[j],&p),"gap excluded");
      lv_point_t corner={0,0}; require(!lv_obj_hit_test(buttons[i],&corner),"corner excluded");
      if(ring) { lv_point_t cp={120,120}; require(!lv_obj_hit_test(buttons[i],&cp),"hole excluded"); }
    }
    auto p=polar(80,-90); clicks=0; tap(input,p); require(clicks==1,"one real input click");
    lv_obj_add_state(buttons[0],LV_STATE_DISABLED); tap(input,p); require(clicks==1,"disabled rejects clicks");
    lv_obj_remove_state(buttons[0],LV_STATE_DISABLED); lv_obj_set_checkable(buttons[0],true); tap(input,p);
    require(lv_obj_has_state(buttons[0],LV_STATE_CHECKED),"checkable toggles"); lv_obj_remove_state(buttons[0],LV_STATE_CHECKED);
    if(ring) {
      auto center=lv_piemenu_get_center_button(menu); lv_piemenu_button_set_text(center,LV_SYMBOL_RIGHT "\nNext");
      lv_point_t cp={120,120}; require(lv_obj_hit_test(center,&cp),"center hit");
      lv_obj_add_event_cb(center,clicked,LV_EVENT_CLICKED,nullptr); clicks=0; tap(input,cp); require(clicks==1,"center input");
    }
    lv_refr_now(display);
    // A six-pixel separator stays six pixels wide at every distance from center.
    for(unsigned i=0;i<n;i++) for(int radius : {20,55,105}) {
      float a=(-90-180.0f/n+360.0f*i/n)*3.14159265f/180;
      for(int side : {-1,1}) for(int offset : {2,4}) {
        lv_point_t p={int32_t(lroundf(119.5f+radius*cosf(a)-side*offset*sinf(a))),
                      int32_t(lroundf(119.5f+radius*sinf(a)+side*offset*cosf(a)))};
        unsigned hits=0; for(unsigned j=0;j<n;j++) hits+=lv_obj_hit_test(buttons[j],&p);
        if(ring && radius==20) { require(hits==0,"ring hole excludes separator samples"); continue; }
        require(hits==unsigned(offset==4),"parallel separator width is independent of radius");
        if(offset==2) require(framebuffer[p.y*240+p.x]==lv_color_to_u16(lv_color_hex(0x080E16)),"separator is visibly empty");
      }
    }
    // Check solid fills over radial samples, excluding each sector's label.
    for(unsigned i=0;i<n;i++) {
      lv_area_t label; lv_obj_get_coords(lv_piemenu_button_get_label(buttons[i]),&label);
      auto expected=lv_color_to_u16(lv_obj_get_style_bg_color(buttons[i],LV_PART_MAIN));
      for(int radius=ring?45:10;radius<115;radius+=5) for(int offset=-30;offset<=30;offset+=3) {
        auto p=polar(radius,-90+360.0f*i/n+offset*(360.0f/n)/100);
        if(p.x>=label.x1-1 && p.x<=label.x2+1 && p.y>=label.y1-1 && p.y<=label.y2+1) continue;
        if(!lv_obj_hit_test(buttons[i],&p)) continue;
        require(framebuffer[p.y*240+p.x]==expected,"rendered sector has no internal seams");
      }
    }
    if(n==4 || n==8) { char path[80]; snprintf(path,sizeof(path),"preview-%u-%s.ppm",n,ring?"ring":"pie"); save(path); }
    lv_obj_set_size(menu,180,200); lv_obj_set_pos(menu,17,9); lv_obj_update_layout(menu);
    for(unsigned i=0;i<n;i++) {
      lv_area_t a; lv_obj_get_coords(lv_piemenu_button_get_label(buttons[i]),&a);
      lv_point_t p={(a.x1+a.x2)/2,(a.y1+a.y2)/2};
      require(lv_obj_hit_test(buttons[i],&p),"labels reflow after resize and move");
    }
    lv_obj_delete(buttons[n-1]); lv_obj_update_layout(menu);
    require(lv_piemenu_add_button(menu)!=nullptr,"deleted slice can be replaced");
    lv_piemenu_set_inner_radius(menu,0); lv_piemenu_set_inner_radius(menu,38); lv_obj_delete(lv_piemenu_get_center_button(menu));
    require(lv_piemenu_get_center_button(menu)==nullptr,"center deletion clears pointer");
    lv_piemenu_set_inner_radius(menu,38); require(lv_piemenu_get_center_button(menu)!=nullptr,"center recreates");
    lv_obj_delete(menu); lv_refr_now(display); require(lv_mem_test()==LV_RESULT_OK,"heap integrity");
  }
  if(pass==0) lv_mem_monitor(&baseline); }
  lv_mem_monitor_t final; lv_mem_monitor(&final); require(final.free_size==baseline.free_size,"no lifecycle memory leak");
  printf("PASS: %u checks across 14 layouts repeated three times; peak LVGL heap %u bytes.\n",checks,(unsigned)final.max_used); lv_deinit();
}






