#include <stdio.h>
#include "esp_display_panel.hpp"
#include <lvgl.h>
#include "lvgl_v9_port.h"
#include <ui.h>
#include "esp_timer.h"
#include <math.h>
// #include "driver/twai.h"
#include "twai_daemon.h"
#include "binocan.h"

// Refresh interval to the LVGL objects
#ifndef DISP_VALUES_REFRESH_INTERVAL
#define DISP_VALUES_REFRESH_INTERVAL 25
#endif

using namespace esp_panel::drivers;
using namespace esp_panel::board;

static const char *TAG = "GENERAL";

#pragma region Global variables

// Vehicle variables and previous values retainers
bool indicatorsOn, p_indicatorsOn = true;
bool highBeamOn, p_highBeamOn = true;
bool lowFuelOn, p_lowFuelOn = true;
bool overTemperatureOn, p_overTemperatureOn = true;
bool brakesOn, p_brakesOn = true;
bool absOn, p_absOn = true;
bool parkingBrakeOn, p_parkingBrakeOn = true;
bool lowCoolantOn, p_lowCoolantOn = true;
bool batteryOn, p_batteryOn = true;
bool lowOilOn, p_lowOilOn = true;
bool milOn, p_milOn = true;
bool airbagOn, p_airbagOn = true;
bool ignitionST, p_ignitionST = false;

// Vehicle numerical parameters
float speed_kph, p_speed_kph = 0;
float lvVoltage_v, p_lvVoltage_v = 12.0;
uint32_t rpm, p_rpm = 0;
uint8_t fuelLevel_pc, p_fuelLevel_pc = 50;
uint8_t coolant_degC, p_coolant_degC = 88;

// Odometer/trip values
float odometer_km, p_odometer_km = 0.0;
float trip_km, p_trip_km = 0.0;

// Global UI objects
lv_obj_t *needleLine = nullptr;

#pragma endregion

#pragma region Helper functions

bool generatorOn = true;

/// @brief Random generator for testing
void generateValues()
{
    if (generatorOn)
    {
        speed_kph = 120.0 + 120.0 * sin((float)(esp_timer_get_time() / 1000) / 10000.0);
        rpm = 100 * (uint8_t)((3500 + 3500 * sin((float)(esp_timer_get_time() / 1000) / 10000.0)) / 100);
        fuelLevel_pc = 50 + 50 * sin((float)(esp_timer_get_time() / 1000) / 15000.0);
        lvVoltage_v = 12 + 2 * sin((float)(esp_timer_get_time() / 1000) / 20000.0);
        coolant_degC = 88 + 12 * sin((float)(esp_timer_get_time() / 1000) / 20000.0);
        indicatorsOn = ((esp_timer_get_time() / 1000) / 500) % 2 == 0;
        highBeamOn = (esp_timer_get_time() / 1000000) % 2 == 0;
        lowFuelOn = fuelLevel_pc < 20;
        overTemperatureOn = coolant_degC > 95;
        brakesOn = (esp_timer_get_time() / 1000000) % 3 == 0;
        absOn = (esp_timer_get_time() / 1000000) % 4 == 0;
        lowCoolantOn = (esp_timer_get_time() / 1000000) % 5 == 0;
        batteryOn = (esp_timer_get_time() / 1000000) % 6 == 0;
        lowOilOn = (esp_timer_get_time() / 1000000) % 7 == 0;
        milOn = (esp_timer_get_time() / 1000000) % 8 == 0;
        airbagOn = (esp_timer_get_time() / 1000000) % 9 == 0;
    }
}

/// @brief Simple functions that compares internal metrics and updates LVGL objects if the metric has changed.
/// @return Number of objects updated
int updateLVGLObjects()
{
    int updatedElements = 0;

    if ((long)(p_speed_kph * 10) != (long)(speed_kph * 10))
    {
        // lv_arc_set_value(objects.itf_speed_kph_arc, speed_kph);
        // animateTargetArc(objects.itf_speed_kph_arc,speed_kph*10);
        // lv_arc_align_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // lv_arc_rotate_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // lv_scale_set_line_needle_value(objects.speed_scale, objects.itf_speed_kph_needle, 230, speed_kph);
        // lv_scale_set_line_needle_value(objects.speed_scale,needleLine,-8,speed_kph);
        lv_scale_set_image_needle_value(objects.speed_scale, objects.simple_needle, (long)(speed_kph * 10));
        lv_label_set_text_fmt(objects.speed, "%03ld", (long)speed_kph);
        p_speed_kph = speed_kph;
        updatedElements++;
    }
    // if (p_rpm != rpm)
    // {
    //     // lv_arc_set_value(objects.rpm_arc, rpm);
    //     lv_scale_set_line_needle_value(objects.rpm_scale,objects.rpm_needle,180,rpm/100);
    //     lv_label_set_text_fmt(objects.rpm, "%04ld", rpm);
    //     p_rpm = rpm;
    // updatedElements++;
    // }
    if (p_fuelLevel_pc != fuelLevel_pc)
    {
        lv_bar_set_value(objects.fuel_bar, fuelLevel_pc, LV_ANIM_OFF);
        lv_label_set_text_fmt(objects.fuel_level, "%03d", fuelLevel_pc);
        p_fuelLevel_pc = fuelLevel_pc;
        updatedElements++;
    }
    if (p_coolant_degC != coolant_degC)
    {
        // lv_bar_set_value(objects.coolant_bar, coolant_degC, LV_ANIM_OFF);
        lv_label_set_text_fmt(objects.coolant, "%03d", coolant_degC);
        p_coolant_degC = coolant_degC;
        updatedElements++;
    }

    if (p_lowFuelOn != lowFuelOn)
    {
        lv_obj_set_style_image_opa(objects.low_fuel_tt, lowFuelOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_lowFuelOn = lowFuelOn;
        updatedElements++;
    }
    if (p_overTemperatureOn != overTemperatureOn)
    {
        lv_obj_set_style_image_opa(objects.over_temperature_tt, overTemperatureOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_overTemperatureOn = overTemperatureOn;
        updatedElements++;
    }
#ifdef STRESS_TEST
    if (p_absOn != absOn)
    {
        lv_obj_set_style_image_opa(objects.abs_al_tt, absOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_absOn = absOn;
        updatedElements++;
    }
    if (p_brakesOn != brakesOn)
    {
        lv_obj_set_style_image_opa(objects.brakes_tt, brakesOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_brakesOn = brakesOn;
        updatedElements++;
    }
    if (p_lowCoolantOn != lowCoolantOn)
    {
        lv_obj_set_style_image_opa(objects.itf_coolant_low_al_tt, lowCoolantOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_lowCoolantOn = lowCoolantOn;
        updatedElements++;
    }
    if (p_batteryOn != batteryOn)
    {
        lv_obj_set_style_image_opa(objects.battery_tt, batteryOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_batteryOn = batteryOn;
        updatedElements++;
    }
    if (p_lowOilOn != lowOilOn)
    {
        lv_obj_set_style_image_opa(objects.low_oil_tt, lowOilOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_lowOilOn = lowOilOn;
        updatedElements++;
    }
    if (p_milOn != milOn)
    {
        lv_obj_set_style_image_opa(objects.mil_tt, milOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_milOn = milOn;
        updatedElements++;
    }
    if (p_highBeamOn != highBeamOn)
    {
        lv_obj_set_style_image_opa(objects.hi_beam_tt, highBeamOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_highBeamOn = highBeamOn;
        updatedElements++;
    }

    if (p_airbagOn != airbagOn)
    {
        lv_obj_set_style_image_opa(objects.itf_airbagal_tt, airbagOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_airbagOn = airbagOn;
        updatedElements++;
    }
#endif

    if (p_indicatorsOn != indicatorsOn)
    {
        lv_obj_set_style_image_opa(objects.indicators_tt, indicatorsOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        p_indicatorsOn = indicatorsOn;
        updatedElements++;
    }

    return updatedElements;
}

/// @brief Dispatcher linked to the TWAI daemon. Parses received CAN frames
/// @param rxMsg Received TWAI frame
/// @return Error code, if relevant
esp_err_t dispatchFrame(twai_message_t *rxMsg)
{
    static binocan_itf_active_hi_lo_t binocan_itf_active_hi_lo_msg;
    static binocan_itf_slow_metrics_t binocan_itf_slow_metrics_msg;
    static binocan_itf_fast_metrics_t binocan_itf_fast_metrics_msg;
    static binocan_itf_odometer_t binocan_itf_odometer_msg;    
    static binocan_ext_oil_metrics_t binocan_ext_oil_metrics_msg;
    static binocan_ext_chargecooling_metrics_t binocan_ext_chargecooling_metrics_msg;

    static binocan_itf_board_st_t binocan_itf_board_st_msg;
    static binocan_itf_board_version_t binocan_itf_board_version_msg;
    static binocan_ldb_st_t binocan_ldb_st_msg;
    static binocan_rdb_uds_req_t binocan_rdb_uds_req_msg;

    generatorOn = false;
    // ESP_LOGI(__func__,"ID: 0x%04LX ",rxMsg->identifier);
    // for (int i = 0; i < rxMsg->data_length_code; i++)
    // {
    //     printf("0x%02X\t",rxMsg->data[i]);
    // }
    // printf("\n");
    

    switch (rxMsg->identifier)
    {
    case BINOCAN_ITF_ACTIVE_HI_LO_FRAME_ID:
    {
        if(binocan_itf_active_hi_lo_unpack(&binocan_itf_active_hi_lo_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
        if (binocan_itf_active_hi_lo_itf_abs_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_abs_al_tt))
        {
            binocan_itf_active_hi_lo_itf_abs_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_abs_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_ABS_AL_TT_ON_CHOICE ? absOn = true : absOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "ABS telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_abs_al_tt);
            absOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_airbag_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_airbag_al_tt))
        {
            binocan_itf_active_hi_lo_itf_airbag_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_airbag_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_AIRBAG_AL_TT_ON_CHOICE ? airbagOn = true : airbagOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Airbag telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_airbag_al_tt);
            airbagOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_cel_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_cel_al_tt))
        {
            binocan_itf_active_hi_lo_itf_cel_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_cel_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_CEL_AL_TT_ON_CHOICE ? milOn = true : milOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Check Engine Light telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_cel_al_tt);
            milOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_hi_beams_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt))
        {
            binocan_itf_active_hi_lo_itf_hi_beams_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_HI_BEAMS_AH_TT_ON_CHOICE ? highBeamOn = true : highBeamOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "High beams telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt);
            highBeamOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_brake_low_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt))
        {
            binocan_itf_active_hi_lo_itf_brake_low_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_BRAKE_LOW_AL_TT_ON_CHOICE ? brakesOn = true : brakesOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Low brake fluid telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt);
            brakesOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_coolant_low_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_coolant_low_al_tt))
        {
            binocan_itf_active_hi_lo_itf_coolant_low_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_coolant_low_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_COOLANT_LOW_AL_TT_ON_CHOICE ? lowCoolantOn = true : lowCoolantOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Low coolant level telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_coolant_low_al_tt);
            lowCoolantOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_fuel_low_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_fuel_low_tt))
        {
            binocan_itf_active_hi_lo_itf_fuel_low_tt_decode(binocan_itf_active_hi_lo_msg.itf_fuel_low_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_FUEL_LOW_TT_ON_CHOICE ? lowFuelOn = true : lowFuelOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Low fuel level telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_fuel_low_tt);
            lowFuelOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_oil_pressure_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt))
        {
            binocan_itf_active_hi_lo_itf_oil_pressure_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_OIL_PRESSURE_AL_TT_ON_CHOICE ? lowOilOn = true : lowOilOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Low oil pressure telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt);
            lowOilOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_alternator_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_alternator_al_tt))
        {
            binocan_itf_active_hi_lo_itf_alternator_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_alternator_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_ALTERNATOR_AL_TT_ON_CHOICE ? batteryOn = true : batteryOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Battery or alternator telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_alternator_al_tt);
            batteryOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_over_temperature_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_over_temperature_tt))
        {
            binocan_itf_active_hi_lo_itf_over_temperature_tt_decode(binocan_itf_active_hi_lo_msg.itf_over_temperature_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_OVER_TEMPERATURE_TT_ON_CHOICE ? overTemperatureOn = true : overTemperatureOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Coolant overtemperature telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_over_temperature_tt);
            overTemperatureOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_parking_brake_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt))
        {
            binocan_itf_active_hi_lo_itf_parking_brake_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_PARKING_BRAKE_AL_TT_ON_CHOICE ? parkingBrakeOn = true : parkingBrakeOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Parking brake telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt);
            parkingBrakeOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_left_turn_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt) && binocan_itf_active_hi_lo_itf_right_turn_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt))
        {
            (binocan_itf_active_hi_lo_itf_left_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_LEFT_TURN_AH_TT_ON_CHOICE) ||
                    (binocan_itf_active_hi_lo_itf_right_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_RIGHT_TURN_AH_TT_ON_CHOICE)
                ? indicatorsOn = true
                : indicatorsOn = false;
        }
        else
        {
            ESP_LOGW(TAG, "Turn indicators telltale signal out of range: L %d - R %d", binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt, binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt);
            indicatorsOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_ignition_ah_st_is_in_range(binocan_itf_active_hi_lo_msg.itf_ignition_ah_st))
        {
            binocan_itf_active_hi_lo_itf_ignition_ah_st_decode(binocan_itf_active_hi_lo_msg.itf_ignition_ah_st) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_IGNITION_AH_ST_ON_CHOICE ? ignitionST = true : ignitionST = false;
        }
        else
        {
            ESP_LOGW(TAG, "Ignition Status signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_ignition_ah_st);
            ignitionST = false; // Default value
        }

        ESP_LOGD(TAG, "Telltales: ABS %d, Airbag %d, CEL %d, High Beams %d, Low Brake Fluid %d, Low Coolant %d, Low Fuel %d, Low Oil Pressure %d, Battery/Alternator %d, Over Temperature %d, Parking Brake %d, Indicators %d, Ignition %d",
                 absOn, airbagOn, milOn, highBeamOn, brakesOn, lowCoolantOn, lowFuelOn, lowOilOn, batteryOn,
                 overTemperatureOn, parkingBrakeOn, indicatorsOn, ignitionST);
    }
    break;

    case BINOCAN_ITF_SLOW_METRICS_FRAME_ID:
    {
        if(binocan_itf_slow_metrics_unpack(&binocan_itf_slow_metrics_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }

        if (binocan_itf_slow_metrics_itf_coolant_temp_is_in_range(binocan_itf_slow_metrics_msg.itf_coolant_temp))
        {
            coolant_degC = (uint8_t)binocan_itf_slow_metrics_itf_coolant_temp_decode(binocan_itf_slow_metrics_msg.itf_coolant_temp);
        }
        else
        {
            ESP_LOGW(TAG, "Coolant temperature signal out of range: %d", binocan_itf_slow_metrics_msg.itf_coolant_temp);
            coolant_degC = 255;       // Default value
            overTemperatureOn = true; // Set over temperature on if coolant is out of range
        }

        if (binocan_itf_slow_metrics_itf_fuel_level_pc_is_in_range(binocan_itf_slow_metrics_msg.itf_fuel_level_pc))
        {
            fuelLevel_pc = (uint8_t)binocan_itf_slow_metrics_itf_fuel_level_pc_decode(binocan_itf_slow_metrics_msg.itf_fuel_level_pc);
        }
        else
        {
            ESP_LOGW(TAG, "Fuel level signal out of range: %d", binocan_itf_slow_metrics_msg.itf_fuel_level_pc);
            fuelLevel_pc = 0; // Default value
            lowFuelOn = true; // Set low fuel on if fuel level is out of range
        }

        if (binocan_itf_slow_metrics_itf_lv_voltage_v_is_in_range(binocan_itf_slow_metrics_msg.itf_lv_voltage_v))
        {
            lvVoltage_v = (float)binocan_itf_slow_metrics_itf_lv_voltage_v_decode(binocan_itf_slow_metrics_msg.itf_lv_voltage_v);
        }
        else
        {
            ESP_LOGW(TAG, "LV Voltage signal out of range: %d", binocan_itf_slow_metrics_msg.itf_lv_voltage_v);
            batteryOn = true; // Set battery on if LV voltage is out of range
        }

        ESP_LOGD(TAG, "Slow Metrics: Coolant %d, Fuel Level %d, LV Voltage %.2f",
                 coolant_degC, fuelLevel_pc, lvVoltage_v);
    }
    break;

    case BINOCAN_ITF_FAST_METRICS_FRAME_ID:
    {
        if(binocan_itf_fast_metrics_unpack(&binocan_itf_fast_metrics_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
        if (binocan_itf_fast_metrics_itf_rpm_is_in_range(binocan_itf_fast_metrics_msg.itf_rpm))
        {
            rpm = (uint32_t)binocan_itf_fast_metrics_itf_rpm_decode(binocan_itf_fast_metrics_msg.itf_rpm);
        }
        else
        {
            ESP_LOGW(TAG, "RPM signal out of range: %d", binocan_itf_fast_metrics_msg.itf_rpm);
            rpm = 0; // Default value
        }

        if (binocan_itf_fast_metrics_itf_speed_kph_is_in_range(binocan_itf_fast_metrics_msg.itf_speed_kph))
        {
            speed_kph = (float)binocan_itf_fast_metrics_itf_speed_kph_decode(binocan_itf_fast_metrics_msg.itf_speed_kph);
        }
        else
        {
            ESP_LOGW(TAG, "Speed signal out of range: %d", binocan_itf_fast_metrics_msg.itf_speed_kph);
            speed_kph = 0; // Default value
        }

        ESP_LOGD(TAG, "Vehicle Metrics: RPM %lu, Speed %.2f", rpm, speed_kph);
    }
    break;

    case BINOCAN_ITF_ODOMETER_FRAME_ID:
    {
        if(binocan_itf_odometer_unpack(&binocan_itf_odometer_msg,rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
        if(binocan_itf_odometer_itf_odometer_km_is_in_range(binocan_itf_odometer_msg.itf_odometer_km) && binocan_itf_odometer_itf_odo_rem_m_is_in_range(binocan_itf_odometer_msg.itf_odo_rem_m))
        {
            odometer_km = binocan_itf_odometer_itf_odometer_km_decode(binocan_itf_odometer_msg.itf_odometer_km) + binocan_itf_odometer_itf_odo_rem_m_decode(binocan_itf_odometer_msg.itf_odo_rem_m)/1000.0;
        }
        else
        {
            ESP_LOGW(TAG,"Odometer could not be decoded : %d + %d /1000",binocan_itf_odometer_itf_odometer_km_decode(binocan_itf_odometer_msg.itf_odometer_km),binocan_itf_odometer_itf_odo_rem_m_decode(binocan_itf_odometer_msg.itf_odo_rem_m));
            odometer_km = 0;
        }

        if(binocan_itf_odometer_itf_trip_km_is_in_range(binocan_itf_odometer_msg.itf_trip_km) && binocan_itf_odometer_itf_trip_rem_m_is_in_range(binocan_itf_odometer_msg.itf_trip_rem_m))
        {
            trip_km = binocan_itf_odometer_itf_trip_km_decode(binocan_itf_odometer_msg.itf_trip_km) + binocan_itf_odometer_itf_trip_rem_m_decode(binocan_itf_odometer_msg.itf_trip_rem_m)/1000.0;
        }
        else
        {
            ESP_LOGW(TAG,"Trip could not be decoded : %d + %d /1000",binocan_itf_odometer_itf_trip_km_decode(binocan_itf_odometer_msg.itf_trip_km),binocan_itf_odometer_itf_trip_rem_m_decode(binocan_itf_odometer_msg.itf_trip_rem_m));
            trip_km = 0;
        }

        ESP_LOGD(TAG,"Odometer : %0.1f km Trip: %0.1f",odometer_km,trip_km);
    }
    break;

    case BINOCAN_EXT_OIL_METRICS_FRAME_ID:
    {
        if(binocan_ext_oil_metrics_unpack(&binocan_ext_oil_metrics_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
    }
    break;

    case BINOCAN_EXT_CHARGECOOLING_METRICS_FRAME_ID:
    {
        if(binocan_ext_chargecooling_metrics_unpack(&binocan_ext_chargecooling_metrics_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
    }
    break;

    case BINOCAN_ITF_BOARD_ST_FRAME_ID:
    {
        if(binocan_itf_board_st_unpack(&binocan_itf_board_st_msg, rxMsg->data, rxMsg->data_length_code)==EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
    }
    break;

    case BINOCAN_ITF_BOARD_VERSION_FRAME_ID:
    {
        if(binocan_itf_board_version_unpack(&binocan_itf_board_version_msg, rxMsg->data, rxMsg->data_length_code)==EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
    }
    break;

    case BINOCAN_LDB_ST_FRAME_ID:
    {
        if(binocan_ldb_st_unpack(&binocan_ldb_st_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
    }
    break;

    case BINOCAN_RDB_UDS_REQ_FRAME_ID:
    {
        if(binocan_rdb_uds_req_unpack(&binocan_rdb_uds_req_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(TAG,"Malformed frame 0x%03LX, invalid DLC",rxMsg->identifier);
            break;
        }
    }
    break;

    default:
    {
        ESP_LOGW(TAG, "Unknown CAN frame received: ID = 0x%03X", (uint16_t)rxMsg->identifier);
        generatorOn = true;
    }
    break;
    }

    return ESP_OK;
}

#pragma endregion

#pragma region Main app

/// @brief Main app
extern "C" void app_main()
{

#pragma region Setup
    // Board initialization
    ESP_LOGI(TAG, "Initializing board");

    Board *board = new Board();
    assert(board);
    ESP_UTILS_CHECK_FALSE_EXIT(board->init(), "Board init failed");

    auto lcd = board->getLCD();
    ESP_UTILS_CHECK_FALSE_EXIT(lcd->configFrameBufferNumber(LVGL_PORT_BUFFER_NUM), "Failed to configure frame buffer(s)");

    // Setting up the Bounce Buffer size (might not be necessary)
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB)
    {
        ESP_UTILS_CHECK_FALSE_EXIT(static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 20), "Failed to set up bounce buffer");
    }
#endif

    // Board start
    ESP_UTILS_CHECK_FALSE_EXIT(board->begin(), "Board begin failed");
    auto backLight = board->getBacklight();
    ESP_LOGD("Backlight OFF", " %d", backLight->off());


// Screen test when in debug mode
#if CONFIG_LOG_DEFAULT_LEVEL >= 4
    auto expander = board->getIO_Expander()->getBase();
    expander->printStatus();
    ESP_LOGI("Backlight", " %d", backLight->on());
    lcd->colorBarTest();
    vTaskDelay(pdMS_TO_TICKS(2000));
#endif

    // Start LVGL port
    ESP_UTILS_CHECK_FALSE_EXIT(lvgl_port_init(board->getLCD(), board->getTouch()), "Failed to start LVGL port");

    // UI loading and mofidifiers
    ESP_LOGI(TAG, "Loading UI");
    ESP_UTILS_CHECK_FALSE_EXIT(lvgl_port_lock(-1), "Failed to perform initial LVGL Mutex lock");
    ui_init();                                                                       // Load the UI library and draw it
    lv_obj_set_style_pad_radial(objects.speed_scale, 15, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    // needleLine = lv_line_create(objects.speed_scale); // Create the needle line indicator
    // lv_obj_set_style_line_color(needleLine, lv_palette_main(LV_PALETTE_RED),LV_PART_MAIN); // Set the needle to red
    // lv_obj_set_style_line_width(needleLine,8,LV_PART_MAIN);
    // lv_obj_set_style_length(needleLine, 20, LV_PART_MAIN);
    // lv_obj_set_style_line_rounded(needleLine,false,LV_PART_MAIN);
    // lv_obj_set_style_pad_right(needleLine,50,LV_PART_MAIN);
    // Following only needed when decimation is used
    static const char *scale_labels[14] = {"0", "20", "40", "60", "80", "100", "120", "140", "160", "180", "200", "220", "240", NULL};
    lv_scale_set_text_src(objects.speed_scale, scale_labels);

    // Masking circle
    //  lv_obj_t *maskCircle = lv_obj_create(objects.speed_scale);
    //  lv_obj_set_size(maskCircle, 300, 300);
    //  lv_obj_center(maskCircle);
    //  lv_obj_set_style_radius(maskCircle, LV_RADIUS_CIRCLE,0);
    //  lv_obj_set_style_bg_color(maskCircle,lv_obj_get_style_bg_color(lv_scr_act(),LV_PART_MAIN),0);
    //  lv_obj_set_style_bg_opa(maskCircle, LV_OPA_COVER,0);
    //  lv_obj_set_style_border_width(maskCircle,0,LV_PART_MAIN);

    // lv_arc_align_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
    // lv_arc_rotate_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "Backlight : %d", backLight->on());

    ESP_LOGI(TAG, "Starting TWAI port and daemon");
    ESP_UTILS_CHECK_ERROR_EXIT(initCAN(&dispatchFrame), "Failed to initialize TWAI port and daemon");
    ESP_LOGI(TAG, "Setup done");

#pragma region Main Loop
    while (true)
    {

        vTaskDelay(pdMS_TO_TICKS(DISP_VALUES_REFRESH_INTERVAL));
        generateValues();

        // Attempt locking LVGL elements prior to updating them (issue with jumping frames ?)
        if (lvgl_port_lock(-1))
        {
            updateLVGLObjects();
            lvgl_port_unlock();
        }
    }
#pragma endregion
}