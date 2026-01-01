#ifndef EEZ_LVGL_UI_ACTIONS_H
#define EEZ_LVGL_UI_ACTIONS_H

#include <lvgl.h>

extern void action_reboot_factory(lv_event_t * e);
extern void action_set_rpm_alarm_override(lv_event_t * e);
extern void action_inc_rpm_spinbox(lv_event_t * e);
extern void action_dec_rpm_spinbox(lv_event_t * e);
extern void action_save_rpm_spinbox(lv_event_t * e);
extern void action_save_brightness(lv_event_t * e);
extern void action_test_brightness(lv_event_t * e);


#endif /*EEZ_LVGL_UI_ACTIONS_H*/