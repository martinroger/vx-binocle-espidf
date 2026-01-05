#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "coefficients.h"

#include "global_vars.hpp"
#include "theme.hpp"
#include "twai_daemon.h"

/// @brief Updates all cyclic elements
/// @param forceRefresh Force a refresh of all the conditional blocks
/// @return Number of updated elements
inline int updateLVGLObjects(bool forceRefresh = false)
{
    int updatedElements = 0;

#ifdef CONFIG_RIGHT_SIDE_DISPLAY

    if ((long)(p_speed_kph * 10) != (long)(speed_kph * 10) || forceRefresh)
    {
        // lv_arc_set_value(objects.itf_speed_kph_arc, speed_kph);
        animateTargetArc(objects.speed_arc, (speed_kph / ((display_board_st.mph_selected) ? COEFF_MPH_TO_KPH : 1)) * 10);
        // // lv_arc_align_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_arc_rotate_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_scale_set_line_needle_value(objects.speed_scale, objects.itf_speed_kph_needle, 230, speed_kph);
        // // lv_scale_set_line_needle_value(objects.speed_scale,needleLine,-8,speed_kph);
        // lv_scale_set_image_needle_value(objects.speed_scale, objects.simple_needle, (long)(speed_kph * 10));
        if ((long)round(speed_kph) != (long)round(p_speed_kph))
            lv_label_set_text_fmt(objects.speed, "%03ld", (long)round(speed_kph / ((display_board_st.mph_selected) ? COEFF_MPH_TO_KPH : 1)));
        p_speed_kph = speed_kph;
        updatedElements++;
    }

    if ((display_board_st.modeLocked != (lv_obj_has_state(objects.mode_lock_switch, LV_STATE_CHECKED))) || forceRefresh)
    {
        lv_obj_set_state(objects.mode_lock_switch, LV_STATE_CHECKED, display_board_st.modeLocked);
        lv_obj_set_state(objects.theme_switch, LV_STATE_DISABLED, !(display_board_st.modeLocked));
        lv_obj_set_state(objects.theme_switch, LV_STATE_CHECKED, !(display_board_st.lightMode));
        updatedElements++;
    }
    if ((display_board_st.darkBrightness) != lv_slider_get_value(objects.dark_slider) || forceRefresh)
    {
        lv_label_set_text_fmt(objects.d_bright, "%u", display_board_st.darkBrightness);
        lv_slider_set_value(objects.dark_slider, display_board_st.darkBrightness, LV_ANIM_OFF);
        updatedElements++;
    }
    if ((display_board_st.lightBrightness) != lv_slider_get_value(objects.light_slider) || forceRefresh)
    {
        lv_label_set_text_fmt(objects.l_bright, "%u", display_board_st.lightBrightness);
        lv_slider_set_value(objects.light_slider, display_board_st.lightBrightness, LV_ANIM_OFF);
        updatedElements++;
    }

#elifdef CONFIG_LEFT_SIDE_DISPLAY
    if ((p_rpm / 10) != (rpm / 10) || forceRefresh)
    {
        // lv_arc_set_value(objects.itf_speed_kph_arc, speed_kph);
        animateTargetArc(objects.rpm_arc, rpm);
        // // lv_arc_align_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_arc_rotate_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_scale_set_line_needle_value(objects.speed_scale, objects.itf_speed_kph_needle, 230, speed_kph);
        // // lv_scale_set_line_needle_value(objects.speed_scale,needleLine,-8,speed_kph);
        // lv_scale_set_image_needle_value(objects.speed_scale, objects.simple_needle, (long)(speed_kph * 10));
        lv_label_set_text_fmt(objects.rpm, "%04ld", (long)((rpm / 10) * 10));
        lv_obj_set_state(objects.rpm, LV_STATE_FOCUSED, (display_board_st.rpm_alarm_override) ? (rpm > display_board_st.rpm_alarm_threshold) : alarmOn);

        p_rpm = rpm;
        updatedElements++;
    }
    if (((p_alarmOn != alarmOn) && !(display_board_st.rpm_alarm_override)) || forceRefresh)
    {
        lv_obj_set_state(objects.rpm, LV_STATE_FOCUSED, alarmOn);
        p_alarmOn = alarmOn;
        updatedElements++;
    }

    if (display_board_st.use_shift_indicator              // Shift indicator is ON
        && !lowFuelOn                                     // Low fuel alarm is OFF
        && !overTemperatureOn                             // Over temperature alarm is OFF
        && screen_interlock_OK                            // Screen interlock is OK
        && !CAN_RX_TimedOut                               // CAN is not timed out
        && (display_board_st.internal_ST == XDB_SM_ST_OK) // Degraded flag is not set
        && (itf_board_st == XDB_SM_ST_OK)                 // ITF board is OK
    )
    {
        if (rpm >= display_board_st.shift_low_threshold)
        {
            if (rpm >= display_board_st.shift_top_threshold)
                lv_obj_set_style_text_color(objects.shift_label, lv_color_hex(0xFF3A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
            else if (rpm >= display_board_st.shift_mid_threshold)
                lv_obj_set_style_text_color(objects.shift_label, lv_color_hex(0xFFAB00), LV_PART_MAIN | LV_STATE_DEFAULT);
            else
                lv_obj_set_style_text_color(objects.shift_label, lv_color_hex(0x00B734), LV_PART_MAIN | LV_STATE_DEFAULT);
            // Controlled by blinker timer in the future
            lv_obj_set_style_opa(objects.shift_label, LV_OPA_COVER, LV_STATE_DEFAULT);
        }
        else
        {
            lv_obj_set_style_opa(objects.shift_label, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        }
    }
    else
    {
        lv_obj_set_style_opa(objects.shift_label, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    }

#endif
    if (CAN_RX_TimedOut) // Turn all on if there is a CAN Timeout
    {
        screen_interlock_OK = false;
        indicatorsOn = true;
        rightTurnOn = true;
        leftTurnOn = true;
        highBeamOn = true;
        brakesOn = true;
        absOn = true;
        parkingBrakeOn = true;
        lowCoolantOn = true;
        batteryOn = true;
        lowOilOn = true;
        milOn = true;
        airbagOn = true;
        ESP_LOGD(__func__, "Turn all on - placeholder");
    }

    if (p_fuelLevel_pc != fuelLevel_pc || forceRefresh) // Fuel level percentage
    {
        // lv_bar_set_value(objects.fuel_bar, fuelLevel_pc, LV_ANIM_OFF);
        lv_label_set_text_fmt(objects.fuel_level, "%03d", fuelLevel_pc);
        p_fuelLevel_pc = fuelLevel_pc;
        updatedElements++;
    }
    if (p_coolant_degC != coolant_degC || forceRefresh) // Coolant temperature
    {
        // lv_bar_set_value(objects.coolant_bar, coolant_degC, LV_ANIM_OFF);
        lv_label_set_text_fmt(objects.coolant, "%03d", coolant_degC);
        p_coolant_degC = coolant_degC;
        updatedElements++;
    }
    if (p_lvVoltage_v != lvVoltage_v || forceRefresh) // 12V Voltage value
    {
        lv_label_set_text_fmt(objects.voltage_lvl, "%04.1fV", lvVoltage_v);
        p_lvVoltage_v = lvVoltage_v;
        updatedElements++;
    }
    if (p_lowFuelOn != lowFuelOn || forceRefresh) // Low Fuel computed TT
    {
        lv_obj_set_style_image_opa(objects.low_fuel_tt, lowFuelOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_state(objects.fuel_level, LV_STATE_FOCUSED, lowFuelOn);
        p_lowFuelOn = lowFuelOn;
        updatedElements++;
    }
    if (p_overTemperatureOn != overTemperatureOn || forceRefresh) // Over temperature computer TT
    {
        lv_obj_set_style_image_opa(objects.over_temperature_tt, overTemperatureOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_state(objects.coolant, LV_STATE_FOCUSED, overTemperatureOn);
        p_overTemperatureOn = overTemperatureOn;
        updatedElements++;
    }
    lv_obj_set_style_opa(objects.interlock_state, screen_interlock_OK ? LV_OPA_TRANSP : LV_OPA_COVER, LV_STATE_DEFAULT);
    // if (p_screen_interlock_OK != screen_interlock_OK) // Left to right screen interlock
    // {
    //     lv_obj_set_style_opa(objects.interlock_state, screen_interlock_OK ? LV_OPA_TRANSP : LV_OPA_COVER, LV_STATE_DEFAULT);
    //     p_screen_interlock_OK = screen_interlock_OK;
    //     updatedElements++;
    // }
    if (p_internal_ST != display_board_st.internal_ST || forceRefresh) // Internal state degraded or fault
    {
        switch (display_board_st.internal_ST)
        {
        case XDB_SM_ST_DEGRADED:
            lv_obj_set_style_text_color(objects.internal_state, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_opa(objects.internal_state, LV_OPA_COVER, LV_STATE_DEFAULT);
            break;
        case XDB_SM_ST_FAULT:
            lv_obj_set_style_text_color(objects.internal_state, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_opa(objects.internal_state, LV_OPA_COVER, LV_STATE_DEFAULT);
            break;
        default:
            lv_obj_set_style_opa(objects.internal_state, LV_OPA_TRANSP, LV_STATE_DEFAULT);
            break;
        }
        p_internal_ST = display_board_st.internal_ST;
        updatedElements++;
    }
    if (p_itf_board_st != itf_board_st || forceRefresh) // Same for CAN-detected ITF board state
    {
        switch (itf_board_st)
        {
        case XDB_SM_ST_DEGRADED:
            lv_obj_set_style_text_color(objects.itf_state, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_opa(objects.itf_state, LV_OPA_COVER, LV_STATE_DEFAULT);
            break;
        case XDB_SM_ST_FAULT:
            lv_obj_set_style_text_color(objects.itf_state, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_opa(objects.itf_state, LV_OPA_COVER, LV_STATE_DEFAULT);
            break;
        default:
            lv_obj_set_style_opa(objects.itf_state, LV_OPA_TRANSP, LV_STATE_DEFAULT);
            break;
        }
        p_itf_board_st = itf_board_st;
        updatedElements++;
    }
    if (CAN_RX_TimedOut != p_CAN_RX_TimedOut || forceRefresh) // Same for CAN Timed out indicator
    {
        lv_obj_set_style_opa(objects.can_state, CAN_RX_TimedOut ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_CAN_RX_TimedOut = CAN_RX_TimedOut;
        updatedElements++;
    }
    if (p_odometer_km != odometer_km || forceRefresh) // Odometer. Might need comparison at the uint level
    {
        lv_label_set_text_fmt(objects.odometer, "%06.0f", odometer_km / ((display_board_st.mph_selected == 1) ? COEFF_MPH_TO_KPH : 1));
        p_odometer_km = odometer_km;
        updatedElements++;
    }
    if (p_trip_km != trip_km || forceRefresh) // Trip, might need comparison at the uint level
    {
        lv_label_set_text_fmt(objects.trip, "%05.1f", trip_km / ((display_board_st.mph_selected == 1) ? COEFF_MPH_TO_KPH : 1));
        p_trip_km = trip_km;
        updatedElements++;
    }
    if (p_indicatorsOn != indicatorsOn || forceRefresh) // Double indicators arrow
    {
        lv_obj_set_style_image_opa(objects.indicators_tt, indicatorsOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_indicatorsOn = indicatorsOn;
        updatedElements++;
    }
    if (p_rightTurnOn != rightTurnOn || forceRefresh) // Right indicator
    {
        // lv_obj_set_style_image_opa(objects.indicators_tt, indicatorsOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_rightTurnOn = rightTurnOn;
        updatedElements++;
    }
    if (p_leftTurnOn != leftTurnOn || forceRefresh) // Right indicator
    {
        // lv_obj_set_style_image_opa(objects.indicators_tt, indicatorsOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_leftTurnOn = leftTurnOn;
        updatedElements++;
    }
    if (p_highBeamOn != highBeamOn || forceRefresh) // High beams
    {
        lv_obj_set_style_image_opa(objects.hi_beam_tt, highBeamOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_highBeamOn = highBeamOn;
        updatedElements++;
    }
    if (p_brakesOn != brakesOn || forceRefresh) // Brakes
    {
        lv_obj_set_style_image_opa(objects.brakes_tt, brakesOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_brakesOn = brakesOn;
        updatedElements++;
    }
    if (p_absOn != absOn || forceRefresh) // ABS
    {
        lv_obj_set_style_image_opa(objects.abs_tt, absOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_absOn = absOn;
        updatedElements++;
    }
    if (p_parkingBrakeOn != parkingBrakeOn || forceRefresh) // Parking Brake
    {
        lv_obj_set_style_image_opa(objects.parkingbrake_tt, parkingBrakeOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_parkingBrakeOn = parkingBrakeOn;
        updatedElements++;
    }
    if (p_lowCoolantOn != lowCoolantOn || forceRefresh) // Low Coolant
    {
        lv_obj_set_style_image_opa(objects.low_coolant_tt, lowCoolantOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_lowCoolantOn = lowCoolantOn;
        updatedElements++;
    }
    if (p_batteryOn != batteryOn || forceRefresh) // Battery/Alternator
    {
        lv_obj_set_style_image_opa(objects.battery_tt, batteryOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_batteryOn = batteryOn;
        updatedElements++;
    }
    if (p_lowOilOn != lowOilOn || forceRefresh) // Low Oil Pressure
    {
        lv_obj_set_style_image_opa(objects.low_oil_tt, lowOilOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_lowOilOn = lowOilOn;
        updatedElements++;
    }
    if (p_milOn != milOn || forceRefresh) // MIL
    {
        lv_obj_set_style_image_opa(objects.mil_tt, milOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_milOn = milOn;
        updatedElements++;
    }
    if (p_airbagOn != airbagOn || forceRefresh) // Airbag
    {
        lv_obj_set_style_image_opa(objects.airbag_tt, airbagOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_airbagOn = airbagOn;
        updatedElements++;
    }
    if (p_headlightsOn != headlightsOn || forceRefresh) // Headlights
    {
        if (!(display_board_st.modeLocked))
        {
            switch_theme(); // Implicitely uses the headlightsOn
        }
        p_headlightsOn = headlightsOn;
        updatedElements++;
    }

    return updatedElements;
}
