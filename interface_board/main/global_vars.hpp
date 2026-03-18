#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "version_parser.h"        // For parsed app metadata struct
#include "odometer.h"              //For odometer and trip variables
#include "mcpwm_capture_helpers.h" // For pwm_info_t struct and capture handles

struct board_ST
{
    bool EN_hi_R_sense_ST = false;   // Is the high Resistance caliber on
    bool EN_5_V_ST = false;          // Is the main 5V power supply enabled
    bool EN_5_V_AUX_ST = false;      // Is the Auxiliary 5V power supply enabled
    uint8_t internal_ST = 0x00;      // Internal state of the board
    bool expander_ST = true;         // Is the IO Expander available and running
    bool adc_ST = true;              // Is the ADC available and running
    float mcu_temperature = 0.0;     // Internally measured MCU temperature (°C)
    bool LDB_check_alive_ST = false; // Is the Left Display reporting 3V3 on the control GPIO
    bool RDB_check_alive_ST = false; // Is the Right Display reporting 3V3 on the control GPIO
    bool lowFuel = false;            // Is the low fuel flag internally set
    bool overTemp = false;           // Is the internal overtemp flag set
    parsed_app_meta_t *app_metadata; // Parsed app metadata for serving over CAN
} interface_board_st;

bool rollBackPossible; // Is rollback possible ?
bool firstBoot;        // Is this the first boot after OTA ?

// Placeholders editable in factory mode
uint16_t fuel_lvl_comp_factor = 1000; // Is divided by 1000.0 later
uint16_t fuel_low_level_threshold_pc = 20;
uint16_t coolant_overtemp_threshold_degC = 106;

// Gear ratios and related metrics
size_t float_size = sizeof(float);
float gear_ratio_1 = 1.0;
float gear_ratio_2 = 1.83;
float gear_ratio_3 = 2.73;
float gear_ratio_4 = 3.73;
float gear_ratio_5 = 4.5;
float gear_ratio_R = 10.0;
float gear_ema_alpha = 0.05;
float gear_stability_tolerance = 3;

// Gear position estimator variables
float gear_ratio_raw = 0.0;
float gear_ratio_filtered = 0.0;

#pragma region MCPWM declarations
// PWM stats structures for coolant, rpm and speed captures
static volatile pwm_info_t pwm_cap_coolant, pwm_cap_rpm, pwm_cap_speed = {.pos_edge_ts = 0, .prev_pos_edge_ts = 0, .period_ticks = 0, .neg_edge_ts = 0, .deltaT = 0};

// Capture channels
mcpwm_cap_channel_handle_t cap_chan_coolant = NULL;
mcpwm_cap_channel_handle_t cap_chan_rpm = NULL;
mcpwm_cap_channel_handle_t cap_chan_speed = NULL;
#pragma endregion