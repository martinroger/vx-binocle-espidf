#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_log.h"
#include "driver/gpio.h"
#include "display_hw.h"
#include "version_parser.h"
#include "binocan.h"
#include "nvs_settings.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

TaskHandle_t updateUiTaskHandle = NULL;

// ESP32 is Little Endian, UDS is BE
template <typename T>
T swapEndian(T u)
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
struct DisplayBoardState
{
    bool interlockOk = false;    // Left to right screen interlock state
    uint8_t smState = XDB_SM_ST_OFF; // Internal board state
    bool unitMph = false;           // MPH unit selected over KPH
    uint32_t rpmDecim = 10;        // RPM decimation factor
    bool alarmOverride = false;     // RPM alarm override from vehicle
    uint32_t alarmRpm = 6000; // RPM value at which to trigger alarm
    bool alarmBlink = false;        // Whether the alarm blinker is enabled
    bool shiftEnabled = false;    // Activation of the SHIFT alarm
    uint32_t shiftMidRpm = 5500; // Orange SHIFT RPM threshold
    uint32_t shiftTopRpm = 6500; // Red SHIFT RPM threshold
    bool hotBuzzEnabled = false;          // Overtemperature buzzer enable
    parsed_app_meta_t *appMeta;     // App metadata
    bool displayHwReady = false;         // Backlight and IO expander are initialised
    uint8_t blDarkPct = 100;        // Screen brightness in dark mode
    uint8_t blLightPct = 100;       // Screen brightness in light mode
    bool themeLight = true;               // Light mode active indicator
    bool themeLocked = false;             // Light/Dark mode override lock indicator
    bool gearShow = false;       // Show gear position instead of RPM
} displayBoard;

// Validation-related booleans
bool rollbackPossible; // Is rollback possible ?
bool firstBoot;        // Is this the first boot after OTA ?

// Used only to selectively update in LVGL
bool interlockOk = false, prevInterlockOk = false; // Checks opposite display status
int64_t lastInterlockTs;
uint8_t prevSmState = XDB_SM_ST_DEGRADED; // Checks internal state in the LVGL elements update routine
uint8_t itfState = XDB_SM_ST_DEGRADED, prevItfState = XDB_SM_ST_DEGRADED;
bool prevCanRxTimedOut = true;

// Vehicle variables and previous values retainers
bool indicatorsOn = true, prevIndicatorsOn = true;
bool rightTurnOn = true, prevRightTurnOn = true;
bool leftTurnOn = true, prevLeftTurnOn = true;
bool highBeamOn = true, prevHighBeamOn = true;
bool lowFuelOn = true, prevLowFuelOn = true;
bool overTemperatureOn = true, prevOverTemperatureOn = true;
bool brakesOn = true, prevBrakesOn = true;
bool absOn = true, prevAbsOn = true;
bool parkingBrakeOn = true, prevParkingBrakeOn = true;
bool lowCoolantOn = true, prevLowCoolantOn = true;
bool batteryOn = true, prevBatteryOn = true;
bool lowOilOn = true, prevLowOilOn = true;
bool milOn = true, prevMilOn = true;
bool airbagOn = true, prevAirbagOn = true;
bool ignitionOn = false, prevIgnitionOn = false;
bool alarmOn = true, prevAlarmOn = true;
bool headlightsOn = true, prevHeadlightsOn = true;

// Vehicle numerical parameters
float speedKph, prevSpeedKph = 0;
float supplyV, prevSupplyV = 12.0;
uint32_t rpm, prevRpm = 0;
uint8_t gearPosition, prevGearPosition = BINOCAN_ITF_FAST_METRICS_ITF_GEAR_POSITION_ST_NEUTRAL_CHOICE;
uint8_t fuelLevelPct, prevFuelLevelPct = 50;
uint8_t coolantDegC, prevCoolantDegC = 88;

// Odometer/trip values
float odometerKm, prevOdometerKm = 0.0;
float tripKm, prevTripKm = 0.0;
#pragma endregion

#pragma region SPECIFIC
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
static const char *speedKphScaleLabels[14] = {"0", "20", "40", "60", "80", "100", "120", "140", "160", "180", "200", "220", "240", NULL};
static const char *speedMphScaleLabels[10] = {"0", "20", "40", "60", "80", "100", "120", "140", "160", NULL};
#elifdef CONFIG_LEFT_SIDE_DISPLAY
static const char *rpm_scale_labels[10] = {"0", "10", "20", "30", "40", "50", "60", "70", "80", NULL};
#endif
#pragma endregion