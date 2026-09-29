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

inline void setup_ui()
{
#ifdef CONFIG_LEFT_SIDE_DISPLAY
    // lv_obj_set_style_pad_radial(objects.rpm_scale, 10, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    lv_scale_set_text_src(objects.rpm_scale, rpm_scale_labels);

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
    lv_spinbox_set_value(objects.shift_mid_spinbox, display_board_st.shift_mid_threshold);
    lv_spinbox_set_value(objects.shift_top_spinbox, display_board_st.shift_top_threshold);
    lv_spinbox_set_max_value(objects.shift_mid_spinbox, display_board_st.shift_top_threshold - 1);
    lv_spinbox_set_min_value(objects.shift_top_spinbox, display_board_st.shift_mid_threshold + 1);

    // RPM decimation selector
    lv_obj_set_state(objects.decimation_sw, LV_STATE_CHECKED, (display_board_st.rpm_decimation > 10));

    // Gear selector
    lv_obj_set_state(objects.gear_on, LV_STATE_CHECKED, display_board_st.showGearPosition);

    // Buzz on hot selector
    lv_obj_set_state(objects.buzz_overtemp_sw, LV_STATE_CHECKED, display_board_st.overTemp_buzz);

#elifdef CONFIG_RIGHT_SIDE_DISPLAY
    lv_slider_set_value(objects.dark_slider, display_board_st.darkBrightness, LV_ANIM_OFF);
    lv_slider_set_value(objects.light_slider, display_board_st.lightBrightness, LV_ANIM_OFF);
    lv_label_set_text_fmt(objects.l_bright, "%u", display_board_st.lightBrightness);
    lv_label_set_text_fmt(objects.d_bright, "%u", display_board_st.darkBrightness);
    lv_obj_set_state(objects.mode_lock_switch, LV_STATE_CHECKED, display_board_st.modeLocked);
    lv_obj_set_state(objects.theme_switch, LV_STATE_DISABLED, !(display_board_st.modeLocked));
    lv_obj_set_state(objects.theme_switch, LV_STATE_CHECKED, !(display_board_st.lightMode));

    // lv_obj_set_style_pad_radial(objects.speed_scale, 15, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
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

    if (display_board_st.lightMode)
        display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
    else
        display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
}