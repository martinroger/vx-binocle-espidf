#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include "vars.h"
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main_scr;
    lv_obj_t *main_tabview;
    lv_obj_t *dials_tab;
    lv_obj_t *rpm_scale;
    lv_obj_t *rpm_arc;
    lv_obj_t *rpm;
    lv_obj_t *fuel_level;
    lv_obj_t *coolant;
    lv_obj_t *low_fuel_tt;
    lv_obj_t *over_temperature_tt;
    lv_obj_t *low_coolant_tt;
    lv_obj_t *battery_tt;
    lv_obj_t *low_oil_tt;
    lv_obj_t *mil_tt;
    lv_obj_t *can_state;
    lv_obj_t *internal_state;
    lv_obj_t *itf_state;
    lv_obj_t *interlock_state;
    lv_obj_t *settings_tab;
    lv_obj_t *decimation_sw;
    lv_obj_t *override_alarm_sw;
    lv_obj_t *rpm_alarm_spinbox;
    lv_obj_t *inc_rpm_alarm_btn;
    lv_obj_t *dec_rpm_alarm_btn;
    lv_obj_t *save_rpm_alarm_btn;
    lv_obj_t *blink_alarm_sw;
    lv_obj_t *shift_ind_sw;
    lv_obj_t *shift_mid_spinbox;
    lv_obj_t *inc_shift_mid_btn;
    lv_obj_t *dec_shift_mid_btn;
    lv_obj_t *shift_top_spinbox;
    lv_obj_t *inc_shift_top_btn;
    lv_obj_t *dec_shift_top_btn;
    lv_obj_t *save_shift_ind_btn;
    lv_obj_t *buzz_overtemp_sw;
    lv_obj_t *debug_tab;
    lv_obj_t *voltage_lvl;
    lv_obj_t *odometer;
    lv_obj_t *trip;
    lv_obj_t *gr_raw;
    lv_obj_t *gr_filt;
    lv_obj_t *gr_diff;
    lv_obj_t *gr_pos;
    lv_obj_t *airbag_tt;
    lv_obj_t *abs_tt;
    lv_obj_t *brakes_tt;
    lv_obj_t *parkingbrake_tt;
    lv_obj_t *hi_beam_tt;
    lv_obj_t *indicators_tt;
    lv_obj_t *version_info;
    lv_obj_t *project_info;
    lv_obj_t *current_partition;
    lv_obj_t *reboot_factory;
    lv_obj_t *reset_settings_btn;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN_SCR = 1,
};

void create_screen_main_scr();
void tick_screen_main_scr();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/