#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "version_parser.h"        // For parsed app metadata struct
#include "odometer.h"              // For odometer and trip variables
#include "mcpwm_capture_helpers.h" // For pwm_info_t struct and capture handles
#include "gear_estimator.h"         // For gear estimator
#include "r39_compliance.hpp"       // For the ECE R39 speed compliance mode

struct ItfBoardState
{
    bool hiRangeSense = false;   // Is the high Resistance caliber on
    bool en5V = false;          // Is the main 5V power supply enabled
    bool en5VAux = false;      // Is the Auxiliary 5V power supply enabled
    uint8_t smState = 0x00;      // Internal state of the board
    bool expanderOk = true;         // Is the IO Expander available and running
    bool adcOk = true;              // Is the ADC available and running
    float mcuTempDegC = 0.0;     // Internally measured MCU temperature (°C)
    bool ldbAlive = false; // Is the Left Display reporting 3V3 on the control GPIO
    bool rdbAlive = false; // Is the Right Display reporting 3V3 on the control GPIO
    bool lowFuel = false;            // Is the low fuel flag internally set
    bool coolantHot = false;           // Is the internal overtemp flag set
    parsed_app_meta_t *appMeta; // Parsed app metadata for serving over CAN
} itfBoard;

bool rollbackPossible;      // Is rollback possible ?
bool firstBoot;             // Is this the first boot after OTA ?
bool ldbReady = false;
bool rdbReady = false;
bool animationDone = false; // Has the animation been sent out

// Placeholders editable in factory mode
uint16_t fuelFullOhm = (uint16_t)COEFF_FUEL_FULL_R; // Learned or calibrated full tank resistance in Ohms (Default: 250)
bool fuelLearnEnabled = true;                          // Enable/disable self-learning of fuelFullOhm
bool fuelSenderOpen = false;              // Fuel sender open circuit flag (>3.4kOhm)
uint16_t fuelLowPct = 20;
uint16_t coolantHotDegC = 106;
r39::Params r39Params = r39::defaults().to_params(CONFIG_R39_BLEND_END_KPH); // ECE R39 speed compliance, loaded from NVS at boot, read-only afterwards

#pragma region MCPWM declarations
// PWM stats structures for coolant, rpm and speed captures
static volatile pwm_info_t pwmCapCoolant, pwmCapRpm, pwmCapSpeed = {.pos_edge_ts = 0, .prev_pos_edge_ts = 0, .period_ticks = 0, .neg_edge_ts = 0, .deltaT = 0};

// Capture channels
mcpwm_cap_channel_handle_t capChanCoolant = NULL;
mcpwm_cap_channel_handle_t capChanRpm = NULL;
mcpwm_cap_channel_handle_t capChanSpeed = NULL;

TaskHandle_t acquireMcpwmTaskHandle = NULL; // Task handle for MCPWM acquisition & gear estimation
#pragma endregion

#pragma region Gear estimator
gear_state_t gearEstimator; // Bayesian gear estimator state
#pragma endregion