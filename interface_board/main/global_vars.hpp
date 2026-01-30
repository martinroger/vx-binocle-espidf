#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "version_parser.h" // For parsed app metadata struct

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