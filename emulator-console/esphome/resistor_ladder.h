#pragma once
#include "esphome.h"
#include <cstdint>

namespace esphome {
namespace vehicle_emulator {

// 19 discrete resistor network masks and corresponding resistance values (Ohm)
// Note: Cluster logic is inverted: ~270 Ohm is Full Tank, ~0 Ohm is Empty Tank.
static const uint16_t FUEL_RES_MASKS[19] = {
    0x7FFF, // Level 1:  30.2 Ohm (Empty Tank / near 0 Ohm)
    0x3F3F, // Level 2:  39.6 Ohm
    0x071F, // Level 3:  50.0 Ohm
    0x0F0F, // Level 4:  59.5 Ohm
    0x3F07, // Level 5:  70.9 Ohm
    0x0707, // Level 6:  79.3 Ohm (1/4 Tank)
    0x0007, // Level 7:  90.0 Ohm
    0x1F03, // Level 8:  100.9 Ohm
    0x0703, // Level 9:  112.3 Ohm
    0x0303, // Level 10: 118.9 Ohm (1/2 Tank)
    0xFF01, // Level 11: 129.8 Ohm
    0x7F01, // Level 12: 138.8 Ohm
    0x3F01, // Level 13: 149.2 Ohm
    0x1F01, // Level 14: 161.2 Ohm (3/4 Tank)
    0x0F01, // Level 15: 175.3 Ohm
    0x0701, // Level 16: 192.2 Ohm
    0x0301, // Level 17: 212.6 Ohm
    0x0101, // Level 18: 237.9 Ohm
    0x0001  // Level 19: 270.0 Ohm (Full Tank)
};

static const float FUEL_RES_VALUES[19] = {
    30.2f, 39.6f, 50.0f, 59.5f, 70.9f, 79.3f, 90.0f, 100.9f, 112.3f, 118.9f,
    129.8f, 138.8f, 149.2f, 161.2f, 175.3f, 192.2f, 212.6f, 237.9f, 270.0f
};

// Read a 16-bit mask from TCA9555 at given address (Reg 0x00 Input Port 0 & Port 1)
inline bool read_expander_mask(i2c::I2CBus *bus, uint8_t i2c_addr, uint16_t &mask) {
    if (bus == nullptr) return false;
    uint8_t reg = 0x00;
    uint8_t data[2] = {0, 0};
    if (bus->write_readv(i2c_addr, &reg, 1, data, 2) != i2c::ERROR_OK) {
        ESP_LOGE("RESISTOR_LADDER", "Failed to read mask from TCA9555 at 0x%02X", i2c_addr);
        return false;
    }
    mask = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
    ESP_LOGD("RESISTOR_LADDER", "TCA9555 (0x%02X) current mask is 0x%04X", i2c_addr, mask);
    return true;
}

// Calculate equivalent resistance (Ohm) from a 16-bit resistor expander mask (0x21)
// Low caliber divider: lower byte (bits 0..7) has active branches in parallel with 270 Ohm base load.
// High caliber divider: upper byte (bits 8..15) has active branches in parallel with 2000 Ohm base load.
inline float calculate_resistance_from_mask(uint16_t mask) {
    // Check if mask matches one of the 19 standard calibrated fuel resistor steps
    for (int i = 0; i < 19; i++) {
        if (FUEL_RES_MASKS[i] == mask) {
            return FUEL_RES_VALUES[i];
        }
    }

    uint8_t low_byte = mask & 0xFF;
    uint8_t high_byte = (mask >> 8) & 0xFF;

    int count_low = 0;
    for (int b = 0; b < 8; b++) {
        if (low_byte & (1 << b)) count_low++;
    }

    int count_high = 0;
    for (int b = 0; b < 8; b++) {
        if (high_byte & (1 << b)) count_high++;
    }

    float conductance = 0.0f;
    if (count_low > 0) {
        conductance += ((float)count_low / 270.0f);
    }
    if (count_high > 0) {
        conductance += ((float)count_high / 2000.0f);
    }

    if (conductance > 0.0f) {
        return 1.0f / conductance;
    }

    return NAN; // Open circuit
}

// Write a 16-bit mask to TCA9555 at address 0x21 (resistor network expander)
inline bool write_resistor_mask(i2c::I2CBus *bus, uint8_t i2c_addr, uint16_t mask) {
    if (bus == nullptr) return false;
    
    // Set all 16 pins as OUTPUT (Reg 0x06: Configuration Port 0, followed by Port 1)
    uint8_t cfg_data[3] = {0x06, 0x00, 0x00};
    if (bus->write(i2c_addr, cfg_data, 3) != i2c::ERROR_OK) {
        ESP_LOGE("RESISTOR_LADDER", "Failed to configure TCA9555 direction at 0x%02X", i2c_addr);
        return false;
    }

    // Set outputs to target 16-bit mask (Reg 0x02: Output Port 0, followed by Port 1)
    uint8_t out_data[3] = {
        0x02,
        static_cast<uint8_t>(mask & 0xFF),
        static_cast<uint8_t>((mask >> 8) & 0xFF)
    };
    if (bus->write(i2c_addr, out_data, 3) != i2c::ERROR_OK) {
        ESP_LOGE("RESISTOR_LADDER", "Failed to write mask 0x%04X to TCA9555 at 0x%02X", mask, i2c_addr);
        return false;
    }

    ESP_LOGD("RESISTOR_LADDER", "TCA9555 (0x%02X) output mask set to 0x%04X", i2c_addr, mask);
    return true;
}

// Set fuel step (1 to 19)
inline bool set_fuel_step(i2c::I2CBus *bus, uint8_t i2c_addr, int step) {
    if (step < 1) step = 1;
    if (step > 19) step = 19;
    uint16_t mask = FUEL_RES_MASKS[step - 1];
    return write_resistor_mask(bus, i2c_addr, mask);
}

// Set low-caliber divider (0..8, 0 is open circuit).
// Reads current mask and preserves high-caliber byte (upper 8 bits).
inline bool set_low_caliber_divider(i2c::I2CBus *bus, uint8_t i2c_addr, int divider) {
    if (divider < 0) divider = 0;
    if (divider > 8) divider = 8;
    
    uint16_t current_mask = 0;
    read_expander_mask(bus, i2c_addr, current_mask);

    uint8_t low_byte = 0;
    for (int i = 0; i < divider; i++) {
        low_byte = (low_byte << 1) | 1;
    }

    uint16_t new_mask = (current_mask & 0xFF00) | low_byte;
    return write_resistor_mask(bus, i2c_addr, new_mask);
}

// Set high-caliber divider (0..8, 0 is open circuit).
// Reads current mask and preserves low-caliber byte (lower 8 bits).
inline bool set_high_caliber_divider(i2c::I2CBus *bus, uint8_t i2c_addr, int divider) {
    if (divider < 0) divider = 0;
    if (divider > 8) divider = 8;
    
    uint16_t current_mask = 0;
    read_expander_mask(bus, i2c_addr, current_mask);

    uint8_t high_byte = 0;
    for (int i = 0; i < divider; i++) {
        high_byte = (high_byte << 1) | 1;
    }

    uint16_t new_mask = (current_mask & 0x00FF) | (static_cast<uint16_t>(high_byte) << 8);
    return write_resistor_mask(bus, i2c_addr, new_mask);
}

// Find the closest standard fuel step (1..19) for a given target resistance in Ohms
inline int find_closest_fuel_step(float target_ohm) {
    int best_step = 1;
    float min_diff = 1e9f;
    for (int i = 0; i < 19; i++) {
        float diff = std::abs(FUEL_RES_VALUES[i] - target_ohm);
        if (diff < min_diff) {
            min_diff = diff;
            best_step = i + 1;
        }
    }
    return best_step;
}

// Extract the number of active branches (divider 0..8) from a low-caliber byte (bits 0..7)
inline int get_low_caliber_divider_from_mask(uint16_t mask) {
    uint8_t low_byte = mask & 0xFF;
    int count = 0;
    for (int b = 0; b < 8; b++) {
        if (low_byte & (1 << b)) count++;
    }
    return count;
}

// Extract the number of active branches (divider 0..8) from a high-caliber byte (bits 8..15)
inline int get_high_caliber_divider_from_mask(uint16_t mask) {
    uint8_t high_byte = (mask >> 8) & 0xFF;
    int count = 0;
    for (int b = 0; b < 8; b++) {
        if (high_byte & (1 << b)) count++;
    }
    return count;
}

} // namespace vehicle_emulator
} // namespace esphome
