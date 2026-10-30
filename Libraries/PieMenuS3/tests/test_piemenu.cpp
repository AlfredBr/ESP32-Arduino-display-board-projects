#include "lv_piemenu.h"
#include <lvgl_private.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <initializer_list>
// Matches the S3 panel. Round-mode cases use a 240 x 240 menu at the origin.
static constexpr int W=368,H=448;
static uint16_t framebuffer[W*H], partial[W*24];
static unsigned checks,clicks;
static lv_point_t touch;
static bool down;
static const uint16_t background=lv_color_to_u16(lv_color_hex(0x080E16));
static void require(bool v,const char *m) { ++checks; if(!v) { fprintf(stderr,"FAIL: %s\n",m); exit(1); } }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *data) {
  auto p=(uint16_t *)data;
  for(int y=a->y1;y<=a->y2;y++) for(int x=a->x1;x<=a->x2;x++) framebuffer[y*W+x]=*p++;
  lv_display_flush_ready(d);
}
static void save(const char *path) {
  FILE *f=fopen(path,"wb"); require(f!=nullptr,"preview opens"); fprintf(f,"P6\n%d %d\n255\n",W,H);
  for(auto pixel:framebuffer) {
    unsigned char rgb[]={(unsigned char)(((pixel>>11)&31)*255/31),(unsigned char)(((pixel>>5)&63)*255/63),(unsigned char)((pixel&31)*255/31)};
    fwrite(rgb,1,3,f);
  } fclose(f);
}
static lv_point_t polar(float cx,float cy,float radius,float degrees) {
  float a=degrees*3.14159265f/180;
  return {int32_t(lroundf(cx+radius*cosf(a))),int32_t(lroundf(cy+radius*sinf(a)))};
}
static lv_point_t polar(float radius,float degrees) { return polar(119.5f,119.5f,radius,degrees); }
// Distance from the screen center to the screen edge along a direction.
static float reach(float degrees) {
  float a=degrees*3.14159265f/180,c=fabsf(cosf(a)),s=fabsf(sinf(a));
  return std::min(c>1e-4f ? (W-1)/2.0f/c : 1e9f,s>1e-4f ? (H-1)/2.0f/s : 1e9f);
}
static unsigned hits(lv_obj_t **buttons,unsigned n,lv_point_t p) {
  unsigned h=0; for(unsigned j=0;j<n;j++) h+=lv_obj_hit_test(buttons[j],&p); return h;
}
static void clicked(lv_event_t *) { ++clicks; }
static void read(lv_indev_t *,lv_indev_data_t *d) { d->point=touch; d->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; }
static void tap(lv_indev_t *input,lv_point_t p) { touch=p; down=true; lv_indev_read(input); down=false; lv_indev_read(input); }
// LVGL gives up after 100 layout passes, leaving some object still dirty.
static bool dirty(lv_obj_t *obj) {
  if(obj->layout_inv) return true;
  for(unsigned i=0;i<lv_obj_get_child_count(obj);i++) if(dirty(lv_obj_get_child(obj,i))) return true;
  return false;
}
static lv_obj_t *build(unsigned n,lv_obj_t **buttons) {
  auto menu=lv_piemenu_create(lv_screen_active());
  lv_piemenu_set_rotation(menu,-90-180.0f/n); lv_piemenu_set_gap(menu,6);
  for(unsigned i=0;i<n;i++) {
    buttons[i]=lv_piemenu_add_button(menu); lv_piemenu_button_set_text(buttons[i],n<=4?"Tap":LV_SYMBOL_HOME);
    lv_obj_set_style_bg_color(buttons[i],lv_palette_main(lv_palette_t(i)),0);
    lv_obj_add_event_cb(buttons[i],clicked,LV_EVENT_CLICKED,nullptr);
  }
  if(n==8) require(lv_piemenu_add_button(menu)==nullptr,"slice limit");
  return menu;
}
static void round_layout(lv_display_t *display,lv_indev_t *input,unsigned n,bool ring) {
  lv_obj_t *buttons[8]; auto menu=build(n,buttons);
  lv_obj_set_size(menu,240,240); lv_obj_set_pos(menu,0,0);
  require(!lv_piemenu_get_fill(menu),"round is the default");
  // Full pies keep a hidden center, as the demo's do.
  lv_piemenu_set_inner_radius(menu,38); lv_obj_update_layout(menu);
  lv_piemenu_set_inner_radius(menu,ring?38:0); lv_obj_update_layout(menu);
  require(!dirty(menu),"layout settles");
  for(unsigned i=0;i<n;i++) {
    auto p=polar(80,-90+360.0f*i/n);
    require(hits(buttons,n,p)==1 && lv_obj_hit_test(buttons[i],&p),"exactly correct sector hit");
    lv_area_t a; lv_obj_get_coords(lv_piemenu_button_get_label(buttons[i]),&a);
    lv_point_t center={(a.x1+a.x2)/2,(a.y1+a.y2)/2};
    require(lv_obj_hit_test(buttons[i],&center),"label center in sector");
    p=polar(80,-90-180.0f/n+360.0f*i/n);
    require(hits(buttons,n,p)==0,"gap excluded");
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
      unsigned h=hits(buttons,n,p);
      if(ring && radius==20) { require(h==0,"ring hole excludes separator samples"); continue; }
      require(h==unsigned(offset==4),"parallel separator width is independent of radius");
      if(offset==2) require(framebuffer[p.y*W+p.x]==background,"separator is visibly empty");
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
      require(framebuffer[p.y*W+p.x]==expected,"rendered sector has no internal seams");
    }
  }
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
static void fill_layout(lv_display_t *display,lv_indev_t *input,unsigned n,bool ring,bool preview) {
  constexpr float cx=(W-1)/2.0f,cy=(H-1)/2.0f;
  lv_obj_t *buttons[8]; auto menu=build(n,buttons);
  lv_obj_set_style_text_font(menu,&lv_font_montserrat_28,0);
  lv_piemenu_set_fill(menu,true); require(lv_piemenu_get_fill(menu),"fill reads back");
  lv_piemenu_set_inner_radius(menu,60); lv_obj_update_layout(menu);
  lv_piemenu_set_inner_radius(menu,ring?60:0); lv_obj_update_layout(menu);
  require(!dirty(menu),"layout settles");
  require(lv_obj_get_width(menu)==W && lv_obj_get_height(menu)==H,"default size fills the screen");
  lv_point_t corners[]={{0,0},{W-1,0},{0,H-1},{W-1,H-1}};
  for(auto c : corners) require(hits(buttons,n,c)==1,"each screen corner belongs to one slice");
  for(unsigned i=0;i<n;i++) {
    float mid=-90+360.0f*i/n,boundary=mid-180.0f/n;
    // Slices reach the screen edge along their bisectors.
    for(float inset : {3.0f,40.0f}) {
      auto p=polar(cx,cy,reach(mid)-inset,mid);
      require(hits(buttons,n,p)==1 && lv_obj_hit_test(buttons[i],&p),"slice reaches the screen edge");
    }
    lv_area_t a; lv_obj_get_coords(lv_piemenu_button_get_label(buttons[i]),&a);
    lv_point_t center={(a.x1+a.x2)/2,(a.y1+a.y2)/2};
    require(lv_obj_hit_test(buttons[i],&center),"label center in clipped slice");
    require(a.x1>=0 && a.y1>=0 && a.x2<W && a.y2<H,"label stays on screen");
    // Separators continue past the inscribed circle to the screen edge.
    for(float frac : {0.5f,0.95f}) {
      auto p=polar(cx,cy,reach(boundary)*frac,boundary);
      require(hits(buttons,n,p)==0,"separator excluded beyond circle");
    }
    if(ring) { lv_point_t cp={int32_t(cx),int32_t(cy)}; require(!lv_obj_hit_test(buttons[i],&cp),"fill keeps the ring hole"); }
  }
  auto corner=corners[0]; clicks=0; tap(input,corner); require(clicks==1,"corner tap clicks");
  if(ring) {
    auto center=lv_piemenu_get_center_button(menu); lv_piemenu_button_set_text(center,LV_SYMBOL_RIGHT "\nNext");
    lv_point_t cp={int32_t(cx),int32_t(cy)}; require(lv_obj_hit_test(center,&cp) && hits(buttons,n,cp)==0,"center owns the middle");
  }
  lv_refr_now(display);
  for(auto c : corners) {
    lv_obj_t *owner=nullptr; for(unsigned j=0;j<n;j++) if(lv_obj_hit_test(buttons[j],&c)) owner=buttons[j];
    require(framebuffer[c.y*W+c.x]==lv_color_to_u16(lv_obj_get_style_bg_color(owner,LV_PART_MAIN)),"corner pixels are painted");
  }
  for(unsigned i=0;i<n;i++) {
    float boundary=-90+360.0f*i/n-180.0f/n;
    auto p=polar(cx,cy,reach(boundary)*0.9f,boundary);
    require(framebuffer[p.y*W+p.x]==background,"separator visible beyond circle");
  }
  if(preview) { char path[80]; snprintf(path,sizeof(path),"preview-%u-%s.ppm",n,ring?"ring":"pie"); save(path); }
  lv_obj_delete(menu); lv_refr_now(display); require(lv_mem_test()==LV_RESULT_OK,"heap integrity");
}
int main() {
  lv_init(); auto display=lv_display_create(W,H);
  lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display,partial,nullptr,sizeof(partial),LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display,flush);
  lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x080E16),0);
  auto input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input,read);
  lv_mem_monitor_t baseline; lv_mem_monitor(&baseline);
  for(unsigned pass=0;pass<3;pass++) {
    for(unsigned n=2;n<=8;n++) for(int ring=0;ring<=1;ring++) {
      round_layout(display,input,n,ring);
      fill_layout(display,input,n,ring,pass==0 && (n==4 || n==8));
    }
    if(pass==0) lv_mem_monitor(&baseline);
  }
  lv_mem_monitor_t final; lv_mem_monitor(&final); require(final.free_size==baseline.free_size,"no lifecycle memory leak");
  printf("PASS: %u checks across 28 layouts (14 round, 14 fill) repeated three times; peak LVGL heap %u bytes.\n",checks,(unsigned)final.max_used); lv_deinit();
}
