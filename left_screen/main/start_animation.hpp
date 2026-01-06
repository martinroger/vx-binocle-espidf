#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <ui.h>
#include "lvgl_v9_port.h"
#include "twai_daemon.h"

#include "global_vars.hpp"
#include "theme.hpp"
#include "updateUI.hpp"

inline void startup_anim()
{
#ifdef CONFIG_LEFT_SIDE_DISPLAY
    lv_obj_set_style_pad_radial(objects.rpm_scale, 20, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    lv_scale_set_text_src(objects.rpm_scale, rpm_scale_labels);
    // Prepare according to starting values
    /*    if (display_board_st.rpm_alarm_override)
       {
           lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, true);
           lv_obj_set_state(objects.blink_alarm_sw, LV_STATE_DISABLED, false);
           lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, false);
           lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, false);
           lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, false);
           lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, false);
           lv_spinbox_set_value(objects.rpm_alarm_spinbox, (int32_t)display_board_st.rpm_alarm_threshold);
       }
       else
       {
           lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, false);
           lv_obj_set_state(objects.blink_alarm_sw, LV_STATE_DISABLED, true);
           lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, true);
           lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, true);
           lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, true);
           lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, true);
           lv_spinbox_set_value(objects.rpm_alarm_spinbox, 0);
       } */

    // Settings screen alarm section
    lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, (display_board_st.rpm_alarm_override));
    lv_obj_set_state(objects.blink_alarm_sw, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
    lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
    lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
    lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
    lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
    lv_spinbox_set_value(objects.rpm_alarm_spinbox, (int32_t)display_board_st.rpm_alarm_threshold);
    lv_obj_set_state(objects.blink_alarm_sw, LV_STATE_CHECKED, display_board_st.rpm_alarm_blink);

    // Settings screen shift indicator section
    lv_obj_set_state(objects.shift_ind_sw, LV_STATE_CHECKED, display_board_st.use_shift_indicator);
    // lv_obj_set_state(objects.save_shift_ind_btn, LV_STATE_DISABLED, !(display_board_st.use_shift_indicator));
    // lv_obj_set_state(objects.inc_shift_mid_btn, LV_STATE_DISABLED, !(display_board_st.use_shift_indicator));
    // lv_obj_set_state(objects.dec_shift_mid_btn, LV_STATE_DISABLED, !(display_board_st.use_shift_indicator));
    // lv_obj_set_state(objects.inc_shift_top_btn, LV_STATE_DISABLED, !(display_board_st.use_shift_indicator));
    // lv_obj_set_state(objects.dec_shift_top_btn, LV_STATE_DISABLED, !(display_board_st.use_shift_indicator));

    // lv_obj_set_state(objects.shift_mid_spinbox, LV_STATE_DISABLED, !(display_board_st.use_shift_indicator));
    lv_spinbox_set_value(objects.shift_mid_spinbox, display_board_st.shift_mid_threshold);

    // lv_obj_set_state(objects.shift_top_spinbox, LV_STATE_DISABLED, !(display_board_st.use_shift_indicator));
    lv_spinbox_set_value(objects.shift_top_spinbox, display_board_st.shift_top_threshold);

    // Probably need to set spinbox limits here
    lv_spinbox_set_max_value(objects.shift_mid_spinbox, display_board_st.shift_top_threshold - 1);
    lv_spinbox_set_min_value(objects.shift_top_spinbox, display_board_st.shift_mid_threshold + 1);

#elifdef CONFIG_RIGHT_SIDE_DISPLAY
    lv_slider_set_value(objects.dark_slider, display_board_st.darkBrightness, LV_ANIM_OFF);
    lv_slider_set_value(objects.light_slider, display_board_st.lightBrightness, LV_ANIM_OFF);
    lv_label_set_text_fmt(objects.l_bright, "%u", display_board_st.lightBrightness);
    lv_label_set_text_fmt(objects.d_bright, "%u", display_board_st.darkBrightness);
    lv_obj_set_state(objects.mode_lock_switch, LV_STATE_CHECKED, display_board_st.modeLocked);
    lv_obj_set_state(objects.theme_switch, LV_STATE_DISABLED, !(display_board_st.modeLocked));
    lv_obj_set_state(objects.theme_switch, LV_STATE_CHECKED, !(display_board_st.lightMode));

    lv_obj_set_style_pad_radial(objects.speed_scale, 15, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    if (!(display_board_st.mph_selected))
    {
        lv_scale_set_range(objects.speed_scale, 0, 2400);
        lv_scale_set_total_tick_count(objects.speed_scale, 49);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_kph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 2400);
        lv_label_set_text(objects.speed_unit, "KPH");
        lv_obj_set_state(objects.mph_on, LV_STATE_CHECKED, false);
    }
    else
    {
        lv_scale_set_range(objects.speed_scale, 0, 1600);
        lv_scale_set_total_tick_count(objects.speed_scale, 33);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_mph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 1600);
        lv_label_set_text(objects.speed_unit, "MPH");
        lv_obj_set_state(objects.mph_on, LV_STATE_CHECKED, true);
    }
#endif

    vTaskSuspend(CAN_RX_tsk_hdl);
    // Prepare values for the starting/check sequence
    p_screen_interlock_OK = !screen_interlock_OK;
    lv_obj_set_style_opa(objects.interlock_state, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_internal_ST = 0xFF;
    lv_obj_set_style_opa(objects.internal_state, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_itf_board_st = 0xFF;
    lv_obj_set_style_opa(objects.itf_state, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_CAN_RX_TimedOut = !CAN_RX_TimedOut;
    lv_obj_set_style_opa(objects.can_state, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_indicatorsOn = !indicatorsOn;
    lv_obj_set_style_opa(objects.indicators_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_rightTurnOn = !rightTurnOn;
    p_leftTurnOn = !leftTurnOn;
    p_highBeamOn = !highBeamOn;
    lv_obj_set_style_opa(objects.hi_beam_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_lowFuelOn = !lowFuelOn;
    lv_obj_set_style_opa(objects.low_fuel_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_overTemperatureOn = !overTemperatureOn;
    lv_obj_set_style_opa(objects.over_temperature_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_brakesOn = !brakesOn;
    lv_obj_set_style_opa(objects.brakes_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_absOn = !absOn;
    lv_obj_set_style_opa(objects.abs_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_parkingBrakeOn = !parkingBrakeOn;
    lv_obj_set_style_opa(objects.parkingbrake_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_lowCoolantOn = !lowCoolantOn;
    lv_obj_set_style_opa(objects.low_coolant_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_batteryOn = !batteryOn;
    lv_obj_set_style_opa(objects.battery_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_lowOilOn = !lowOilOn;
    lv_obj_set_style_opa(objects.low_oil_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_milOn = !milOn;
    lv_obj_set_style_opa(objects.mil_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_airbagOn = !airbagOn;
    lv_obj_set_style_opa(objects.airbag_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    lvgl_port_unlock();
    vTaskDelay(100);
    // ESP_LOGI(__func__, "Backlight : %d", board->getBacklight()->on());
    if (display_board_st.lightMode)
        display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
    else
        display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
    // This probably needs to be called in a second point
    lvgl_port_lock(-1);
#ifdef CONFIG_LEFT_SIDE_DISPLAY
    animateTargetArcWithDuration(objects.rpm_arc, 8000, 1000);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(1600));
    lvgl_port_lock(-1);
    animateTargetArcWithDuration(objects.rpm_arc, 0, 500);
#elifdef CONFIG_RIGHT_SIDE_DISPLAY
    animateTargetArcWithDuration(objects.speed_arc, 2400, 1000);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(1600));
    lvgl_port_lock(-1);
    animateTargetArcWithDuration(objects.speed_arc, 0, 500);
#endif
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(600));
    lvgl_port_lock(-1);
    updateLVGLObjects(true);
    lvgl_port_unlock();
}