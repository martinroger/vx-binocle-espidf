#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <ui.h>
#include "ui_lock.hpp"
#include "twai_daemon.h"

#include "global_vars.hpp"
#include "theme.hpp"
#include "updateUI.hpp"

inline void setupUi()
{
#ifdef CONFIG_LEFT_SIDE_DISPLAY
    // lv_obj_set_style_pad_radial(objects.rpm_scale, 10, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    lv_scale_set_text_src(objects.rpm_scale, rpm_scale_labels);

    // Settings screen alarm section
    lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, (displayBoard.alarmOverride));
    lv_obj_set_state(objects.blink_alarm_sw, LV_STATE_DISABLED, !(displayBoard.alarmOverride));
    lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, !(displayBoard.alarmOverride));
    lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, !(displayBoard.alarmOverride));
    lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, !(displayBoard.alarmOverride));
    lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, !(displayBoard.alarmOverride));
    lv_spinbox_set_value(objects.rpm_alarm_spinbox, (int32_t)displayBoard.alarmRpm);
    lv_obj_set_state(objects.blink_alarm_sw, LV_STATE_CHECKED, displayBoard.alarmBlink);

    // Settings screen shift indicator section
    lv_obj_set_state(objects.shift_ind_sw, LV_STATE_CHECKED, displayBoard.shiftEnabled);

    // Sanity check shift thresholds (must be mid < top) and reset NVS if invalid
    if (displayBoard.shiftMidRpm >= displayBoard.shiftTopRpm || displayBoard.shiftMidRpm < 1000)
    {
        ESP_LOGW(__func__, "Invalid shift thresholds (mid=%lu, top=%lu), resetting to defaults 5500/6500",
                 (unsigned long)displayBoard.shiftMidRpm, (unsigned long)displayBoard.shiftTopRpm);
        displayBoard.shiftMidRpm = 5500;
        displayBoard.shiftTopRpm = 6500;
        nvs_handle_t h;
        if (nvs_open(vx::nvs::disp::NS, NVS_READWRITE, &h) == ESP_OK)
        {
            nvs_set_u32(h, vx::nvs::disp::SHIFT_MID_RPM, displayBoard.shiftMidRpm);
            nvs_set_u32(h, vx::nvs::disp::SHIFT_TOP_RPM, displayBoard.shiftTopRpm);
            nvs_commit(h);
            nvs_close(h);
        }
    }

    lv_spinbox_set_value(objects.shift_mid_spinbox, displayBoard.shiftMidRpm);
    lv_spinbox_set_value(objects.shift_top_spinbox, displayBoard.shiftTopRpm);
    lv_spinbox_set_max_value(objects.shift_mid_spinbox, displayBoard.shiftTopRpm - 1);
    lv_spinbox_set_min_value(objects.shift_top_spinbox, displayBoard.shiftMidRpm + 1);

    // RPM decimation selector
    lv_obj_set_state(objects.decimation_sw, LV_STATE_CHECKED, (displayBoard.rpmDecim > 10));

    // Gear selector
    lv_obj_set_state(objects.gear_on, LV_STATE_CHECKED, displayBoard.gearShow);

    // Buzz on hot selector
    lv_obj_set_state(objects.buzz_overtemp_sw, LV_STATE_CHECKED, displayBoard.hotBuzzEnabled);

#elifdef CONFIG_RIGHT_SIDE_DISPLAY
    lv_slider_set_value(objects.dark_slider, displayBoard.blDarkPct, LV_ANIM_OFF);
    lv_slider_set_value(objects.light_slider, displayBoard.blLightPct, LV_ANIM_OFF);
    lv_label_set_text_fmt(objects.l_bright, "%u", displayBoard.blLightPct);
    lv_label_set_text_fmt(objects.d_bright, "%u", displayBoard.blDarkPct);
    lv_obj_set_state(objects.mode_lock_switch, LV_STATE_CHECKED, displayBoard.themeLocked);
    lv_obj_set_state(objects.theme_switch, LV_STATE_DISABLED, !(displayBoard.themeLocked));
    lv_obj_set_state(objects.theme_switch, LV_STATE_CHECKED, !(displayBoard.themeLight));

    // lv_obj_set_style_pad_radial(objects.speed_scale, 15, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    if (!(displayBoard.unitMph))
    {
        lv_scale_set_range(objects.speed_scale, 0, 2400);
        lv_scale_set_total_tick_count(objects.speed_scale, 49);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speedKphScaleLabels);
        lv_arc_set_range(objects.speed_arc, 0, 2400);
        lv_label_set_text(objects.speed_unit, "KPH");
        lv_obj_set_state(objects.mph_on, LV_STATE_CHECKED, false);
    }
    else
    {
        lv_scale_set_range(objects.speed_scale, 0, 1600);
        lv_scale_set_total_tick_count(objects.speed_scale, 33);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speedMphScaleLabels);
        lv_arc_set_range(objects.speed_arc, 0, 1600);
        lv_label_set_text(objects.speed_unit, "MPH");
        lv_obj_set_state(objects.mph_on, LV_STATE_CHECKED, true);
    }
#endif
}