#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include "vars.h"
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main_scr;
    lv_obj_t *dials_tab;
    lv_obj_t *speed_scale;
    lv_obj_t *speed_arc;
    lv_obj_t *speed;
    lv_obj_t *speed_unit;
    lv_obj_t *fuel_level;
    lv_obj_t *coolant;
    lv_obj_t *low_fuel_tt;
    lv_obj_t *over_temperature_tt;
    lv_obj_t *parkingbrake_tt;
    lv_obj_t *brakes_tt;
    lv_obj_t *abs_tt;
    lv_obj_t *low_coolant_tt;
    lv_obj_t *battery_tt;
    lv_obj_t *low_oil_tt;
    lv_obj_t *mil_tt;
    lv_obj_t *hi_beam_tt;
    lv_obj_t *indicators_tt;
    lv_obj_t *airbag_tt;
    lv_obj_t *odometer;
    lv_obj_t *trip;
    lv_obj_t *can_state;
    lv_obj_t *internal_state;
    lv_obj_t *itf_state;
    lv_obj_t *interlock_state;
    lv_obj_t *voltage_lvl;
    lv_obj_t *settings_tab;
    lv_obj_t *mph_on;
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