#include "lv_piemenu.h"
#include <lvgl_private.h>
#include <cmath>
#include <algorithm>

struct Menu { lv_obj_t obj; int32_t inner; float gap, rotation; lv_obj_t *center; bool laying; };
struct Slice { lv_obj_t obj; lv_obj_t *label; unsigned index; bool center; };
static void slice_event(const lv_obj_class_t *, lv_event_t *);
static void menu_event(const lv_obj_class_t *, lv_event_t *);
static lv_obj_class_t make_class(size_t size, const char *name,
                                void (*cb)(const lv_obj_class_t *, lv_event_t *)) {
  lv_obj_class_t c = {}; c.base_class = &lv_obj_class; c.instance_size = size;
  c.name = name; c.event_cb = cb; c.width_def = 240; c.height_def = 240; return c;
}
static const lv_obj_class_t menu_class = make_class(sizeof(Menu), "piemenu", menu_event);
static const lv_obj_class_t slice_class = make_class(sizeof(Slice), "piebutton", slice_event);
static constexpr float pi = 3.14159265358979323846f;
static float wrap(float a) { a = fmodf(a,360); return a < 0 ? a+360 : a; }
static unsigned count(lv_obj_t *obj) {
  unsigned n=0;
  for(unsigned i=0;i<lv_obj_get_child_count(obj);i++) {
    auto c=lv_obj_get_child(obj,i);
    if(lv_obj_check_type(c,&slice_class) && !reinterpret_cast<Slice *>(c)->center) ++n;
  }
  return n;
}
struct Geometry { float x,y,r,inner,start,sweep,gap; };
static Geometry geometry(lv_obj_t *obj) {
  auto s=reinterpret_cast<Slice *>(obj);
  auto m=reinterpret_cast<Menu *>(lv_obj_get_parent(obj));
  lv_area_t a; lv_obj_get_coords(obj,&a);
  float r=std::max(0.0f,std::min(lv_obj_get_width(obj),lv_obj_get_height(obj))/2.0f-0.5f);
  unsigned n=count(&m->obj);
  float step=360.0f/std::max(1u,n);
  float gap=n<2 ? 0 : std::min(m->gap,0.4f*r*sinf(pi/n));
  return {(a.x1+a.x2)/2.0f,(a.y1+a.y2)/2.0f,r,
          std::min(float(m->inner),r*0.7f),m->rotation+s->index*step,step,gap};
}
// Offset each boundary by half the gap, producing parallel separator edges.
// Share fixed-point half-plane tests between drawing and pointer hit testing.
struct Edges {
  int32_t ax,ay,bx,by; int64_t inset; bool wide;
  bool contains(int64_t dx2,int64_t dy2) const {
    bool left=ax*dy2-ay*dx2>=inset,right=bx*dy2-by*dx2<=-inset;
    return wide ? left||right : left&&right;
  }
};
static Edges edges(const Geometry &g) {
  float a=g.start*pi/180,b=(g.start+g.sweep)*pi/180;
  return {int32_t(lroundf(cosf(a)*32768)),int32_t(lroundf(sinf(a)*32768)),
          int32_t(lroundf(cosf(b)*32768)),int32_t(lroundf(sinf(b)*32768)),
          int64_t(ceilf(g.gap*32768)),g.sweep>180};
}
static void layout(lv_obj_t *obj) {
  auto m=reinterpret_cast<Menu *>(obj); if(m->laying) return; m->laying=true;
  unsigned idx=0;
  for(unsigned i=0;i<lv_obj_get_child_count(obj);i++) {
    auto child=lv_obj_get_child(obj,i); if(!lv_obj_check_type(child,&slice_class)) continue;
    auto s=reinterpret_cast<Slice *>(child); if(!s->center) s->index=idx++;
    lv_obj_set_size(child,lv_obj_get_width(obj),lv_obj_get_height(obj)); lv_obj_set_pos(child,0,0);
    if(!s->label) continue;
    auto g=geometry(child); float angle=(g.start+g.sweep/2)*pi/180;
    float radius=s->center ? 0 : (g.inner>0 ? (g.r+g.inner)/2 : g.r*0.62f);
    lv_obj_align(s->label,LV_ALIGN_CENTER,lroundf(radius*cosf(angle)),lroundf(radius*sinf(angle)));
    lv_obj_invalidate(child);
  }
  m->laying=false;
}
static void menu_event(const lv_obj_class_t *c,lv_event_t *e) {
  if(lv_obj_event_base(c,e)!=LV_RESULT_OK) return;
  auto code=lv_event_get_code(e);
  if(code==LV_EVENT_SIZE_CHANGED || code==LV_EVENT_CHILD_CHANGED || code==LV_EVENT_LAYOUT_CHANGED) layout(lv_event_get_target_obj(e));
}
static void slice_event(const lv_obj_class_t *c,lv_event_t *e) {
  auto code=lv_event_get_code(e); auto obj=lv_event_get_target_obj(e);
  auto s=reinterpret_cast<Slice *>(obj);
  if(code==LV_EVENT_DELETE && s->center) reinterpret_cast<Menu *>(lv_obj_get_parent(obj))->center=nullptr;
  if(code==LV_EVENT_COVER_CHECK) { auto info=(lv_cover_check_info_t *)lv_event_get_param(e); info->res=LV_COVER_RES_NOT_COVER; return; }
  if(code!=LV_EVENT_DRAW_MAIN && lv_obj_event_base(c,e)!=LV_RESULT_OK) return;
  if(code==LV_EVENT_HIT_TEST) {
    auto info=(lv_hit_test_info_t *)lv_event_get_param(e); auto g=geometry(obj);
    float dx=info->point->x-g.x,dy=info->point->y-g.y,r2=dx*dx+dy*dy;
    auto bounds=edges(g);
    float cr=std::max(0.0f,g.inner-1);
    info->res = s->center ? (cr>0 && r2<=cr*cr) :
      (r2<=g.r*g.r && r2>=g.inner*g.inner && bounds.contains(lroundf(dx*2),lroundf(dy*2)));
  } else if(code==LV_EVENT_DRAW_MAIN) {
    auto g=geometry(obj); auto layer=lv_event_get_layer(e);
    auto color=lv_obj_get_style_bg_color(obj,LV_PART_MAIN);
    auto opa=lv_obj_get_style_bg_opa(obj,LV_PART_MAIN);
    if(s->center) {
      if(g.inner<=1) return;
      lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color=color; d.bg_opa=opa; d.radius=LV_RADIUS_CIRCLE;
      lv_area_t a={(int32_t)ceilf(g.x-g.inner+1),(int32_t)ceilf(g.y-g.inner+1),
                   (int32_t)floorf(g.x+g.inner-1),(int32_t)floorf(g.y+g.inner-1)};
      lv_draw_rect(layer,&d,&a); return;
    }
    // Rasterize horizontal runs. Shared triangle edges otherwise leave visible
    // spokes in LVGL's software renderer. Only visit the current partial buffer.
    auto bounds=edges(g);
    int32_t cx2=lroundf(g.x*2),cy2=lroundf(g.y*2);
    int64_t inner2=int64_t(ceilf(g.inner*g.inner*4));
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color=color; d.bg_opa=opa;
    int y1=std::max(int(ceilf(g.y-g.r)),int(layer->_clip_area.y1));
    int y2=std::min(int(floorf(g.y+g.r)),int(layer->_clip_area.y2));
    for(int y=y1;y<=y2;y++) {
      float dy=y-g.y,half=sqrtf(std::max(0.0f,g.r*g.r-dy*dy));
      int x1=std::max(int(ceilf(g.x-half)),int(layer->_clip_area.x1));
      int x2=std::min(int(floorf(g.x+half)),int(layer->_clip_area.x2)),run=0;
      bool inRun=false; int64_t dy2=2*y-cy2;
      for(int x=x1;x<=x2+1;x++) {
        int64_t dx2=2*x-cx2;
        bool inside=x<=x2 && dx2*dx2+dy2*dy2>=inner2 &&
                    bounds.contains(dx2,dy2);
        if(inside && !inRun) { run=x; inRun=true; }
        if(!inside && inRun) {
          lv_area_t row={run,y,x-1,y}; lv_draw_rect(layer,&d,&row); inRun=false;
        }
      }
    }
  }
}
static lv_obj_t *button(lv_obj_t *menu,bool center) {
  auto obj=lv_obj_class_create_obj(&slice_class,menu); if(!obj) return nullptr;
  reinterpret_cast<Slice *>(obj)->center=center;
  lv_obj_class_init_obj(obj); lv_obj_remove_style_all(obj);
  lv_obj_set_scrollable(obj,false); lv_obj_set_clickable(obj,true); lv_obj_set_adv_hittest(obj,true);
  lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
  lv_obj_set_style_bg_color(obj,lv_color_hex(0x245878),0);
  lv_obj_set_style_bg_color(obj,lv_color_hex(0x428BAC),LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(obj,lv_color_hex(0x28785B),LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(obj,lv_color_hex(0x384049),LV_STATE_DISABLED);
  lv_obj_set_style_text_color(obj,lv_color_white(),0);
  lv_obj_set_style_text_color(obj,lv_color_hex(0x8B949E),LV_STATE_DISABLED);
  auto label=lv_label_create(obj); reinterpret_cast<Slice *>(obj)->label=label;
  lv_label_set_text(label,""); lv_obj_set_clickable(label,false); lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
  layout(menu); return obj;
}
lv_obj_t *lv_piemenu_create(lv_obj_t *parent) {
  auto obj=lv_obj_class_create_obj(&menu_class,parent); if(!obj) return nullptr;
  lv_obj_class_init_obj(obj); lv_obj_remove_style_all(obj);
  lv_obj_set_scrollable(obj,false); lv_obj_set_clickable(obj,false);
  auto m=reinterpret_cast<Menu *>(obj); m->rotation=-90; m->gap=6;
  lv_obj_set_size(obj,240,240); return obj;
}
lv_obj_t *lv_piemenu_add_button(lv_obj_t *menu) { return count(menu)>=8 ? nullptr : button(menu,false); }
void lv_piemenu_set_inner_radius(lv_obj_t *menu,int32_t radius) {
  auto m=reinterpret_cast<Menu *>(menu); m->inner=std::max(int32_t(0),radius);
  if(m->inner>0 && !m->center) m->center=button(menu,true);
  if(m->center) {
    if(m->inner) lv_obj_set_hidden(m->center,false);
    else lv_obj_set_hidden(m->center,true);
  }
  layout(menu);
}
void lv_piemenu_set_gap(lv_obj_t *menu,float gap) { reinterpret_cast<Menu *>(menu)->gap=std::isfinite(gap)?std::max(0.0f,gap):0; layout(menu); }
void lv_piemenu_set_rotation(lv_obj_t *menu,float angle) { reinterpret_cast<Menu *>(menu)->rotation=std::isfinite(angle)?wrap(angle):0; layout(menu); }
lv_obj_t *lv_piemenu_get_center_button(lv_obj_t *menu) { return reinterpret_cast<Menu *>(menu)->center; }
void lv_piemenu_button_set_text(lv_obj_t *obj,const char *text) { lv_label_set_text(reinterpret_cast<Slice *>(obj)->label,text); layout(lv_obj_get_parent(obj)); }
lv_obj_t *lv_piemenu_button_get_label(lv_obj_t *obj) { return reinterpret_cast<Slice *>(obj)->label; }




