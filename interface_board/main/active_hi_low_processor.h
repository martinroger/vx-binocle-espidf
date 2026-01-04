// Uses IO expander interrupt and other GPIO interrupts to trigger update of the internal signals
// Uses task notification from interrupts to read the IO expander
#pragma once
#include "tca9555_helpers.h"

#ifdef TAG
#undef TAG
#endif

#define TAG "ActHiLo Processor"

#define EXP_IO_0_BITMASK (1 << 0)
#define EXP_IO_1_BITMASK (1 << 1)
#define EXP_IO_2_BITMASK (1 << 2)
#define EXP_IO_3_BITMASK (1 << 3)
#define EXP_IO_4_BITMASK (1 << 4)
#define EXP_IO_5_BITMASK (1 << 5)
#define EXP_IO_6_BITMASK (1 << 6)
#define EXP_IO_7_BITMASK (1 << 7)
#define EXP_IO_8_BITMASK (1 << 8)
#define EXP_IO_9_BITMASK (1 << 9)
#define EXP_IO_10_BITMASK (1 << 10)
#define EXP_IO_11_BITMASK (1 << 11)
#define EXP_IO_12_BITMASK (1 << 12)
#define EXP_IO_13_BITMASK (1 << 13)
#define EXP_IO_14_BITMASK (1 << 14)
#define EXP_IO_15_BITMASK (1 << 15)

/// @brief Structure holding the logic values for the inputs. Physical values will be INVERTED !
struct active_hi_lo_grp_t
{
    bool AH_ignition = false;      // Ignition presence, active HIGH
    bool AH_hi_beams = false;      // High beams enabled, active HIGH
    bool AL_alternator = false;    // Alternator/Charge telltale, active LOW
    bool AL_brake_low = false;     // Brake (fluid) alter, active LOW
    bool AL_parking_brake = false; // Parking Brake indicator, active LOW
    bool AL_oil_pressure = false;  // Low oil pressure telltale, active LOW
    bool AL_airbag = false;        // Airbag fault telltale, active LOW
    bool AL_CEL = false;           // Check Engine Light, active LOW
    bool AH_right_turn = false;    // Right turn indicator, active HIGH
    bool AH_left_turn = false;     // Left turn indicator, active HIGH
    bool AL_ABS = false;           // ABS telltale, active LOW
    bool AL_door = false;          // Door switch, active LOW
    bool AH_coolant_low = false;   // Coolant level alert, active HIGH
    bool AL_button = false;        // Steering column input button, active LOW
    bool AH_alarm = false;         // Alarm ? RPM ? only on tubbies ? active HIGH assumed
    bool AH_backlight = false;     // Backlight/low beams input, active HIGH
} active_hi_lo_grp;

/// @brief Utility that associates a boolean return to a position being ON in the bitmask
/// @param bitfield 8-bit bitfield containing the various values of the IO expander
/// @param bitmask Bitmask to filter the exact boolean to parse
/// @return
inline bool read_bitmask(uint16_t bitfield, uint16_t bitmask)
{
    bool ret = false;
    ret = ((bitfield & bitmask) == bitmask);
    return ret;
}

/// @brief Initialisation routine for expander IO active high low routines
/// @return ESP_OK when all initialised correctly, otherwise ESP_FAIL
inline esp_err_t initialize_exp_active_hi_lo_proc()
{
    if (initialize_io_expanders() != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}
