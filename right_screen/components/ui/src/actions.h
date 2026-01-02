#ifndef EEZ_LVGL_UI_ACTIONS_H
#define EEZ_LVGL_UI_ACTIONS_H

#include <lvgl.h>

extern void action_mph_switch_toggled(lv_event_t * e);
extern void action_reboot_factory(lv_event_t * e);
extern void action_test_brightness(lv_event_t * e);
extern void action_save_brightness(lv_event_t * e);
extern void action_mode_lock_switch_toggled(lv_event_t * e);
extern void action_mode_switch_toggled(lv_event_t * e);


#endif /*EEZ_LVGL_UI_ACTIONS_H*/