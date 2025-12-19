#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_display_panel.hpp"
#include <lvgl.h>
#include "lvgl_v9_port.h"
#include <ui.h>

#include <math.h>
#include "twai_daemon.h"
#include "binocan.h"
#include "coefficients.h"
#include "version_parser.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_err.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

// Refresh interval to the LVGL objects
#ifndef DISP_VALUES_REFRESH_INTERVAL
#define DISP_VALUES_REFRESH_INTERVAL 25
#endif

using namespace esp_panel::drivers;
using namespace esp_panel::board;

#ifdef TAG
#undef TAG
#endif
#define TAG "Main"

// ESP32 is Little Endian, UDS is BE
template <typename T>
T swap_endian(T u)
{
    static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");
    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;
    source.u = u;
    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];
    return dest.u;
}

#pragma region Global variables

#ifdef CONFIG_RIGHT_SIDE_DISPLAY
#define XDB_SM_ST_OFF BINOCAN_RDB_ST_RDB_SM_ST_OFF_CHOICE
#define XDB_SM_ST_INIT BINOCAN_RDB_ST_RDB_SM_ST_INIT_CHOICE
#define XDB_SM_ST_OK BINOCAN_RDB_ST_RDB_SM_ST_OK_CHOICE
#define XDB_SM_ST_DEGRADED BINOCAN_RDB_ST_RDB_SM_ST_DEGRADED_CHOICE
#define XDB_SM_ST_FAULT BINOCAN_RDB_ST_RDB_SM_ST_FAULT_CHOICE
#define XDB_SM_ST_OTA BINOCAN_RDB_ST_RDB_SM_ST_OTA_CHOICE
#elifdef CONFIG_LEFT_SIDE_DISPLAY
#define XDB_SM_ST_OFF BINOCAN_LDB_ST_LDB_SM_ST_OFF_CHOICE
#define XDB_SM_ST_INIT BINOCAN_LDB_ST_LDB_SM_ST_INIT_CHOICE
#define XDB_SM_ST_OK BINOCAN_LDB_ST_LDB_SM_ST_OK_CHOICE
#define XDB_SM_ST_DEGRADED BINOCAN_LDB_ST_LDB_SM_ST_DEGRADED_CHOICE
#define XDB_SM_ST_FAULT BINOCAN_LDB_ST_LDB_SM_ST_FAULT_CHOICE
#define XDB_SM_ST_OTA BINOCAN_LDB_ST_LDB_SM_ST_OTA_CHOICE
#endif

struct board_ST
{
    bool screen_interlock_OK = false;
    uint8_t internal_ST = XDB_SM_ST_OFF;
    uint8_t mph_selected = false;
    uint8_t rpm_alarm_override = false;
    uint32_t rpm_alarm_threshold = 6000;
    parsed_app_meta_t *app_metadata;
} display_board_st;

// Used only to selectively update in LVGL
bool screen_interlock_OK, p_screen_interlock_OK = false; // Checks opposite display status
int64_t last_interlock_ts;
uint8_t p_internal_ST = XDB_SM_ST_DEGRADED; // Checks internal state in the LVGL elements update routine
uint8_t itf_board_st, p_itf_board_st = XDB_SM_ST_DEGRADED;
bool p_CAN_RX_TimedOut = true;

// Vehicle variables and previous values retainers
bool indicatorsOn, p_indicatorsOn = true;
bool rightTurnOn, p_rightTurnOn = true;
bool leftTurnOn, p_leftTurnOn = true;
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
bool alarmOn, p_alarmOn = true;

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
static const char *speed_kph_scale_labels[14] = {"0", "20", "40", "60", "80", "100", "120", "140", "160", "180", "200", "220", "240", NULL};
static const char *speed_mph_scale_labels[10] = {"0", "20", "40", "60", "80", "100", "120", "140", "160", NULL};
static const char *rpm_scale_labels[10] = {"0", "1000", "2000", "3000", "4000", "5000", "6000", "7000", "8000", NULL};

#pragma endregion

#pragma region Helper functions

extern "C" void action_reboot_factory(lv_event_t *e)
{
    const esp_partition_t *factoryPart = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
    if (factoryPart != NULL)
    {
        esp_err_t bootsel_err = esp_ota_set_boot_partition(factoryPart);
        if (bootsel_err == ESP_OK)
            esp_restart();
        else
            ESP_LOGE(__func__, "Could not restart ESP");
    }
}

#ifdef CONFIG_RIGHT_SIDE_DISPLAY
extern "C" void action_mph_switch_toggled(lv_event_t *e)
{

    display_board_st.mph_selected = lv_obj_has_state(objects.mph_on, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "mph_on", display_board_st.mph_selected) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save mph selector status");
        nvs_commit(h);
        nvs_close(h);
    }
    lvgl_port_lock(-1);
    if (display_board_st.mph_selected == 0)
    {
        lv_scale_set_range(objects.speed_scale, 0, 2400);
        lv_scale_set_total_tick_count(objects.speed_scale, 49);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_kph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 2400);
        lv_label_set_text(objects.speed_unit, "KPH");
    }
    else if (display_board_st.mph_selected == 1)
    {
        lv_scale_set_range(objects.speed_scale, 0, 1600);
        lv_scale_set_total_tick_count(objects.speed_scale, 33);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_mph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 1600);
        lv_label_set_text(objects.speed_unit, "MPH");
    }
    lvgl_port_unlock();
    // Force refresh
    p_odometer_km = 0;
    p_trip_km = 0;
    p_speed_kph = 0;
}
#elifdef CONFIG_LEFT_SIDE_DISPLAY

extern "C" void action_set_rpm_alarm_override(lv_event_t *e)
{
    display_board_st.rpm_alarm_override = lv_obj_has_state(objects.override_alarm_sw, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "rpm_al_overr", display_board_st.rpm_alarm_override) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save rpm alarm override switch status");
    }
    lvgl_port_lock(-1);
    switch (display_board_st.rpm_alarm_override)
    {
    case 1:
        lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, false);
        if (nvs_get_u32(h, "rpm_al_thr", &(display_board_st.rpm_alarm_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve RPM alarm threshold from NVS");
        lv_spinbox_set_value(objects.rpm_alarm_spinbox, (int32_t)display_board_st.rpm_alarm_threshold);
        break;
    case 0:
        lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_spinbox_set_value(objects.rpm_alarm_spinbox, 0);
        break;
    default:
        ESP_LOGE(__func__, "Unauthorized RPM alarm override state");
        break;
    }
    nvs_commit(h);
    nvs_close(h);
    lvgl_port_unlock();
}

extern "C" void action_inc_rpm_spinbox(lv_event_t *e)
{
    lvgl_port_lock(-1);
    lv_spinbox_increment(objects.rpm_alarm_spinbox);
    lvgl_port_unlock();
}

extern "C" void action_dec_rpm_spinbox(lv_event_t *e)
{
    lvgl_port_lock(-1);
    lv_spinbox_decrement(objects.rpm_alarm_spinbox);
    lvgl_port_unlock();
}

extern "C" void action_save_rpm_spinbox(lv_event_t *e)
{
    display_board_st.rpm_alarm_threshold = lv_spinbox_get_value(objects.rpm_alarm_spinbox);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u32(h, "rpm_al_thr", display_board_st.rpm_alarm_threshold) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save rpm alarm override threshold");
        nvs_commit(h);
        nvs_close(h);
    }
}
#endif

/// @brief Updates all cyclic elements
/// @param forceRefresh Force a refresh of all the conditional blocks
/// @return Number of updated elements
int updateLVGLObjects(bool forceRefresh = false)
{
    int updatedElements = 0;

#ifdef CONFIG_RIGHT_SIDE_DISPLAY

    if ((long)(p_speed_kph * 10) != (long)(speed_kph * 10) || forceRefresh)
    {
        // lv_arc_set_value(objects.itf_speed_kph_arc, speed_kph);
        animateTargetArc(objects.speed_arc, (speed_kph / ((display_board_st.mph_selected == 1) ? COEFF_MPH_TO_KPH : 1)) * 10);
        // // lv_arc_align_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_arc_rotate_obj_to_angle(objects.itf_speed_kph_arc, objects.itf_speed_kph_needle, 0);
        // // lv_scale_set_line_needle_value(objects.speed_scale, objects.itf_speed_kph_needle, 230, speed_kph);
        // // lv_scale_set_line_needle_value(objects.speed_scale,needleLine,-8,speed_kph);
        // lv_scale_set_image_needle_value(objects.speed_scale, objects.simple_needle, (long)(speed_kph * 10));
        if ((long)round(speed_kph) != (long)round(p_speed_kph))
            lv_label_set_text_fmt(objects.speed, "%03ld", (long)round(speed_kph / ((display_board_st.mph_selected == 1) ? COEFF_MPH_TO_KPH : 1)));
        p_speed_kph = speed_kph;
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
        lv_obj_set_state(objects.rpm, LV_STATE_FOCUSED, (display_board_st.rpm_alarm_override == 1) ? (rpm > display_board_st.rpm_alarm_threshold) : alarmOn);

        p_rpm = rpm;
        updatedElements++;
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
        lv_obj_set_state(objects.fuel_level,LV_STATE_FOCUSED,lowFuelOn);
        p_lowFuelOn = lowFuelOn;
        updatedElements++;
    }
    if (p_overTemperatureOn != overTemperatureOn || forceRefresh) // Over temperature computer TT
    {
        lv_obj_set_style_image_opa(objects.over_temperature_tt, overTemperatureOn ? LV_OPA_COVER : LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_state(objects.coolant,LV_STATE_FOCUSED,overTemperatureOn);
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
    return updatedElements;
}

#pragma region Frame Dispatcher

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

    // Variables reserved for the UDS side of business
    static uint8_t ST_min = CONFIG_OTA_SEPARATION_TIME_MS;
    static uint8_t BSize = CONFIG_OTA_BLOCK_SIZE;
    static uint8_t blockCounter = 0x00;
    static uint8_t sequenceNumber = 0x01;
    static const esp_partition_t *update_partition = NULL;
    static bool FC_sent = false;
    static bool FF_received = false;
    static uint32_t receivedBytes;
    static bool transferComplete = false;
    static bool OTA_started = false;
    static uint32_t image_size;
    static esp_ota_handle_t ota_handle;
    twai_message_t UDS_RESP_MSG = {
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
        .identifier = BINOCAN_RDB_UDS_RESP_FRAME_ID,
#elifdef CONFIG_LEFT_SIDE_DISPLAY
        .identifier = BINOCAN_LDB_UDS_RESP_FRAME_ID,
#endif
        .data_length_code = 8};

    esp_err_t ota_err;
    if (update_partition == NULL) // Only first time
        update_partition = esp_ota_get_next_update_partition(NULL);

#ifdef CONFIG_RIGHT_SIDE_DISPLAY
    static binocan_ldb_st_t binocan_ldb_st_msg;
    static binocan_rdb_uds_req_t binocan_rdb_uds_req_msg;
#elifdef CONFIG_LEFT_SIDE_DISPLAY
    static binocan_rdb_st_t binocan_rdb_st_msg;
    static binocan_ldb_uds_req_t binocan_ldb_uds_req_msg;
#endif
    UDS_RESP_MSG.ss = 0;

    switch (rxMsg->identifier)
    {
    case BINOCAN_ITF_ACTIVE_HI_LO_FRAME_ID:
    {
        if (binocan_itf_active_hi_lo_unpack(&binocan_itf_active_hi_lo_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
        if (binocan_itf_active_hi_lo_itf_abs_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_abs_al_tt))
        {
            absOn = !(binocan_itf_active_hi_lo_itf_abs_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_abs_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_ABS_AL_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "ABS telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_abs_al_tt);
            absOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_airbag_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_airbag_al_tt))
        {
            airbagOn = !(binocan_itf_active_hi_lo_itf_airbag_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_airbag_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_AIRBAG_AL_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Airbag telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_airbag_al_tt);
            airbagOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_cel_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_cel_al_tt))
        {
            milOn = !(binocan_itf_active_hi_lo_itf_cel_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_cel_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_CEL_AL_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Check Engine Light telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_cel_al_tt);
            milOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_hi_beams_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt))
        {
            highBeamOn = (binocan_itf_active_hi_lo_itf_hi_beams_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_HI_BEAMS_AH_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "High beams telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt);
            highBeamOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_brake_low_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt))
        {
            brakesOn = !(binocan_itf_active_hi_lo_itf_brake_low_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_BRAKE_LOW_AL_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Low brake fluid telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt);
            brakesOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_coolant_low_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_coolant_low_ah_tt))
        {
            lowCoolantOn = (binocan_itf_active_hi_lo_itf_coolant_low_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_coolant_low_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_COOLANT_LOW_AH_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Low coolant level telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_coolant_low_ah_tt);
            lowCoolantOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_fuel_low_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_fuel_low_tt))
        {
            lowFuelOn = (binocan_itf_active_hi_lo_itf_fuel_low_tt_decode(binocan_itf_active_hi_lo_msg.itf_fuel_low_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_FUEL_LOW_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Low fuel level telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_fuel_low_tt);
            lowFuelOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_oil_pressure_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt))
        {
            lowOilOn = !(binocan_itf_active_hi_lo_itf_oil_pressure_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_OIL_PRESSURE_AL_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Low oil pressure telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt);
            lowOilOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_alternator_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_alternator_al_tt))
        {
            batteryOn = !(binocan_itf_active_hi_lo_itf_alternator_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_alternator_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_ALTERNATOR_AL_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Battery or alternator telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_alternator_al_tt);
            batteryOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_over_temperature_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_over_temperature_tt))
        {
            overTemperatureOn = (binocan_itf_active_hi_lo_itf_over_temperature_tt_decode(binocan_itf_active_hi_lo_msg.itf_over_temperature_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_OVER_TEMPERATURE_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Coolant overtemperature telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_over_temperature_tt);
            overTemperatureOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_parking_brake_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt))
        {
            parkingBrakeOn = !(binocan_itf_active_hi_lo_itf_parking_brake_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_PARKING_BRAKE_AL_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Parking brake telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt);
            parkingBrakeOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_left_turn_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt) && binocan_itf_active_hi_lo_itf_right_turn_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt))
        {
            indicatorsOn = (binocan_itf_active_hi_lo_itf_left_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_LEFT_TURN_AH_TT_ON_CHOICE) ||
                           (binocan_itf_active_hi_lo_itf_right_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_RIGHT_TURN_AH_TT_ON_CHOICE);
            leftTurnOn = (binocan_itf_active_hi_lo_itf_left_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_LEFT_TURN_AH_TT_ON_CHOICE);
            rightTurnOn = (binocan_itf_active_hi_lo_itf_right_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_RIGHT_TURN_AH_TT_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Turn indicators telltale signal out of range: L %d - R %d", binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt, binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt);
            indicatorsOn = true;
            rightTurnOn = true;
            leftTurnOn = true;
        }

        if (binocan_itf_active_hi_lo_itf_ignition_ah_st_is_in_range(binocan_itf_active_hi_lo_msg.itf_ignition_ah_st))
        {
            ignitionST = (binocan_itf_active_hi_lo_itf_ignition_ah_st_decode(binocan_itf_active_hi_lo_msg.itf_ignition_ah_st) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_IGNITION_AH_ST_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Ignition Status signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_ignition_ah_st);
            ignitionST = false; // Default value
        }
        if (binocan_itf_active_hi_lo_itf_alarm_ah_is_in_range(binocan_itf_active_hi_lo_msg.itf_alarm_ah))
        {
            alarmOn = (binocan_itf_active_hi_lo_itf_alarm_ah_decode(binocan_itf_active_hi_lo_msg.itf_alarm_ah) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_ALARM_AH_ON_CHOICE);
        }
        else
        {
            ESP_LOGW(__func__, "Alarm signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_alarm_ah);
            alarmOn = true; // Default value
        }

        ESP_LOGD(__func__, "Telltales: ABS %d, Airbag %d, CEL %d, High Beams %d, Low Brake Fluid %d, Low Coolant %d, Low Fuel %d, Low Oil Pressure %d, Battery/Alternator %d, Over Temperature %d, Parking Brake %d, Indicators %d, Ignition %d, Alarm %d",
                 absOn, airbagOn, milOn, highBeamOn, brakesOn, lowCoolantOn, lowFuelOn, lowOilOn, batteryOn,
                 overTemperatureOn, parkingBrakeOn, indicatorsOn, ignitionST, alarmOn);
    }
    break;

    case BINOCAN_ITF_SLOW_METRICS_FRAME_ID:
    {
        if (binocan_itf_slow_metrics_unpack(&binocan_itf_slow_metrics_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }

        if (binocan_itf_slow_metrics_itf_coolant_temp_is_in_range(binocan_itf_slow_metrics_msg.itf_coolant_temp))
        {
            coolant_degC = (uint8_t)round(binocan_itf_slow_metrics_itf_coolant_temp_decode(binocan_itf_slow_metrics_msg.itf_coolant_temp));
        }
        else
        {
            ESP_LOGW(__func__, "Coolant temperature signal out of range: %d", binocan_itf_slow_metrics_msg.itf_coolant_temp);
            coolant_degC = 255;       // Default value
            overTemperatureOn = true; // Set over temperature on if coolant is out of range
        }

        if (binocan_itf_slow_metrics_itf_fuel_level_pc_is_in_range(binocan_itf_slow_metrics_msg.itf_fuel_level_pc))
        {
            fuelLevel_pc = (uint8_t)round(binocan_itf_slow_metrics_itf_fuel_level_pc_decode(binocan_itf_slow_metrics_msg.itf_fuel_level_pc));
        }
        else
        {
            ESP_LOGW(__func__, "Fuel level signal out of range: %d", binocan_itf_slow_metrics_msg.itf_fuel_level_pc);
            fuelLevel_pc = 0; // Default value
            lowFuelOn = true; // Set low fuel on if fuel level is out of range
        }

        if (binocan_itf_slow_metrics_itf_lv_voltage_v_is_in_range(binocan_itf_slow_metrics_msg.itf_lv_voltage_v))
        {
            lvVoltage_v = (float)binocan_itf_slow_metrics_itf_lv_voltage_v_decode(binocan_itf_slow_metrics_msg.itf_lv_voltage_v);
        }
        else
        {
            ESP_LOGW(__func__, "LV Voltage signal out of range: %d", binocan_itf_slow_metrics_msg.itf_lv_voltage_v);
            batteryOn = true; // Set battery on if LV voltage is out of range
        }

        ESP_LOGD(__func__, "Slow Metrics: Coolant %d, Fuel Level %d, LV Voltage %.2f",
                 coolant_degC, fuelLevel_pc, lvVoltage_v);
    }
    break;

    case BINOCAN_ITF_FAST_METRICS_FRAME_ID:
    {
        if (binocan_itf_fast_metrics_unpack(&binocan_itf_fast_metrics_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
        if (binocan_itf_fast_metrics_itf_rpm_is_in_range(binocan_itf_fast_metrics_msg.itf_rpm))
        {
            rpm = (uint32_t)round(binocan_itf_fast_metrics_itf_rpm_decode(binocan_itf_fast_metrics_msg.itf_rpm));
        }
        else
        {
            ESP_LOGW(__func__, "RPM signal out of range: %d", binocan_itf_fast_metrics_msg.itf_rpm);
            rpm = 0; // Default value
        }

        if (binocan_itf_fast_metrics_itf_speed_kph_is_in_range(binocan_itf_fast_metrics_msg.itf_speed_kph))
        {
            speed_kph = (float)binocan_itf_fast_metrics_itf_speed_kph_decode(binocan_itf_fast_metrics_msg.itf_speed_kph);
        }
        else
        {
            ESP_LOGW(__func__, "Speed signal out of range: %d", binocan_itf_fast_metrics_msg.itf_speed_kph);
            speed_kph = 0; // Default value
        }

        ESP_LOGD(__func__, "Vehicle Metrics: RPM %lu, Speed %.2f", rpm, speed_kph);
    }
    break;

    case BINOCAN_ITF_ODOMETER_FRAME_ID:
    {
        if (binocan_itf_odometer_unpack(&binocan_itf_odometer_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
        if (binocan_itf_odometer_itf_odometer_km_is_in_range(binocan_itf_odometer_msg.itf_odometer_km) && binocan_itf_odometer_itf_odo_rem_m_is_in_range(binocan_itf_odometer_msg.itf_odo_rem_m))
        {
            odometer_km = binocan_itf_odometer_itf_odometer_km_decode(binocan_itf_odometer_msg.itf_odometer_km) + binocan_itf_odometer_itf_odo_rem_m_decode(binocan_itf_odometer_msg.itf_odo_rem_m) / 1000.0;
        }
        else
        {
            ESP_LOGW(__func__, "Odometer could not be decoded : %d + %d /1000", binocan_itf_odometer_itf_odometer_km_decode(binocan_itf_odometer_msg.itf_odometer_km), binocan_itf_odometer_itf_odo_rem_m_decode(binocan_itf_odometer_msg.itf_odo_rem_m));
            odometer_km = 0;
        }

        if (binocan_itf_odometer_itf_trip_km_is_in_range(binocan_itf_odometer_msg.itf_trip_km) && binocan_itf_odometer_itf_trip_rem_m_is_in_range(binocan_itf_odometer_msg.itf_trip_rem_m))
        {
            trip_km = binocan_itf_odometer_itf_trip_km_decode(binocan_itf_odometer_msg.itf_trip_km) + binocan_itf_odometer_itf_trip_rem_m_decode(binocan_itf_odometer_msg.itf_trip_rem_m) / 1000.0;
        }
        else
        {
            ESP_LOGW(__func__, "Trip could not be decoded : %d + %d /1000", binocan_itf_odometer_itf_trip_km_decode(binocan_itf_odometer_msg.itf_trip_km), binocan_itf_odometer_itf_trip_rem_m_decode(binocan_itf_odometer_msg.itf_trip_rem_m));
            trip_km = 0;
        }

        ESP_LOGD(__func__, "Odometer : %0.1f km Trip: %0.1f", odometer_km, trip_km);
    }
    break;

    case BINOCAN_EXT_OIL_METRICS_FRAME_ID:
    {
        if (binocan_ext_oil_metrics_unpack(&binocan_ext_oil_metrics_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
    }
    break;

    case BINOCAN_EXT_CHARGECOOLING_METRICS_FRAME_ID:
    {
        if (binocan_ext_chargecooling_metrics_unpack(&binocan_ext_chargecooling_metrics_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
    }
    break;

    case BINOCAN_ITF_BOARD_ST_FRAME_ID:
    {
        if (binocan_itf_board_st_unpack(&binocan_itf_board_st_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
        if (binocan_itf_board_st_itf_sm_st_is_in_range(binocan_itf_board_st_msg.itf_sm_st))
        {
            itf_board_st = (uint8_t)binocan_itf_board_st_itf_sm_st_decode(binocan_itf_board_st_msg.itf_sm_st);
        }
        else
        {
            ESP_LOGW(__func__, "ITF SM Status signal out of range: %d", binocan_itf_board_st_msg.itf_sm_st);
            itf_board_st = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_FAULT_CHOICE; // Default value
        }
    }
    break;

    case BINOCAN_ITF_BOARD_VERSION_FRAME_ID:
    {
        if (binocan_itf_board_version_unpack(&binocan_itf_board_version_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
    }
    break;
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
    case BINOCAN_LDB_ST_FRAME_ID:
    {
        if (binocan_ldb_st_unpack(&binocan_ldb_st_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
        if (binocan_ldb_st_ldb_sm_st_is_in_range(binocan_ldb_st_msg.ldb_sm_st))
        {
            screen_interlock_OK = ((uint8_t)binocan_ldb_st_ldb_sm_st_decode(binocan_ldb_st_msg.ldb_sm_st) == XDB_SM_ST_OK);
        }
        else
        {
            ESP_LOGW(__func__, "LDB SM Status signal out of range: %d", binocan_ldb_st_msg.ldb_sm_st);
            screen_interlock_OK = false; // Default value
        }
    }
    break;

    case BINOCAN_RDB_UDS_REQ_FRAME_ID:
    {
        if (binocan_rdb_uds_req_unpack(&binocan_rdb_uds_req_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
#elifdef CONFIG_LEFT_SIDE_DISPLAY
    case BINOCAN_RDB_ST_FRAME_ID:
    {
        if (binocan_rdb_st_unpack(&binocan_rdb_st_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            break;
        }
        if (binocan_rdb_st_rdb_sm_st_is_in_range(binocan_rdb_st_msg.rdb_sm_st))
        {
            screen_interlock_OK = ((uint8_t)binocan_rdb_st_rdb_sm_st_decode(binocan_rdb_st_msg.rdb_sm_st) == XDB_SM_ST_OK);
        }
        else
        {
            ESP_LOGW(__func__, "RDB SM Status signal out of range: %d", binocan_rdb_st_msg.rdb_sm_st);
            screen_interlock_OK = false; // Default value
        }
    }
    break;

    case BINOCAN_LDB_UDS_REQ_FRAME_ID:
    {
        if (binocan_ldb_uds_req_unpack(&binocan_ldb_uds_req_msg, rxMsg->data, rxMsg->data_length_code) == EINVAL)
#endif // Could be pulled up to include UDS logic
        {
            ESP_LOGE(__func__, "Malformed frame 0x%03LX, invalid DLC", rxMsg->identifier);
            if (OTA_started)
            {
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                ESP_LOGW(__func__, "OTA was in progress, aborted.");
            }
            // Send the Error Frame
            UDS_RESP_MSG.extd = false;
            UDS_RESP_MSG.data[0] = 0x40;
            UDS_RESP_MSG.data[1] = 0xFF; // General error
            for (size_t i = 2; i < 8; i++)
            {
                UDS_RESP_MSG.data[i] = 0xAA;
            }
            if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                ESP_LOGE(__func__, "Could not queue internal state message in queue");
            else
                ESP_LOGI(__func__, "Error frame sent");
            break; // Error break
        }
        // Check if this is a correct first frame
        if ((rxMsg->data[0] & 0xF0) == 0x10)
        {
            if (OTA_started || FF_received) // Break case
            {
                ESP_LOGE(__func__, "New FF received while OTA in progress, aborting.");
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                // Send the Error Frame
                UDS_RESP_MSG.extd = false;
                UDS_RESP_MSG.data[0] = 0x40;
                UDS_RESP_MSG.data[1] = 0xFF; // General error
                for (size_t i = 2; i < 8; i++)
                {
                    UDS_RESP_MSG.data[i] = 0xAA;
                }
                if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                    ESP_LOGE(__func__, "Could not queue internal state message in queue");
                else
                    ESP_LOGI(__func__, "Error frame sent");
                break;
            }

            // TODO : check for escape sequence, shorter size type
            image_size = swap_endian<uint32_t>(*(uint32_t *)(rxMsg->data + 2));
            receivedBytes = 0;
            sequenceNumber = 0x01;
            blockCounter = 0x00;
            transferComplete = false;
            ESP_LOGI(__func__, "Received FF, starting OTA for size : %lu bytes on partition %s", image_size, update_partition->label);
            ota_err = esp_ota_begin(update_partition, (size_t)image_size, &ota_handle);
            if (ota_err != ESP_OK) // Break if somehow the OTA doesn't start, print error code
            {
                ESP_LOGE(__func__, "Could not start OTA : %s", esp_err_to_name(ota_err));
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                // Send the Error Frame
                UDS_RESP_MSG.extd = false;
                UDS_RESP_MSG.data[0] = 0x40;
                UDS_RESP_MSG.data[1] = 0x04; // No start
                for (size_t i = 2; i < 8; i++)
                {
                    UDS_RESP_MSG.data[i] = 0xAA;
                }
                if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                    ESP_LOGE(__func__, "Could not queue internal state message in queue");
                else
                    ESP_LOGI(__func__, "Error frame sent");
                break;
            }
            ota_err = esp_ota_write(ota_handle, (rxMsg->data + 6), 2);
            if (ota_err != ESP_OK) // Break if somehow the OTA doesn't write
            {
                ESP_LOGE(__func__, "Could not write OTA segment : %s", esp_err_to_name(ota_err));
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                // Send the Error Frame
                UDS_RESP_MSG.extd = false;
                UDS_RESP_MSG.data[0] = 0x40;
                UDS_RESP_MSG.data[1] = 0x05; // No write
                for (size_t i = 2; i < 8; i++)
                {
                    UDS_RESP_MSG.data[i] = 0xAA;
                }
                if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                    ESP_LOGE(__func__, "Could not queue internal state message in queue");
                else
                    ESP_LOGI(__func__, "Error frame sent");
                break;
            }
            receivedBytes = 2;
            FF_received = true;
            OTA_started = true;
            // Send the FC frame for continuation
            UDS_RESP_MSG.extd = false;
            UDS_RESP_MSG.data[0] = 0x30;
            UDS_RESP_MSG.data[1] = BSize;
            UDS_RESP_MSG.data[2] = ST_min;
            for (size_t i = 3; i < 8; i++)
            {
                UDS_RESP_MSG.data[i] = 0xAA;
            }
            if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
            {
                ESP_LOGE(__func__, "Could not queue internal state message in queue");
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                break;
            }
            else
            {
                ESP_LOGI(__func__, "Flow Control Frame sent.");
            }
            FC_sent = true;
            break; // Successful break
        }
        else if ((rxMsg->data[0] & 0xF0) == 0x20) // CF is received
        {
            if (!OTA_started || !FF_received || !FC_sent) // Weird break case
            {
                ESP_LOGE(__func__, "Unexpected CF received");
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                // Send the Error Frame
                UDS_RESP_MSG.extd = false;
                UDS_RESP_MSG.data[0] = 0x40;
                UDS_RESP_MSG.data[1] = 0xFF; // General error
                for (size_t i = 2; i < 8; i++)
                {
                    UDS_RESP_MSG.data[i] = 0xAA;
                }
                if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                    ESP_LOGE(__func__, "Could not queue internal state message in queue");
                else
                    ESP_LOGI(__func__, "Error frame sent");
                break;
            }

            // Check sequence number
            if ((rxMsg->data[0] & 0x0F) != sequenceNumber)
            {
                ESP_LOGE(__func__, "Bad sequence number : %u instead of %u", rxMsg->data[0] & 0x0F, sequenceNumber);
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                // Send the Error Frame
                UDS_RESP_MSG.extd = false;
                UDS_RESP_MSG.data[0] = 0x40;
                UDS_RESP_MSG.data[1] = 0x03; // Seq Number error
                UDS_RESP_MSG.data[2] = sequenceNumber; // Expected sequence number
                for (size_t i = 3; i < 8; i++)
                {
                    UDS_RESP_MSG.data[i] = 0xAA;
                }
                if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                    ESP_LOGE(__func__, "Could not queue internal state message in queue");
                else
                    ESP_LOGI(__func__, "Error frame sent");
                break;
            }
            sequenceNumber = ((sequenceNumber + 1) & 0x0F) == 0x00 ? 0x01 : (sequenceNumber + 1)&0x0F;
            blockCounter++;

            // Check if the whole message can be written to OTA or if something needs to be ignored
            if (image_size - receivedBytes >= 7)
            {
                ota_err = esp_ota_write(ota_handle, (rxMsg->data + 1), 7);
                if (ota_err != ESP_OK) // Break if somehow the OTA doesn't write
                {
                    ESP_LOGE(__func__, "Could not write OTA segment : %s", esp_err_to_name(ota_err));
                    esp_ota_abort(ota_handle);
                    FC_sent = false;
                    FF_received = false;
                    OTA_started = false;
                    receivedBytes = 0;
                    transferComplete = false;
                    image_size = 0;
                    blockCounter = 0x00;
                    sequenceNumber = 0x01;
                    // Send the Error Frame
                    UDS_RESP_MSG.extd = false;
                    UDS_RESP_MSG.data[0] = 0x40;
                    UDS_RESP_MSG.data[1] = 0x05; // No write
                    for (size_t i = 2; i < 8; i++)
                    {
                        UDS_RESP_MSG.data[i] = 0xAA;
                    }
                    if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                        ESP_LOGE(__func__, "Could not queue internal state message in queue");
                    else
                        ESP_LOGI(__func__, "Error frame sent");
                    break;
                }
                receivedBytes += 7;

                odometer_km = image_size - receivedBytes;
                trip_km = 100.0* (float)(receivedBytes) / (float)(image_size);
                fuelLevel_pc = trip_km;
                if (image_size == receivedBytes)
                    transferComplete = true;
            }
            else // Only part of the buffer needs to be taken in
            {
                ota_err = esp_ota_write(ota_handle, (rxMsg->data + 1), image_size - receivedBytes);
                if (ota_err != ESP_OK) // Break if somehow the OTA doesn't write
                {
                    ESP_LOGE(__func__, "Could not write OTA segment : %s", esp_err_to_name(ota_err));
                    esp_ota_abort(ota_handle);
                    FC_sent = false;
                    FF_received = false;
                    OTA_started = false;
                    receivedBytes = 0;
                    transferComplete = false;
                    image_size = 0;
                    blockCounter = 0x00;
                    sequenceNumber = 0x01;
                    // Send the Error Frame
                    UDS_RESP_MSG.extd = false;
                    UDS_RESP_MSG.data[0] = 0x40;
                    UDS_RESP_MSG.data[1] = 0x05; // No write
                    for (size_t i = 2; i < 8; i++)
                    {
                        UDS_RESP_MSG.data[i] = 0xAA;
                    }
                    if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                        ESP_LOGE(__func__, "Could not queue internal state message in queue");
                    else
                        ESP_LOGI(__func__, "Error frame sent");
                    break;
                }
                receivedBytes = image_size;
                transferComplete = true;
            }

            if (transferComplete) // Need to reset everything
            {
                ESP_LOGI(__func__, "Transfer complete");
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                ota_err = esp_ota_end(ota_handle);
                if (ota_err != ESP_OK)
                {
                    ESP_LOGE(__func__, "OTA not successful : %s", esp_err_to_name(ota_err));
                    // Send the Error Frame
                    UDS_RESP_MSG.extd = false;
                    UDS_RESP_MSG.data[0] = 0x40;
                    UDS_RESP_MSG.data[1] = 0x02; // No verif
                    for (size_t i = 2; i < 8; i++)
                    {
                        UDS_RESP_MSG.data[i] = 0xAA;
                    }
                    if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                        ESP_LOGE(__func__, "Could not queue internal state message in queue");
                    else
                        ESP_LOGI(__func__, "Error frame sent");
                }
                else
                {
                    ESP_LOGI(__func__, "OTA image verification OK, setting boot to %s and restarting.", update_partition->label);
                    esp_ota_set_boot_partition(update_partition);
                    // Send the OK Frame
                    UDS_RESP_MSG.extd = false;
                    UDS_RESP_MSG.data[0] = 0x40;
                    UDS_RESP_MSG.data[1] = 0x00; // OK status
                    for (size_t i = 2; i < 8; i++)
                    {
                        UDS_RESP_MSG.data[i] = 0xAA;
                    }
                    if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                        ESP_LOGE(__func__, "Could not queue internal state message in queue");
                    else
                        ESP_LOGI(__func__, "Status frame sent");
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                }
            }

            // Send a FC frame if blockCounter == BSize
            if (BSize > 0 && blockCounter == BSize)
            {
                UDS_RESP_MSG.extd = false;
                UDS_RESP_MSG.data[0] = 0x30;
                UDS_RESP_MSG.data[1] = BSize;
                UDS_RESP_MSG.data[2] = ST_min;
                for (size_t i = 3; i < 8; i++)
                {
                    UDS_RESP_MSG.data[i] = 0xAA;
                }
                if (xQueueSend(CAN_TX_queue_hdl, &UDS_RESP_MSG, pdMS_TO_TICKS(1)) != pdTRUE)
                {
                    ESP_LOGE(__func__, "Could not queue internal state message in queue");
                    esp_ota_abort(ota_handle);
                    FC_sent = false;
                    FF_received = false;
                    OTA_started = false;
                    receivedBytes = 0;
                    transferComplete = false;
                    image_size = 0;
                    blockCounter = 0x00;
                    sequenceNumber = 0x01;
                    break;
                }
                else
                {
                    ESP_LOGI(__func__, "Flow Control Frame sent.");
                }
                FC_sent = true;
                sequenceNumber = 0x01;
                blockCounter = 0x00;
            }
            break; // Successful break from top level switch
        }
        else
        {
            ESP_LOGD(__func__, "Unknown frame type received, ignoring.");
            break;
        }
    }
    break;

    default:
    {
        ESP_LOGD(__func__, "Unknown CAN frame received: ID = 0x%03X", (uint16_t)rxMsg->identifier);
    }
    break;
    }

    return ESP_OK;
}
#pragma endregion

#pragma endregion

#pragma region FreeRTOS tasks
TaskHandle_t display_board_st_PKG_hdl;
TaskHandle_t display_board_version_PKG_hdl;

/// @brief Packaging handler that send the board state over CAN
/// @param pvParameters
void display_board_st_PKG(void *pvParameters)
{
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
    binocan_rdb_st_t binocan_rdb_st;
    binocan_rdb_st_init(&binocan_rdb_st);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_RDB_ST_FRAME_ID,
        .data_length_code = BINOCAN_RDB_ST_LENGTH};
#elifdef CONFIG_LEFT_SIDE_DISPLAY
    binocan_ldb_st_t binocan_ldb_st;
    binocan_ldb_st_init(&binocan_ldb_st);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_LDB_ST_FRAME_ID,
        .data_length_code = BINOCAN_LDB_ST_LENGTH};
#endif
    tx_msg.ss = false; // Redundant
    while (true)
    {
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_RDB_ST_CYCLE_TIME_MS));
        binocan_rdb_st.rdb_sm_st = binocan_rdb_st_rdb_sm_st_encode(display_board_st.internal_ST);
        binocan_rdb_st_pack(tx_msg.data, &binocan_rdb_st, BINOCAN_RDB_ST_LENGTH);
#elifdef CONFIG_LEFT_SIDE_DISPLAY
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_LDB_ST_CYCLE_TIME_MS));
        binocan_ldb_st.ldb_sm_st = binocan_ldb_st_ldb_sm_st_encode(display_board_st.internal_ST);
        binocan_ldb_st_pack(tx_msg.data, &binocan_ldb_st, BINOCAN_LDB_ST_LENGTH);
#endif
        // DEBUG
        // tx_msg.data[7] = screen_interlock_OK;
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue internal state message in queue");
        }
    }
}

void display_board_version_PKG(void *pvParameters)
{
    uint8_t message_mux = 0;
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
    binocan_rdb_board_version_t binocan_rdb_board_version;
    binocan_rdb_board_version_init(&binocan_rdb_board_version);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_RDB_BOARD_VERSION_FRAME_ID,
        .data_length_code = BINOCAN_RDB_BOARD_VERSION_LENGTH};
    uint32_t cycle_time_ms = BINOCAN_RDB_BOARD_VERSION_CYCLE_TIME_MS;

    binocan_rdb_board_version.rdb_version_major = binocan_rdb_board_version_rdb_version_major_encode((uint8_t)(display_board_st.app_metadata->base_version[0]) - 48);
    binocan_rdb_board_version.rdb_version_minor = binocan_rdb_board_version_rdb_version_minor_encode((uint8_t)(display_board_st.app_metadata->base_version[2]) - 48);
    binocan_rdb_board_version.rdb_version_patch = binocan_rdb_board_version_rdb_version_patch_encode((uint8_t)(display_board_st.app_metadata->base_version[4]) - 48);
    binocan_rdb_board_version.rdb_version_dirty = binocan_rdb_board_version_rdb_version_dirty_encode((uint8_t)display_board_st.app_metadata->is_dirty);
#elifdef CONFIG_LEFT_SIDE_DISPLAY
    binocan_ldb_board_version_t binocan_ldb_board_version;
    binocan_ldb_board_version_init(&binocan_ldb_board_version);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_LDB_BOARD_VERSION_FRAME_ID,
        .data_length_code = BINOCAN_LDB_BOARD_VERSION_LENGTH};
    uint32_t cycle_time_ms = BINOCAN_LDB_BOARD_VERSION_CYCLE_TIME_MS;

    binocan_ldb_board_version.ldb_version_major = binocan_ldb_board_version_ldb_version_major_encode((uint8_t)(display_board_st.app_metadata->base_version[0]) - 48);
    binocan_ldb_board_version.ldb_version_minor = binocan_ldb_board_version_ldb_version_minor_encode((uint8_t)(display_board_st.app_metadata->base_version[2]) - 48);
    binocan_ldb_board_version.ldb_version_patch = binocan_ldb_board_version_ldb_version_patch_encode((uint8_t)(display_board_st.app_metadata->base_version[4]) - 48);
    binocan_ldb_board_version.ldb_version_dirty = binocan_ldb_board_version_ldb_version_dirty_encode((uint8_t)display_board_st.app_metadata->is_dirty);

#endif
    // Build a 64-bit value from up to 8 bytes of commitID and pass to the encode helper.
    // This avoids memcpy into a numeric field and is lightweight.
    if (display_board_st.app_metadata && display_board_st.app_metadata->commitID)
    {
        // ESP_LOGI(TAG,"Commit is valid");
        uint64_t commit_val = 0;
        size_t src_len = display_board_st.app_metadata->commit_len;
        if (src_len > 8)
            src_len = 8;
        for (size_t i = 0; i < src_len; ++i)
        {
            commit_val = (commit_val << 8) | (uint8_t)display_board_st.app_metadata->commitID[src_len - i - 1];
        }
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
        binocan_rdb_board_version.rdb_version_commit = binocan_rdb_board_version_rdb_version_commit_encode(commit_val);
    }
    else
    {
        // ESP_LOGI(TAG,"Commit will be 0");
        binocan_rdb_board_version.rdb_version_commit = binocan_rdb_board_version_rdb_version_commit_encode(0ULL);
    }
#elifdef CONFIG_LEFT_SIDE_DISPLAY
        binocan_ldb_board_version.ldb_version_commit = binocan_ldb_board_version_ldb_version_commit_encode(commit_val);
    }
    else
    {
        // ESP_LOGI(TAG,"Commit will be 0");
        binocan_ldb_board_version.ldb_version_commit = binocan_ldb_board_version_ldb_version_commit_encode(0ULL);
    }
#endif

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(cycle_time_ms));
        // Switch the mux indicator
        message_mux = (message_mux + 1) % 2; // Currently only two values to the MUX
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
        binocan_rdb_board_version.rdb_version_mux = binocan_rdb_board_version_rdb_version_mux_encode(message_mux);
        binocan_rdb_board_version_pack(tx_msg.data, &binocan_rdb_board_version, BINOCAN_RDB_BOARD_VERSION_LENGTH);
#elifdef CONFIG_LEFT_SIDE_DISPLAY
        binocan_ldb_board_version.ldb_version_mux = binocan_ldb_board_version_ldb_version_mux_encode(message_mux);
        binocan_ldb_board_version_pack(tx_msg.data, &binocan_ldb_board_version, BINOCAN_LDB_BOARD_VERSION_LENGTH);
#endif
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue internal state message in queue");
        }
    }
}
#pragma endregion

#pragma region Main app

/// @brief Main app
extern "C" void app_main()
{
    // Declare board state
    display_board_st.internal_ST = XDB_SM_ST_INIT;

#pragma region App metadata parse
    // Allocate memory for app_metadata and fill it by reference
    display_board_st.app_metadata = (parsed_app_meta_t *)malloc(sizeof(parsed_app_meta_t));
    if (display_board_st.app_metadata)
    {
        esp_err_t meta_err = parse_app_metadata(display_board_st.app_metadata);
        if (meta_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to parse app metadata");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to allocate memory for app_metadata");
    }

#pragma endregion

#pragma region OTA and Rollback check

    bool rollBackPossible = esp_ota_check_rollback_is_possible();
    ESP_LOGI(__func__, "Rollback Possible ? %u", rollBackPossible);
    const esp_partition_t *runningPart = esp_ota_get_running_partition();
    esp_ota_img_states_t imageState;
    esp_ota_get_state_partition(runningPart, &imageState);
    bool firstBoot = (imageState == ESP_OTA_IMG_PENDING_VERIFY);
    ESP_LOGI(__func__, "First boot ? %u", firstBoot);

    // Init NVS and write last partition info (for factory app)
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err != ESP_OK)
        ESP_LOGE(__func__, "Cannot init default NVS");
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGE(__func__, "Could not init default NVS, erasing and retrying.");
        nvs_flash_erase();
        nvs_flash_init();
    }
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
    {
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        if (rollBackPossible)
        {
            ESP_LOGW(__func__, "Activating rollback on next reboot.");
            esp_ota_mark_app_invalid_rollback();
        }
    }
    else
    {
        if (strcmp("ota_0", runningPart->label) == 0)
        {
            ESP_LOGI(__func__, "Running partition is ota_0");
            nvs_set_i8(h, "lastPart", 0);
        }
        else if (strcmp("ota_1", runningPart->label) == 0)
        {
            ESP_LOGI(__func__, "Running partition is ota_1");
            nvs_set_i8(h, "lastPart", 1);
        }
        else
        {
            ESP_LOGW(__func__, "Current running partition could not be identified, defaulting to factory.");
            nvs_set_i8(h, "lastPart", -1);
        }
        nvs_err = nvs_get_u8(h, "mph_on", &(display_board_st.mph_selected));
        nvs_err = nvs_get_u8(h, "rpm_al_overr", &(display_board_st.rpm_alarm_override));
        nvs_err = nvs_get_u32(h, "rpm_al_thr", &(display_board_st.rpm_alarm_threshold));
        nvs_commit(h);
        nvs_close(h);
    }

    // To populate the debug screen
    const esp_app_desc_t *app_metadata;
    app_metadata = esp_app_get_description();
    if (!app_metadata)
    {
        ESP_LOGE(TAG, "No app metadata available");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        esp_ota_mark_app_invalid_rollback();
    }

#pragma endregion

#pragma region Setup

    // CAN communications
    ESP_LOGI(__func__, "Starting TWAI port and daemon");
    if (initCAN(&dispatchFrame) != ESP_OK)
    {
        ESP_LOGW(__func__, "Issue starting TWAI port and daemon.");
        if (rollBackPossible)
        {
            ESP_LOGW(__func__, "Activating rollback on next reboot.");
            esp_ota_mark_app_invalid_rollback();
        }
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
    }
    // Set up state transmission over CAN
    if (xTaskCreate(display_board_st_PKG, "XDB_ST", 4096, NULL, 3, &display_board_st_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create display state package task");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        if (rollBackPossible)
            esp_ota_mark_app_invalid_rollback();
        else
            ESP_LOGW(__func__, "Rollback impossible, image could not be invalidated.");
    }
    if (xTaskCreate(display_board_version_PKG, "XDB_VER", 4096, NULL, 3, &display_board_version_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create display version package task");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        if (rollBackPossible)
            esp_ota_mark_app_invalid_rollback();
        else
            ESP_LOGW(__func__, "Rollback impossible, image could not be invalidated.");
    }

    // Board initialization
    ESP_LOGI(__func__, "Initializing board");

    Board *board = new Board();
    if (!(board->init()))
    {
        ESP_LOGW(__func__, "Could not initialize board.");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        if (rollBackPossible)
            esp_ota_mark_app_invalid_rollback();
        else
            ESP_LOGW(__func__, "Rollback impossible, image could not be invalidated.");
    }
    else
    {
        auto lcd = board->getLCD();
        if (!(lcd->configFrameBufferNumber(LVGL_PORT_BUFFER_NUM)))
        {
            ESP_LOGW(__func__, "Could not set up framebuffers.");
            display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
            if (rollBackPossible)
                esp_ota_mark_app_invalid_rollback();
            else
                ESP_LOGW(__func__, "Rollback impossible, image could not be invalidated.");
        }
        else
        {
            // Setting up the Bounce Buffer size (might not be necessary)
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
            auto lcd_bus = lcd->getBus();
            if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB)
            {
                if (!(static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 20)))
                {
                    ESP_LOGW(__func__, "Could not set up bounce buffer.");
                    display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
                    if (rollBackPossible)
                        esp_ota_mark_app_invalid_rollback();
                    else
                        ESP_LOGW(__func__, "Rollback impossible, image could not be invalidated.");
                }
            }
#endif
        }
        // Board start
        if (!(board->begin()))
        {
            ESP_LOGW(__func__, "Could not begin() board.");
            display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
            if (rollBackPossible)
                esp_ota_mark_app_invalid_rollback();
            else
                ESP_LOGW(__func__, "Rollback impossible, image could not be invalidated.");
        }
        else
        {
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
        }
    }

    if (!(lvgl_port_init(board->getLCD(), board->getTouch())))
    {
        ESP_LOGW(__func__, "Could not start LVGL port.");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        if (rollBackPossible)
            esp_ota_mark_app_invalid_rollback();
        else
            ESP_LOGW(__func__, "Rollback impossible, image could not be invalidated.");
    }
#pragma region Starting animation
    // UI loading and mofidifiers
    ESP_LOGI(__func__, "Loading UI");
    ESP_UTILS_CHECK_FALSE_EXIT(lvgl_port_lock(-1), "Failed to perform initial LVGL Mutex lock");
    ui_init(); // Load the UI library and draw it
    // Set up the debug screen
    lv_label_set_text_fmt(objects.version_info, "%s - %s - %s", app_metadata->version, app_metadata->date, app_metadata->time);
    lv_label_set_text_fmt(objects.project_info, "%s", app_metadata->project_name);
    lv_label_set_text_fmt(objects.current_partition, "%s", runningPart->label);
#ifdef CONFIG_LEFT_SIDE_DISPLAY
    lv_obj_set_style_pad_radial(objects.rpm_scale, 20, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    lv_scale_set_text_src(objects.rpm_scale, rpm_scale_labels);

    switch (display_board_st.rpm_alarm_override)
    {
    case 1:
        lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, true);
        lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_spinbox_set_value(objects.rpm_alarm_spinbox, (int32_t)display_board_st.rpm_alarm_threshold);
        break;
    case 0:
        lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, false);
        lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_spinbox_set_value(objects.rpm_alarm_spinbox, 0);
        break;
    default:
        ESP_LOGE(__func__, "Unauthorized RPM alarm override state");
        break;
    }

#elifdef CONFIG_RIGHT_SIDE_DISPLAY
    lv_obj_set_style_pad_radial(objects.speed_scale, 15, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    if (display_board_st.mph_selected == 0)
    {
        lv_scale_set_range(objects.speed_scale, 0, 2400);
        lv_scale_set_total_tick_count(objects.speed_scale, 49);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_kph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 2400);
        lv_label_set_text(objects.speed_unit, "KPH");
        lv_obj_set_state(objects.mph_on, LV_STATE_CHECKED, false);
    }
    else if (display_board_st.mph_selected == 1)
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
    ESP_LOGI(__func__, "Backlight : %d", board->getBacklight()->on());
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
#pragma endregion
    ESP_LOGI(__func__, "Setup done");
    if (display_board_st.internal_ST != XDB_SM_ST_DEGRADED)
    {
        esp_ota_mark_app_valid_cancel_rollback();
        display_board_st.internal_ST = XDB_SM_ST_OK;
        ESP_LOGI(__func__, "App image is valid.");
    }
    vTaskResume(CAN_RX_tsk_hdl);
#pragma region Main Loop
    while (true)
    {

        vTaskDelay(pdMS_TO_TICKS(DISP_VALUES_REFRESH_INTERVAL));
        // generateValues();

        // Attempt locking LVGL elements prior to updating them (issue with jumping frames ?)
        if (lvgl_port_lock(-1))
        {
            updateLVGLObjects();
            lvgl_port_unlock();
        }
    }
#pragma endregion
}