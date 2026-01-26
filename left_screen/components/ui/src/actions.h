#ifndef EEZ_LVGL_UI_ACTIONS_H
#define EEZ_LVGL_UI_ACTIONS_H

#include <lvgl.h>

extern void action_reboot_factory(lv_event_t * e);
extern void action_set_rpm_alarm_override(lv_event_t * e);
extern void action_inc_rpm_spinbox(lv_event_t * e);
extern void action_dec_rpm_spinbox(lv_event_t * e);
extern void action_save_rpm_spinbox(lv_event_t * e);
extern void action_shift_indicator_toggled(lv_event_t * e);
extern void action_blink_rpm_alarm_toggled(lv_event_t * e);
extern void action_reset_settings(lv_event_t * e);
extern void action_decimation_update(lv_event_t * e);
extern void action_buzz_overtemp_toggled(lv_event_t * e);


#endif /*EEZ_LVGL_UI_ACTIONS_H*/