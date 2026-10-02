#include "do_mian.h"

int now_is_auto_play = 0;
namespace {
lv_timer_t* autoplayTimer = nullptr;
uint8_t nextScreen = 0;
bool lastButtonSample = true;
bool stableButton = true;
uint32_t buttonChangedAt = 0;

void pollButton(lv_timer_t*) {
    const bool released = digitalRead(9) != LOW;
    const uint32_t now = millis();
    if (released != lastButtonSample) {
        lastButtonSample = released;
        buttonChangedAt = now;
    }
    if (released != stableButton && (uint32_t)(now - buttonChangedAt) >= 40) {
        stableButton = released;
        if (!released) {
            now_is_auto_play = !now_is_auto_play;
            if (autoplayTimer) {
                nextScreen = 0;
                lv_timer_set_period(autoplayTimer, 2000);
                lv_timer_reset(autoplayTimer);
            }
        }
    }
}

void advanceScreen(lv_timer_t* timer) {
    if (!now_is_auto_play) return;
    lv_obj_t* const screens[] = {
        ui_Screen4, ui_Screen7, ui_Screen11, ui_Screen6,
        ui_Screen8, ui_Screen9, ui_Screen10, ui_Screen5
    };
    const lv_scr_load_anim_t animation = nextScreen < 3
        ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_FADE_ON;
    _ui_screen_change(screens[nextScreen], animation, 500, 0);
    nextScreen = (nextScreen + 1) % 8;
    lv_timer_set_period(timer, nextScreen == 0 ? 2000 : 3000);
}
}

void ui_mian(void) {
    ui_init();
    if (is_auto_play) auto_play();
    // LVGL timers run inside lv_timer_handler; all UI access stays on one task.
    lv_timer_create(pollButton, 10, nullptr);
}

void auto_play(void) {
    if (!autoplayTimer) autoplayTimer = lv_timer_create(advanceScreen, 2000, nullptr);
}
