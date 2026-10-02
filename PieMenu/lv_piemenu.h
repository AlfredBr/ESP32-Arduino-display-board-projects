#pragma once
#include <lvgl.h>
// LVGL 9.6. Angles are clockwise; zero points right.
lv_obj_t *lv_piemenu_create(lv_obj_t *parent);
// Equal slices, up to eight. Returns nullptr when full.
lv_obj_t *lv_piemenu_add_button(lv_obj_t *menu);
// Zero selects full pie; positive pixels creates/reuses a center button.
void lv_piemenu_set_inner_radius(lv_obj_t *menu, int32_t radius);
// Constant separator width in pixels (default 6); zero removes separators.
void lv_piemenu_set_gap(lv_obj_t *menu, float pixels);
void lv_piemenu_set_rotation(lv_obj_t *menu, float degrees);
lv_obj_t *lv_piemenu_get_center_button(lv_obj_t *menu);
void lv_piemenu_button_set_text(lv_obj_t *button, const char *text);
lv_obj_t *lv_piemenu_button_get_label(lv_obj_t *button);

