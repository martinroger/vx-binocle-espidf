#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_display_panel.hpp"
#include "version_parser.h"
#include "binocan.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

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

#pragma region COMMON
/// @brief Internal state structure
struct board_ST
{
    bool screen_interlock_OK = false;    // Left to right screen interlock state
    uint8_t internal_ST = XDB_SM_ST_OFF; // Internal board state
    bool mph_selected = false;           // MPH unit selected over KPH
    uint32_t rpm_decimation = 10;        // RPM decimation factor
    bool rpm_alarm_override = false;     // RPM alarm override from vehicle
    uint32_t rpm_alarm_threshold = 6000; // RPM value at which to trigger alarm
    bool rpm_alarm_blink = false;        // Whether the alarm blinker is enabled
    bool use_shift_indicator = false;    // Activation of the SHIFT alarm
    uint32_t shift_mid_threshold = 5500; // Orange SHIFT RPM threshold
    uint32_t shift_top_threshold = 6500; // Red SHIFT RPM threshold
    bool overTemp_buzz = false;          // Overtemperature buzzer enable
    parsed_app_meta_t *app_metadata;     // App metadata
    Backlight *backLight;                // Pointer to backligh controller
    IO_Expander *ioExpander;             // Pointer to IO expander controller
    uint8_t darkBrightness = 100;        // Screen brightness in dark mode
    uint8_t lightBrightness = 100;       // Screen brightness in light mode
    bool lightMode = true;               // Light mode active indicator
    bool modeLocked = false;             // Light/Dark mode override lock indicator
} display_board_st;

// Validation-related booleans
bool rollBackPossible; // Is rollback possible ?
bool firstBoot;        // Is this the first boot after OTA ?

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
bool headlightsOn, p_headlightsOn = true;

// Vehicle numerical parameters
float speed_kph, p_speed_kph = 0;
float lvVoltage_v, p_lvVoltage_v = 12.0;
uint32_t rpm, p_rpm = 0;
uint8_t fuelLevel_pc, p_fuelLevel_pc = 50;
uint8_t coolant_degC, p_coolant_degC = 88;

// Odometer/trip values
float odometer_km, p_odometer_km = 0.0;
float trip_km, p_trip_km = 0.0;
#pragma endregion

#pragma region SPECIFIC
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
static const char *speed_kph_scale_labels[14] = {"0", "20", "40", "60", "80", "100", "120", "140", "160", "180", "200", "220", "240", NULL};
static const char *speed_mph_scale_labels[10] = {"0", "20", "40", "60", "80", "100", "120", "140", "160", NULL};
#elifdef CONFIG_LEFT_SIDE_DISPLAY
static const char *rpm_scale_labels[10] = {"0", "10", "20", "30", "40", "50", "60", "70", "80", NULL};
#endif
#pragma endregion