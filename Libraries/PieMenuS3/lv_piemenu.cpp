#include "lv_piemenu.h"
#include <lvgl_private.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

struct Menu { lv_obj_t obj; int32_t inner; float gap, rotation; lv_obj_t *center; bool laying, fill; };
struct Slice { lv_obj_t obj; lv_obj_t *label; unsigned index; bool center; };
static void slice_event(const lv_obj_class_t *, lv_event_t *);
static void menu_event(const lv_obj_class_t *, lv_event_t *);
static lv_obj_class_t make_class(size_t size, const char *name,
                                void (*cb)(const lv_obj_class_t *, lv_event_t *)) {
  lv_obj_class_t c = {}; c.base_class = &lv_obj_class; c.instance_size = size;
  c.name = name; c.event_cb = cb; c.width_def = LV_PCT(100); c.height_def = LV_PCT(100); return c;
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
// r is the circle fitting the smaller dimension; hw/hh are the half-extents
// between pixel centers, used when fill extends slices to the rectangle.
// Measured from the menu: each slice object only spans its own bounding box.
struct Geometry { float x,y,r,inner,start,sweep,gap,hw,hh; bool fill; };
static Geometry geometry(lv_obj_t *obj) {
  auto s=reinterpret_cast<Slice *>(obj);
  auto m=reinterpret_cast<Menu *>(lv_obj_get_parent(obj));
  lv_area_t a; lv_obj_get_coords(&m->obj,&a);
  float r=std::max(0.0f,std::min(lv_obj_get_width(&m->obj),lv_obj_get_height(&m->obj))/2.0f-0.5f);
  unsigned n=count(&m->obj);
  float step=360.0f/std::max(1u,n);
  float gap=n<2 ? 0 : std::min(m->gap,0.4f*r*sinf(pi/n));
  return {(a.x1+a.x2)/2.0f,(a.y1+a.y2)/2.0f,r,
          std::min(float(m->inner),r*0.7f),m->rotation+s->index*step,step,gap,
          (a.x2-a.x1)/2.0f,(a.y2-a.y1)/2.0f,m->fill};
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
// Offsets from the center are doubled so pixel centers stay integral.
// Bounds are rounded once per slice; the per-pixel test is integer-only.
struct Shape {
  Edges b; int64_t hw2,hh2,outer2,inner2; bool fill;
  bool contains(int64_t dx2,int64_t dy2) const {
    int64_t d2=dx2*dx2+dy2*dy2;
    bool outer=fill ? std::llabs(dx2)<=hw2 && std::llabs(dy2)<=hh2 : d2<=outer2;
    return outer && d2>=inner2 && b.contains(dx2,dy2);
  }
};
static Shape shape(const Geometry &g) {
  return {edges(g),lroundf(g.hw*2),lroundf(g.hh*2),int64_t(floorf(g.r*g.r*4)),
          int64_t(ceilf(g.inner*g.inner*4)),g.fill};
}
// Area centroid of a clipped slice, sampled every four pixels, as an offset
// from the menu center. Rectangle corners pull it outward along the slice.
static lv_point_t centroid(const Geometry &g) {
  auto sh=shape(g); int64_t sx=0,sy=0,n=0;
  for(int64_t dy2=-sh.hh2;dy2<=sh.hh2;dy2+=8) for(int64_t dx2=-sh.hw2;dx2<=sh.hw2;dx2+=8)
    if(sh.contains(dx2,dy2)) { sx+=dx2; sy+=dy2; ++n; }
  if(!n) return {0,0};
  return {int32_t(lroundf(sx/(2.0f*n))),int32_t(lroundf(sy/(2.0f*n)))};
}
// Conservative absolute bounding box of a slice: the center, the outer
// boundary at both edges, and axis points (round) or corners (fill) within
// the sweep. Gaps and the hole only shrink the slice. State changes then
// redraw just this box instead of the whole menu.
static lv_area_t bounds(const Slice *s,const Geometry &g) {
  float x1=g.x,y1=g.y,x2=g.x,y2=g.y;
  auto add=[&](float px,float py) { x1=std::min(x1,px); y1=std::min(y1,py); x2=std::max(x2,px); y2=std::max(y2,py); };
  auto ray=[&](float deg) {
    float c=cosf(deg*pi/180),sn=sinf(deg*pi/180),t=g.r;
    if(g.fill) t=std::min(fabsf(c)>1e-6f ? g.hw/fabsf(c) : 1e9f,fabsf(sn)>1e-6f ? g.hh/fabsf(sn) : 1e9f);
    add(g.x+t*c,g.y+sn*t);
  };
  auto within=[&](float deg) { return wrap(deg-g.start)<=g.sweep; };
  if(s->center) { add(g.x-g.inner,g.y-g.inner); add(g.x+g.inner,g.y+g.inner); }
  else {
    ray(g.start); ray(g.start+g.sweep);
    for(int k=0;k<4;k++) {
      float sx=(k&1)?1:-1,sy=(k&2)?1:-1;
      if(g.fill) { if(within(atan2f(sy*g.hh,sx*g.hw)*180/pi)) add(g.x+sx*g.hw,g.y+sy*g.hh); }
      else if(within(k*90.0f)) ray(k*90.0f);
    }
  }
  return {int32_t(floorf(x1))-1,int32_t(floorf(y1))-1,int32_t(ceilf(x2))+1,int32_t(ceilf(y2))+1};
}
static void layout(lv_obj_t *obj) {
  auto m=reinterpret_cast<Menu *>(obj); if(m->laying) return; m->laying=true;
  lv_area_t ma; lv_obj_get_coords(obj,&ma);
  int32_t mw=lv_obj_get_width(obj),mh=lv_obj_get_height(obj);
  unsigned idx=0;
  for(unsigned i=0;i<lv_obj_get_child_count(obj);i++) {
    auto child=lv_obj_get_child(obj,i); if(!lv_obj_check_type(child,&slice_class)) continue;
    auto s=reinterpret_cast<Slice *>(child); if(!s->center) s->index=idx++;
    auto g=geometry(child); lv_area_t b=bounds(s,g);
    b.x1=std::max(b.x1,ma.x1); b.y1=std::max(b.y1,ma.y1); b.x2=std::min(b.x2,ma.x2); b.y2=std::min(b.y2,ma.y2);
    int32_t px=b.x1-ma.x1,py=b.y1-ma.y1,w=std::max(int32_t(1),lv_area_get_width(&b)),h=std::max(int32_t(1),lv_area_get_height(&b));
    // Visible slices are layout-positioned, so LVGL ignores their x/y styles:
    // move their coordinates directly, as LVGL's flex layout does. Hidden
    // ones (the full-pie center) are placed by LVGL from x/y; moving those
    // would be undone every pass and the layout would never settle.
    lv_obj_set_size(child,w,h); lv_obj_refr_size(child);
    if(!lv_obj_is_layout_positioned(child)) lv_obj_set_pos(child,px,py);
    int32_t dx=b.x1-child->coords.x1,dy=b.y1-child->coords.y1;
    if(lv_obj_is_layout_positioned(child) && (dx || dy)) {
      lv_obj_invalidate(child); lv_area_move(&child->coords,dx,dy); lv_obj_invalidate(child);
      lv_obj_move_children_by(child,dx,dy,false);
    }
    if(!s->label) continue;
    lv_point_t offset={0,0};
    if(s->center) {}
    else if(g.fill) offset=centroid(g);
    else {
      float angle=(g.start+g.sweep/2)*pi/180;
      float radius=g.inner>0 ? (g.r+g.inner)/2 : g.r*0.62f;
      offset={int32_t(lroundf(radius*cosf(angle))),int32_t(lroundf(radius*sinf(angle)))};
    }
    // Offsets are from the menu center; re-express them from the box center,
    // using LVGL's own center-alignment rounding.
    lv_obj_align(s->label,LV_ALIGN_CENTER,offset.x+mw/2-px-w/2,offset.y+mh/2-py-h/2);
    lv_obj_invalidate(child);
  }
  m->laying=false;
}
static void menu_event(const lv_obj_class_t *c,lv_event_t *e) {
  if(lv_obj_event_base(c,e)!=LV_RESULT_OK) return;
  auto code=lv_event_get_code(e);
  if(code==LV_EVENT_SIZE_CHANGED || code==LV_EVENT_CHILD_CHANGED) lv_obj_mark_layout_as_dirty(lv_event_get_target_obj(e));
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
    float cr=std::max(0.0f,g.inner-1);
    info->res = s->center ? (cr>0 && r2<=cr*cr) :
      shape(g).contains(lroundf(dx*2),lroundf(dy*2));
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
    auto sh=shape(g);
    int32_t cx2=lroundf(g.x*2),cy2=lroundf(g.y*2);
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color=color; d.bg_opa=opa;
    float ry=g.fill ? g.hh : g.r;
    int y1=std::max(int(ceilf(g.y-ry)),int(layer->_clip_area.y1));
    int y2=std::min(int(floorf(g.y+ry)),int(layer->_clip_area.y2));
    for(int y=y1;y<=y2;y++) {
      float dy=y-g.y,half=g.fill ? g.hw : sqrtf(std::max(0.0f,g.r*g.r-dy*dy));
      int x1=std::max(int(ceilf(g.x-half)),int(layer->_clip_area.x1));
      int x2=std::min(int(floorf(g.x+half)),int(layer->_clip_area.x2)),run=0;
      bool inRun=false; int64_t dy2=2*y-cy2;
      // Each edge of a slice up to 180 degrees bounds x on this row. Scan only
      // that window, widened by two pixels; contains() still decides exactly.
      if(!sh.b.wide) {
        double lo=-1e9,hi=1e9;
        auto clamp=[&](double k,double c,bool upper) { (upper ? hi : lo)=upper ? std::min(hi,c/k) : std::max(lo,c/k); };
        const Edges &b=sh.b;
        if(b.ay) clamp(b.ay,double(b.ax)*dy2-b.inset,b.ay>0);
        if(b.by) clamp(b.by,double(b.bx)*dy2+b.inset,b.by<0);
        x1=std::max(x1,int(std::max(-1e6,floor((lo+cx2)/2)-2)));
        x2=std::min(x2,int(std::min(1e6,ceil((hi+cx2)/2)+2)));
      }
      for(int x=x1;x<=x2+1;x++) {
        bool in=x<=x2 && sh.contains(2*x-cx2,dy2);
        if(in && !inRun) { run=x; inRun=true; }
        if(!in && inRun) {
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
  // Labels may extend past a slice's tight bounding box; never clip them.
  lv_obj_set_overflow_visible(obj,true);
  lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
  lv_obj_set_style_bg_color(obj,lv_color_hex(0x245878),0);
  lv_obj_set_style_bg_color(obj,lv_color_hex(0x428BAC),LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(obj,lv_color_hex(0x28785B),LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(obj,lv_color_hex(0x384049),LV_STATE_DISABLED);
  lv_obj_set_style_text_color(obj,lv_color_white(),0);
  lv_obj_set_style_text_color(obj,lv_color_hex(0x8B949E),LV_STATE_DISABLED);
  auto label=lv_label_create(obj); reinterpret_cast<Slice *>(obj)->label=label;
  lv_label_set_text(label,""); lv_obj_set_clickable(label,false); lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_mark_layout_as_dirty(menu); return obj;
}
lv_obj_t *lv_piemenu_create(lv_obj_t *parent) {
  auto obj=lv_obj_class_create_obj(&menu_class,parent); if(!obj) return nullptr;
  lv_obj_class_init_obj(obj); lv_obj_remove_style_all(obj);
  lv_obj_set_scrollable(obj,false); lv_obj_set_clickable(obj,false);
  auto m=reinterpret_cast<Menu *>(obj); m->rotation=-90; m->gap=6;
  // A registered LVGL layout: setters only mark the menu dirty, and slices
  // are placed once per lv_obj_update_layout() or display refresh.
  static uint32_t layout_id=lv_layout_create({[](lv_obj_t *o,void *) { layout(o); },nullptr},nullptr);
  lv_obj_set_style_layout(obj,layout_id,0);
  lv_obj_set_size(obj,LV_PCT(100),LV_PCT(100)); return obj;
}
lv_obj_t *lv_piemenu_add_button(lv_obj_t *menu) { return count(menu)>=8 ? nullptr : button(menu,false); }
void lv_piemenu_set_inner_radius(lv_obj_t *menu,int32_t radius) {
  auto m=reinterpret_cast<Menu *>(menu); m->inner=std::max(int32_t(0),radius);
  if(m->inner>0 && !m->center) m->center=button(menu,true);
  if(m->center) {
    if(m->inner) lv_obj_set_hidden(m->center,false);
    else lv_obj_set_hidden(m->center,true);
  }
  lv_obj_mark_layout_as_dirty(menu);
}
void lv_piemenu_set_gap(lv_obj_t *menu,float gap) { reinterpret_cast<Menu *>(menu)->gap=std::isfinite(gap)?std::max(0.0f,gap):0; lv_obj_mark_layout_as_dirty(menu); }
void lv_piemenu_set_fill(lv_obj_t *menu,bool fill) { reinterpret_cast<Menu *>(menu)->fill=fill; lv_obj_mark_layout_as_dirty(menu); }
bool lv_piemenu_get_fill(lv_obj_t *menu) { return reinterpret_cast<Menu *>(menu)->fill; }
void lv_piemenu_set_rotation(lv_obj_t *menu,float angle) { reinterpret_cast<Menu *>(menu)->rotation=std::isfinite(angle)?wrap(angle):0; lv_obj_mark_layout_as_dirty(menu); }
lv_obj_t *lv_piemenu_get_center_button(lv_obj_t *menu) { return reinterpret_cast<Menu *>(menu)->center; }
void lv_piemenu_button_set_text(lv_obj_t *obj,const char *text) { lv_label_set_text(reinterpret_cast<Slice *>(obj)->label,text); lv_obj_mark_layout_as_dirty(lv_obj_get_parent(obj)); }
lv_obj_t *lv_piemenu_button_get_label(lv_obj_t *obj) { return reinterpret_cast<Slice *>(obj)->label; }
