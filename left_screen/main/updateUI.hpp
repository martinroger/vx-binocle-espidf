#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_lock.hpp"

#include "coefficients.h"

#include "global_vars.hpp"
#include "theme.hpp"
#include "twai_daemon.h"

#define SLOW_BLINK_PERIOD CONFIG_SLOW_BLINK_INTERVAL_MS * 1000
#define FAST_BLINK_PERIOD CONFIG_FAST_BLINK_INTERVAL_MS * 1000
bool prevBlinkOn, blinkOn = false; // Whether the blinker is sending a valid value or not

/// @brief Blink variable inverter
/// @param arg
inline void onBlinkTimer(void *arg)
{
    blinkOn = !blinkOn;
}

esp_timer_handle_t blink_timer_Hdl; // Handle for the general blink timer

/// @brief Creates and starts general blink timer
/// @return ESP_OK if started, error otherwise
inline esp_err_t initBlinkTimer()
{
    esp_err_t ret;
    const esp_timer_create_args_t blink_timer_args = {
        .callback = &onBlinkTimer,
        .name = "blink_timer"};
    ret = esp_timer_create(&blink_timer_args, &blink_timer_Hdl);
    if (ret == ESP_OK)
    {
        ret = esp_timer_start_periodic(blink_timer_Hdl, SLOW_BLINK_PERIOD);
    }
    return ret;
}

/// @brief Updates all cyclic elements
/// @param forceRefresh Force a refresh of all the conditional blocks
/// @return Number of updated elements
inline int updateLvglObjects(bool forceRefresh = false)
{
    uint64_t blinkTimerPeriod;
    int updatedElements = 0;

#ifdef CONFIG_RIGHT_SIDE_DISPLAY

    if ((long)(prevSpeedKph * 10) != (long)(speedKph * 10) || forceRefresh) // Probably should use lround
    {
        // lv_arc_set_value(objects.itf_speed_kph_arc, speedKph);
        animateTargetArc(objects.speed_arc, (speedKph / ((displayBoard.unitMph) ? COEFF_MPH_TO_KPH : 1)) * 10);
        // // lv_arc_align_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_arc_rotate_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_scale_set_line_needle_value(objects.speed_scale, objects.itf_speed_kph_needle, 230, speedKph);
        // // lv_scale_set_line_needle_value(objects.speed_scale,needleLine,-8,speedKph);
        // lv_scale_set_image_needle_value(objects.speed_scale, objects.simple_needle, (long)(speedKph * 10));
        if (lround(speedKph) != lround(prevSpeedKph))
            lv_label_set_text_fmt(objects.speed, "%ld", lround(speedKph / ((displayBoard.unitMph) ? COEFF_MPH_TO_KPH : 1)));
        prevSpeedKph = speedKph;
        updatedElements++;
    }

    if ((displayBoard.themeLocked != (lv_obj_has_state(objects.mode_lock_switch, LV_STATE_CHECKED))) || forceRefresh)
    {
        lv_obj_set_state(objects.mode_lock_switch, LV_STATE_CHECKED, displayBoard.themeLocked);
        lv_obj_set_state(objects.theme_switch, LV_STATE_DISABLED, !(displayBoard.themeLocked));
        lv_obj_set_state(objects.theme_switch, LV_STATE_CHECKED, !(displayBoard.themeLight));
        updatedElements++;
    }
    if ((displayBoard.blDarkPct) != lv_slider_get_value(objects.dark_slider) || forceRefresh)
    {
        lv_label_set_text_fmt(objects.d_bright, "%u", displayBoard.blDarkPct);
        lv_slider_set_value(objects.dark_slider, displayBoard.blDarkPct, LV_ANIM_OFF);
        updatedElements++;
    }
    if ((displayBoard.blLightPct) != lv_slider_get_value(objects.light_slider) || forceRefresh)
    {
        lv_label_set_text_fmt(objects.l_bright, "%u", displayBoard.blLightPct);
        lv_slider_set_value(objects.light_slider, displayBoard.blLightPct, LV_ANIM_OFF);
        updatedElements++;
    }

#elifdef CONFIG_LEFT_SIDE_DISPLAY
    if (((lround(prevRpm / 10.0) != lround(rpm / 10.0))) || (gearPosition != prevGearPosition) || forceRefresh)
    {
        // lv_arc_set_value(objects.itf_speed_kph_arc, speedKph);
        animateTargetArc(objects.rpm_arc, rpm);
        // // lv_arc_align_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_arc_rotate_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_scale_set_line_needle_value(objects.speed_scale, objects.itf_speed_kph_needle, 230, speedKph);
        // // lv_scale_set_line_needle_value(objects.speed_scale,needleLine,-8,speedKph);
        // lv_scale_set_image_needle_value(objects.speed_scale, objects.simple_needle, (long)(speedKph * 10));
        if (displayBoard.gearShow)
        {
            if ((gearPosition != prevGearPosition) || forceRefresh)
            {
                switch (gearPosition)
                {
                case BINOCAN_ITF_FAST_METRICS_ITF_GEAR_POSITION_ST_NEUTRAL_CHOICE:
                    lv_label_set_text(objects.rpm, "N");
                    break;

                case BINOCAN_ITF_FAST_METRICS_ITF_GEAR_POSITION_ST_UNCERTAIN_CHOICE:
                    lv_label_set_text(objects.rpm,"-");
                    break;
                
                case BINOCAN_ITF_FAST_METRICS_ITF_GEAR_POSITION_ST_REVERSE_CHOICE:
                    lv_label_set_text(objects.rpm,"R");
                    break;
                
                default:
                    lv_label_set_text_fmt(objects.rpm,"%u",gearPosition);
                    break;
                }

                prevGearPosition = gearPosition;
            }
            
        }
        else
        {
            if (((lround(prevRpm / (float)(displayBoard.rpmDecim)) != lround(rpm / (float)(displayBoard.rpmDecim)))) || forceRefresh)
                lv_label_set_text_fmt(objects.rpm, "%ld", (displayBoard.rpmDecim * lround(((float)rpm) / (displayBoard.rpmDecim))));
        }

        // lv_obj_set_state(objects.rpm, LV_STATE_FOCUSED, (displayBoard.alarmOverride) ? (rpm > displayBoard.alarmRpm) && (blinkOn || !(displayBoard.alarmBlink)) : alarmOn);
        if (rpm >= displayBoard.shiftMidRpm)
        {
            if ((rpm >= displayBoard.shiftTopRpm)) // Not focused or still has checked
            {
                lv_obj_add_state(objects.rpm_arc, LV_STATE_FOCUSED);     // Make it focused
                if (lv_obj_has_state(objects.rpm_arc, LV_STATE_CHECKED)) // Remove orange checked if present
                    lv_obj_remove_state(objects.rpm_arc, LV_STATE_CHECKED);
            }
            else if (!(lv_obj_has_state(objects.rpm_arc, LV_STATE_CHECKED))) // Not in the red zone, but is not checked or has still red zone
            {
                if (lv_obj_has_state(objects.rpm_arc, LV_STATE_FOCUSED)) // remove red zone attributes
                    lv_obj_remove_state(objects.rpm_arc, LV_STATE_FOCUSED);
                lv_obj_add_state(objects.rpm_arc, LV_STATE_CHECKED); // make it orange
            }
        }
        else if (lv_obj_has_state(objects.rpm_arc, LV_STATE_FOCUSED) || lv_obj_has_state(objects.rpm_arc, LV_STATE_CHECKED)) // not in orange or red zone, clear both values
            lv_obj_remove_state(objects.rpm_arc, (lv_state_t)(LV_STATE_FOCUSED | LV_STATE_CHECKED));
        prevRpm = rpm;
        updatedElements++;
    }
    // RPM alarm override
    if ((displayBoard.alarmOverride))
    {
        if ((rpm > displayBoard.alarmRpm))
            lv_obj_set_state(objects.rpm, LV_STATE_FOCUSED, (blinkOn || !(displayBoard.alarmBlink)));
        else if (lv_obj_has_state(objects.rpm, LV_STATE_FOCUSED))
            lv_obj_set_state(objects.rpm, LV_STATE_FOCUSED, false);
        updatedElements++;
    }
    // Non RPM alarm override for tubbies
    if (((prevAlarmOn != alarmOn) && !(displayBoard.alarmOverride)) || forceRefresh)
    {
        lv_obj_set_state(objects.rpm, LV_STATE_FOCUSED, alarmOn);
        prevAlarmOn = alarmOn;
        updatedElements++;
    }
    // If shift indicator is being used
    if (displayBoard.shiftEnabled // Shift indicator is ON
        && (prevBlinkOn != blinkOn)            // Blink inversion is needed
    )
    {
        esp_timer_get_period(blink_timer_Hdl, &blinkTimerPeriod);
        if (rpm >= displayBoard.shiftTopRpm) // Top zone
        {
            lv_obj_set_state(objects.main_tabview, LV_STATE_FOCUSED, blinkOn); // Make it red depending on blinkOn value
            if (lv_obj_has_state(objects.main_tabview, LV_STATE_CHECKED))      // In any case remove the orange
                lv_obj_remove_state(objects.main_tabview, LV_STATE_CHECKED);
            if (blinkTimerPeriod != FAST_BLINK_PERIOD) // Move to fast blinking
                esp_timer_restart(blink_timer_Hdl, FAST_BLINK_PERIOD);
        }
        else if (rpm >= displayBoard.shiftMidRpm) // Mid Zone
        {
            if (lv_obj_has_state(objects.main_tabview, LV_STATE_FOCUSED)) // Remove RED if present
                lv_obj_remove_state(objects.main_tabview, LV_STATE_FOCUSED);
            lv_obj_set_state(objects.main_tabview, LV_STATE_CHECKED, blinkOn);
            if (blinkTimerPeriod != SLOW_BLINK_PERIOD)
                esp_timer_restart(blink_timer_Hdl, SLOW_BLINK_PERIOD);
        }
        else // Low Zone
        {
            if (lv_obj_has_state(objects.main_tabview, LV_STATE_CHECKED) || lv_obj_has_state(objects.main_tabview, LV_STATE_FOCUSED)) // If red or orange
                lv_obj_remove_state(objects.main_tabview, (lv_state_t)(LV_STATE_FOCUSED | LV_STATE_CHECKED));
            if (blinkTimerPeriod != SLOW_BLINK_PERIOD)
                esp_timer_restart(blink_timer_Hdl, SLOW_BLINK_PERIOD);
        }
    }
    // If somehow the shift indicator is disactivated mid-blink
    if (!(displayBoard.shiftEnabled) && (lv_obj_has_state(objects.main_tabview, LV_STATE_FOCUSED) || lv_obj_has_state(objects.main_tabview, LV_STATE_CHECKED)))
        lv_obj_remove_state(objects.main_tabview, (lv_state_t)(LV_STATE_FOCUSED | LV_STATE_CHECKED));

    prevBlinkOn = blinkOn;

    // Buzzer catch
    if ((prevOverTemperatureOn != overTemperatureOn))
    {
        if (displayBoard.displayHwReady)
        {
            display_hw_buzzer_set(displayBoard.hotBuzzEnabled && overTemperatureOn);
        }
        // Don't update the overTemperatureOn so it can be used downstream
    }

#endif
    if (CAN_RX_TimedOut) // Turn all on if there is a CAN Timeout
    {
        interlockOk = false;
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

    if (prevFuelLevelPct != fuelLevelPct || forceRefresh) // Fuel level percentage
    {
        // lv_bar_set_value(objects.fuel_bar, fuelLevelPct, LV_ANIM_OFF);
        lv_label_set_text_fmt(objects.fuel_level, "%d", fuelLevelPct);
        prevFuelLevelPct = fuelLevelPct;
        updatedElements++;
    }
    if (prevCoolantDegC != coolantDegC || forceRefresh) // Coolant temperature
    {
        // lv_bar_set_value(objects.coolant_bar, coolantDegC, LV_ANIM_OFF);
        lv_label_set_text_fmt(objects.coolant, "%d", coolantDegC);
        prevCoolantDegC = coolantDegC;
        updatedElements++;
    }
    if (prevSupplyV != supplyV || forceRefresh) // 12V Voltage value
    {
        lv_label_set_text_fmt(objects.voltage_lvl, "%4.1fV", supplyV);
        prevSupplyV = supplyV;
        updatedElements++;
    }
    if (prevLowFuelOn != lowFuelOn || forceRefresh) // Low Fuel computed TT
    {
        lv_obj_set_style_image_opa(objects.low_fuel_tt, lowFuelOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_state(objects.fuel_level, LV_STATE_FOCUSED, lowFuelOn);
        prevLowFuelOn = lowFuelOn;
        updatedElements++;
    }
    if (prevOverTemperatureOn != overTemperatureOn || forceRefresh) // Over temperature computer TT
    {
        lv_obj_set_style_image_opa(objects.over_temperature_tt, overTemperatureOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_state(objects.coolant, LV_STATE_FOCUSED, overTemperatureOn);
        prevOverTemperatureOn = overTemperatureOn;
        updatedElements++;
    }
    lv_obj_set_style_opa(objects.interlock_state, interlockOk ? LV_OPA_TRANSP : LV_OPA_COVER, LV_STATE_DEFAULT);
    // if (prevInterlockOk != interlockOk) // Left to right screen interlock
    // {
    //     lv_obj_set_style_opa(objects.interlock_state, interlockOk ? LV_OPA_TRANSP : LV_OPA_COVER, LV_STATE_DEFAULT);
    //     prevInterlockOk = interlockOk;
    //     updatedElements++;
    // }
    if (prevSmState != displayBoard.smState || forceRefresh) // Internal state degraded or fault
    {
        switch (displayBoard.smState)
        {
        case XDB_SM_ST_DEGRADED:
            lv_obj_set_style_text_color(objects.internal_state, lv_color_hex(0xFFAB00), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_opa(objects.internal_state, LV_OPA_COVER, LV_STATE_DEFAULT);
            break;
        case XDB_SM_ST_FAULT:
            lv_obj_set_style_text_color(objects.internal_state, lv_color_hex(0xFF3A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_opa(objects.internal_state, LV_OPA_COVER, LV_STATE_DEFAULT);
            break;
        default:
            lv_obj_set_style_opa(objects.internal_state, LV_OPA_TRANSP, LV_STATE_DEFAULT);
            break;
        }
        prevSmState = displayBoard.smState;
        updatedElements++;
    }
    if (prevItfState != itfState || forceRefresh) // Same for CAN-detected ITF board state
    {
        switch (itfState)
        {
        case XDB_SM_ST_DEGRADED:
            lv_obj_set_style_text_color(objects.itf_state, lv_color_hex(0xFFAB00), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_opa(objects.itf_state, LV_OPA_COVER, LV_STATE_DEFAULT);
            break;
        case XDB_SM_ST_FAULT:
            lv_obj_set_style_text_color(objects.itf_state, lv_color_hex(0xFF3A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_opa(objects.itf_state, LV_OPA_COVER, LV_STATE_DEFAULT);
            break;
        default:
            lv_obj_set_style_opa(objects.itf_state, LV_OPA_TRANSP, LV_STATE_DEFAULT);
            break;
        }
        prevItfState = itfState;
        updatedElements++;
    }
    if (CAN_RX_TimedOut != prevCanRxTimedOut || forceRefresh) // Same for CAN Timed out indicator
    {
        lv_obj_set_style_opa(objects.can_state, CAN_RX_TimedOut ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevCanRxTimedOut = CAN_RX_TimedOut;
        updatedElements++;
    }
    if (prevOdometerKm != odometerKm || forceRefresh) // Odometer. Might need comparison at the uint level
    {
        lv_label_set_text_fmt(objects.odometer, "%.0f", odometerKm / ((displayBoard.unitMph == 1) ? COEFF_MPH_TO_KPH : 1));
        prevOdometerKm = odometerKm;
        updatedElements++;
    }
    if (prevTripKm != tripKm || forceRefresh) // Trip, might need comparison at the uint level
    {
        lv_label_set_text_fmt(objects.trip, "%.1f", tripKm / ((displayBoard.unitMph == 1) ? COEFF_MPH_TO_KPH : 1));
        prevTripKm = tripKm;
        updatedElements++;
    }
    if (prevIndicatorsOn != indicatorsOn || forceRefresh) // Double indicators arrow
    {
        lv_obj_set_style_image_opa(objects.indicators_tt, indicatorsOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevIndicatorsOn = indicatorsOn;
        updatedElements++;
    }
    if (prevRightTurnOn != rightTurnOn || forceRefresh) // Right indicator
    {
        // lv_obj_set_style_image_opa(objects.indicators_tt, indicatorsOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevRightTurnOn = rightTurnOn;
        updatedElements++;
    }
    if (prevLeftTurnOn != leftTurnOn || forceRefresh) // Right indicator
    {
        // lv_obj_set_style_image_opa(objects.indicators_tt, indicatorsOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevLeftTurnOn = leftTurnOn;
        updatedElements++;
    }
    if (prevHighBeamOn != highBeamOn || forceRefresh) // High beams
    {
        lv_obj_set_style_image_opa(objects.hi_beam_tt, highBeamOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevHighBeamOn = highBeamOn;
        updatedElements++;
    }
    if (prevBrakesOn != brakesOn || forceRefresh) // Brakes
    {
        lv_obj_set_style_image_opa(objects.brakes_tt, brakesOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevBrakesOn = brakesOn;
        updatedElements++;
    }
    if (prevAbsOn != absOn || forceRefresh) // ABS
    {
        lv_obj_set_style_image_opa(objects.abs_tt, absOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevAbsOn = absOn;
        updatedElements++;
    }
    if (prevParkingBrakeOn != parkingBrakeOn || forceRefresh) // Parking Brake
    {
        lv_obj_set_style_image_opa(objects.parkingbrake_tt, parkingBrakeOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevParkingBrakeOn = parkingBrakeOn;
        updatedElements++;
    }
    if (prevLowCoolantOn != lowCoolantOn || forceRefresh) // Low Coolant
    {
        lv_obj_set_style_image_opa(objects.low_coolant_tt, lowCoolantOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevLowCoolantOn = lowCoolantOn;
        updatedElements++;
    }
    if (prevBatteryOn != batteryOn || forceRefresh) // Battery/Alternator
    {
        lv_obj_set_style_image_opa(objects.battery_tt, batteryOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevBatteryOn = batteryOn;
        updatedElements++;
    }
    if (prevLowOilOn != lowOilOn || forceRefresh) // Low Oil Pressure
    {
        lv_obj_set_style_image_opa(objects.low_oil_tt, lowOilOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevLowOilOn = lowOilOn;
        updatedElements++;
    }
    if (prevMilOn != milOn || forceRefresh) // MIL
    {
        lv_obj_set_style_image_opa(objects.mil_tt, milOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevMilOn = milOn;
        updatedElements++;
    }
    if (prevAirbagOn != airbagOn || forceRefresh) // Airbag
    {
        lv_obj_set_style_image_opa(objects.airbag_tt, airbagOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        prevAirbagOn = airbagOn;
        updatedElements++;
    }
    static int64_t last_headlights_change_time = 0;
    static bool pending_headlights_target = false;
    static bool headlights_debounce_active = false;

    if (forceRefresh)
    {
        prevHeadlightsOn = headlightsOn;
        headlights_debounce_active = false;
        if (!(displayBoard.themeLocked))
        {
            switchTheme(headlightsOn, false, 250);
        }
        updatedElements++;
    }
    else if (prevHeadlightsOn != headlightsOn)
    {
        int64_t now = esp_timer_get_time() / 1000; // ms
        if (!headlights_debounce_active || pending_headlights_target != headlightsOn)
        {
            // First detection of change or target switched again: start debounce window
            headlights_debounce_active = true;
            pending_headlights_target = headlightsOn;
            last_headlights_change_time = now;
        }
        else if (now - last_headlights_change_time >= 150)
        {
            // Stable for at least 150 ms: apply change
            prevHeadlightsOn = headlightsOn;
            headlights_debounce_active = false;
            if (!(displayBoard.themeLocked))
            {
                switchTheme(headlightsOn, false, 250);
            }
            updatedElements++;
        }
    }
    else
    {
        headlights_debounce_active = false;
    }

    return updatedElements;
}

/// @brief FreeRTOS task responsible for periodic and notified updates of LVGL objects
/// @param pvParameters Task parameters (unused)
inline void updateUiTask(void *pvParameters)
{
    bool forceRefresh = true; // First iteration forces a full initial refresh
    while (true)
    {
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_DATA_REFRESH_INTERVAL));
        if (notified > 0)
        {
            forceRefresh = true;
        }

        if (lvgl_port_lock(-1))
        {
            updateLvglObjects(forceRefresh);
            lvgl_port_unlock();
        }
        forceRefresh = false;
    }
}

/// @brief Notifies the updateUiTask to execute an immediate full refresh of all UI elements
inline void forceUiRefresh()
{
    if (updateUiTaskHandle != NULL)
    {
        xTaskNotifyGive(updateUiTaskHandle);
    }
}
