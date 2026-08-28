#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <math.h>

// Filters section
#include "sma_filter.h"

// #include "esp_ota_ops.h"

#include "global_vars.hpp"
#include "twai_daemon.h"
#include "binocan.h"
#include "active_hi_low_processor.h"
#include "adc_processor.h"
#include "nvs.h"
#include "nvs_flash.h"

#pragma region FreeRTOS tasks for CAN packaging
// FreeRTOS handles
TaskHandle_t itf_slow_metrics_PKG_hdl;
TaskHandle_t itf_fast_metrics_PKG_hdl;
TaskHandle_t itf_odometer_PKG_hdl;
TaskHandle_t itf_board_st_PKG_hdl;
TaskHandle_t itf_board_version_PKG_hdl;

// Handles for the debug message packaging tasks
#ifdef CONFIG_DBG_SPEED_MSG
TaskHandle_t dbg_itf_speed_PKG_hdl;

/// @brief Packaging task for debug speed message
/// @param pvParameters
inline void dbg_itf_speed_PKG(void *pvParameters)
{
    binocan_dbg_itf_speed_t binocan_dbg_itf_speed;
    binocan_dbg_itf_speed_init(&binocan_dbg_itf_speed);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_DBG_ITF_SPEED_FRAME_ID,
        .data_length_code = BINOCAN_DBG_ITF_SPEED_LENGTH};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_DBG_ITF_SPEED_CYCLE_TIME_MS));
        binocan_dbg_itf_speed.dbg_speed_freq = binocan_dbg_itf_speed_dbg_speed_freq_encode(pwm_cap_speed.frequency);
        float duty_cycle = pwm_cap_speed.duty_cycle * 100.0;
        if (duty_cycle > 100.0)
            duty_cycle = 100.0;
        else if (duty_cycle < 0.0)
            duty_cycle = 0.0;
        binocan_dbg_itf_speed.dbg_speed_duty = binocan_dbg_itf_speed_dbg_speed_duty_encode(duty_cycle);
        binocan_dbg_itf_speed_pack(tx_msg.data, &binocan_dbg_itf_speed, BINOCAN_DBG_ITF_SPEED_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue debug speed message in queue");
        }
    }
}
#endif
#ifdef CONFIG_DBG_RPM_MSG
TaskHandle_t dbg_itf_rpm_PKG_hdl;

/// @brief Packaging task for debug rpm message
/// @param pvParameters
inline void dbg_itf_rpm_PKG(void *pvParameters)
{
    binocan_dbg_itf_rpm_t binocan_dbg_itf_rpm;
    binocan_dbg_itf_rpm_init(&binocan_dbg_itf_rpm);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_DBG_ITF_RPM_FRAME_ID,
        .data_length_code = BINOCAN_DBG_ITF_RPM_LENGTH};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_DBG_ITF_RPM_CYCLE_TIME_MS));
        binocan_dbg_itf_rpm.dbg_rpm_freq = binocan_dbg_itf_rpm_dbg_rpm_freq_encode(pwm_cap_rpm.frequency);
        float duty_cycle = pwm_cap_rpm.duty_cycle * 100.0;
        if (duty_cycle > 100.0)
            duty_cycle = 100.0;
        else if (duty_cycle < 0.0)
            duty_cycle = 0.0;
        binocan_dbg_itf_rpm.dbg_rpm_duty = binocan_dbg_itf_rpm_dbg_rpm_duty_encode(pwm_cap_rpm.duty_cycle * 100.0);
        binocan_dbg_itf_rpm_pack(tx_msg.data, &binocan_dbg_itf_rpm, BINOCAN_DBG_ITF_RPM_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue debug rpm message in queue");
        }
    }
}
#endif
#ifdef CONFIG_DBG_COOL_MSG
TaskHandle_t dbg_itf_coolant_PKG_hdl;

/// @brief Packaging task for debug coolant message
/// @param pvParameters
inline void dbg_itf_coolant_PKG(void *pvParameters)
{
    binocan_dbg_itf_coolant_t binocan_dbg_itf_coolant;
    binocan_dbg_itf_coolant_init(&binocan_dbg_itf_coolant);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_DBG_ITF_COOLANT_FRAME_ID,
        .data_length_code = BINOCAN_DBG_ITF_COOLANT_LENGTH};
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_DBG_ITF_COOLANT_CYCLE_TIME_MS));
        binocan_dbg_itf_coolant.dbg_coolant_freq = binocan_dbg_itf_coolant_dbg_coolant_freq_encode(pwm_cap_coolant.frequency);
        float duty_cycle = pwm_cap_coolant.duty_cycle * 100.0;
        if (duty_cycle > 100.0)
            duty_cycle = 100.0;
        else if (duty_cycle < 0.0)
            duty_cycle = 0.0;
        binocan_dbg_itf_coolant.dbg_coolant_duty = binocan_dbg_itf_coolant_dbg_coolant_duty_encode(pwm_cap_coolant.duty_cycle * 100.0);
        binocan_dbg_itf_coolant_pack(tx_msg.data, &binocan_dbg_itf_coolant, BINOCAN_DBG_ITF_COOLANT_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue debug coolant message in queue");
        }
    }
}

#endif
#ifdef CONFIG_DBG_ADC_MSG
TaskHandle_t dbg_itf_adc_raw_PKG_hdl;

/// @brief Packaging task for debug ADC raw message
/// @param pvParameters
inline void dbg_itf_adc_raw_PKG(void *pvParameters)
{
    binocan_dbg_itf_adc_raw_t binocan_dbg_itf_adc_raw;
    binocan_dbg_itf_adc_raw_init(&binocan_dbg_itf_adc_raw);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_DBG_ITF_ADC_RAW_FRAME_ID,
        .data_length_code = BINOCAN_DBG_ITF_ADC_RAW_LENGTH};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_DBG_ITF_ADC_RAW_CYCLE_TIME_MS));
        float fuel_raw_v = adc_raw_buffer[0] * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float lv_raw_v = adc_raw_buffer[1] * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        binocan_dbg_itf_adc_raw.dbg_fuel_raw_v = binocan_dbg_itf_adc_raw_dbg_fuel_raw_v_encode(fuel_raw_v);
        binocan_dbg_itf_adc_raw.dbg_12_v_raw_v = binocan_dbg_itf_adc_raw_dbg_12_v_raw_v_encode(lv_raw_v);
        binocan_dbg_itf_adc_raw_pack(tx_msg.data, &binocan_dbg_itf_adc_raw, BINOCAN_DBG_ITF_ADC_RAW_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue debug ADC raw message in queue");
        }
    }
}
#endif
#ifdef CONFIG_DBG_PULSE_COUNTS_MSG
TaskHandle_t dbg_itf_pulse_counts_PKG_hdl;

/// @brief Packaging task for debug pulse counts message
/// @param pvParameters
inline void dbg_itf_pulse_counts_PKG(void *pvParameters)
{
    binocan_dbg_itf_pulse_counts_t binocan_dbg_itf_pulse_counts;
    binocan_dbg_itf_pulse_counts_init(&binocan_dbg_itf_pulse_counts);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_DBG_ITF_PULSE_COUNTS_FRAME_ID,
        .data_length_code = BINOCAN_DBG_ITF_PULSE_COUNTS_LENGTH};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_DBG_ITF_PULSE_COUNTS_CYCLE_TIME_MS));
        binocan_dbg_itf_pulse_counts.dbg_rpm_pulses = binocan_dbg_itf_pulse_counts_dbg_rpm_pulses_encode(pwm_cap_rpm.cumulative_pulse_counter);
        binocan_dbg_itf_pulse_counts.dbg_speed_pulses = binocan_dbg_itf_pulse_counts_dbg_speed_pulses_encode(pwm_cap_speed.cumulative_pulse_counter);
        binocan_dbg_itf_pulse_counts_pack(tx_msg.data, &binocan_dbg_itf_pulse_counts, BINOCAN_DBG_ITF_PULSE_COUNTS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue debug pulse counts message in queue");
        }
    }
}
#endif

/// @brief Packaging task for base_slow_metrics
/// @param pvParameters
inline void itf_slow_metrics_PKG(void *pvParameters)
{
    // SMA-ed slower metrics :
    // coolant_temp
    // fuel_level_pc
    // lv_vol__func__e_v

    binocan_itf_slow_metrics_t binocan_itf_slow_metrics;
    binocan_itf_slow_metrics_init(&binocan_itf_slow_metrics);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_SLOW_METRICS_FRAME_ID,
        .data_length_code = BINOCAN_ITF_SLOW_METRICS_LENGTH};
    esp_err_t compute_err;

    sma_handle_t *fuel_level_SMA = sma_init_full(CONFIG_FUEL_SMA_SIZE, adc_raw_buffer[0]);
    sma_handle_t *lv_vol__func__e_SMA = sma_init_full(CONFIG_LV_SMA_SIZE, adc_raw_buffer[1]);

    while (true)
    {
        // SMAs should already be protected, and some of the MCPWM logic can be brought in here.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_SLOW_METRICS_CYCLE_TIME_MS)); // Waits for notification or one cyclic for frame message
        compute_err = compute_freq_dut(&pwm_cap_coolant);
        // if (compute_err != ESP_OK)
        //     ESP_LOGW(__func__,"Speed DutFreq Compute error : %s",esp_err_to_name(compute_err));
        float coolant_degC = lround((100.0 * pwm_cap_coolant.duty_cycle * COEFF_DUTY_TO_COOLANT_DEGC_M + COEFF_DUTY_TO_COOLANT_DEGC_P) * 10.0) / 10.0;
        if (coolant_degC < 70)
            coolant_degC = 70;
        if (coolant_degC > 130)
            coolant_degC = 130;
        interface_board_st.overTemp = (coolant_degC >= coolant_overtemp_threshold_degC ? true : false);
        // Comment in for debug
        ESP_LOGD(__func__, "Coolant: %.2f - %.2f - %.2f", pwm_cap_coolant.frequency, pwm_cap_coolant.duty_cycle * 100.0, coolant_degC);

        // Collect the SMA values
        sma_add(fuel_level_SMA, adc_raw_buffer[0]);
        sma_add(lv_vol__func__e_SMA, adc_raw_buffer[1]);

        float fuel_level_raw = sma_get_avg(fuel_level_SMA);
        float fuel_level_v = fuel_level_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;

        // Vref (3.3V) validation on ADC channel 3
        int16_t vref_raw = adc_raw_buffer[3];
        float vref_3v3 = (float)vref_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float vref_raw_valid = 0.0f;
        if (vref_3v3 >= (float)COEFF_VREF_MIN_V && vref_3v3 <= (float)COEFF_VREF_MAX_V && vref_raw > 0)
        {
            vref_raw_valid = (float)vref_raw;
        }
        else
        {
            // Fallback to ideal 3.3V equivalent raw count
            vref_raw_valid = (float)(COEFF_VREF_DEFAULT_V * ADS111X_MAX_VALUE / ads111x_gain_values[ADS111X_GAIN_4V096]);
            ESP_LOGW(__func__, "Measured Vref (%.2fV) out of bounds [%.1fV, %.1fV], using fallback %.1fV", vref_3v3, COEFF_VREF_MIN_V, COEFF_VREF_MAX_V, COEFF_VREF_DEFAULT_V);
        }

        // Check active caliber state
        bool is_hi_cal = (gpio_get_level((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO) != 0);
        interface_board_st.EN_hi_R_sense_ST = is_hi_cal;

        double k_factor = is_hi_cal ? COEFF_K_FACTOR_HI_SENSE : COEFF_K_FACTOR_LOW_SENSE;
        float fuel_level_R = (float)(k_factor * fuel_level_raw / vref_raw_valid);

        // Caliber switching hysteresis check
        if (!is_hi_cal && fuel_level_R > (float)COEFF_FUEL_SWITCH_TO_HI_R)
        {
            // Switch from Low to High caliber
            ESP_LOGI(__func__, "Fuel resistance %.1f Ohm > %.1f Ohm, switching to HIGH caliber sensing", fuel_level_R, COEFF_FUEL_SWITCH_TO_HI_R);
            gpio_set_level((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO, 1);
            interface_board_st.EN_hi_R_sense_ST = true;
            is_hi_cal = true;

            // Wait for settling & ADC conversion interval
            vTaskDelay(pdMS_TO_TICKS(conversion_interval_ms + 5));

            // Synchronous fresh sample and SMA filter reseed
            int16_t fresh_raw = adc_sample_channel_threadsafe(0);
            sma_reset(fuel_level_SMA, fresh_raw);
            fuel_level_raw = (float)fresh_raw;

            // Recompute resistance with High Caliber K-factor
            fuel_level_R = (float)(COEFF_K_FACTOR_HI_SENSE * fuel_level_raw / vref_raw_valid);
        }
        else if (is_hi_cal && fuel_level_R < (float)COEFF_FUEL_SWITCH_TO_LOW_R)
        {
            // Switch from High to Low caliber
            ESP_LOGI(__func__, "Fuel resistance %.1f Ohm < %.1f Ohm, switching to LOW caliber sensing", fuel_level_R, COEFF_FUEL_SWITCH_TO_LOW_R);
            gpio_set_level((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO, 0);
            interface_board_st.EN_hi_R_sense_ST = false;
            is_hi_cal = false;

            // Wait for settling & ADC conversion interval
            vTaskDelay(pdMS_TO_TICKS(conversion_interval_ms + 5));

            // Synchronous fresh sample and SMA filter reseed
            int16_t fresh_raw = adc_sample_channel_threadsafe(0);
            sma_reset(fuel_level_SMA, fresh_raw);
            fuel_level_raw = (float)fresh_raw;

            // Recompute resistance with Low Caliber K-factor
            fuel_level_R = (float)(COEFF_K_FACTOR_LOW_SENSE * fuel_level_raw / vref_raw_valid);
        }

        // Open circuit safety check and fuel percentage calculation
        float fuel_level_pc = 0.0f;
        if (is_hi_cal && fuel_level_R > (float)COEFF_FUEL_OC_R)
        {
            fuel_sensor_open_circuit = true;
            fuel_level_pc = 100.0f;
            interface_board_st.lowFuel = true;
            ESP_LOGW(__func__, "Fuel sender open-circuit detected (R = %.1f Ohm > %.1f Ohm)", fuel_level_R, COEFF_FUEL_OC_R);
        }
        else
        {
            fuel_sensor_open_circuit = false;

            // SMA-averaged self-learning of full tank resistance (fuel_full_r)
            if (fuel_learn_en && fuel_level_R > (float)fuel_full_r && fuel_level_R <= (float)(fuel_full_r * COEFF_FUEL_LEARN_MAX_FACTOR))
            {
                uint16_t new_learned_r = (uint16_t)lround(fuel_level_R);
                ESP_LOGI(__func__, "Self-learning: updating fuel_full_r from %u Ohm to %u Ohm", fuel_full_r, new_learned_r);
                fuel_full_r = new_learned_r;

                // Persist to NVS storage namespace
                nvs_handle_t nvs_h;
                if (nvs_open("storage", NVS_READWRITE, &nvs_h) == ESP_OK)
                {
                    if (nvs_set_u16(nvs_h, "fuel_full_r", fuel_full_r) == ESP_OK)
                    {
                        nvs_commit(nvs_h);
                        ESP_LOGI(__func__, "Persisted fuel_full_r (%u Ohm) to NVS", fuel_full_r);
                    }
                    else
                    {
                        ESP_LOGW(__func__, "Failed to persist fuel_full_r to NVS");
                    }
                    nvs_close(nvs_h);
                }
            }

            fuel_level_pc = 100.0f * fuel_level_R / (float)fuel_full_r;
            if (fuel_level_pc > 100.0f)
                fuel_level_pc = 100.0f;
            if (fuel_level_pc < 0.0f)
                fuel_level_pc = 0.0f;

            interface_board_st.lowFuel = (fuel_level_pc < (float)fuel_low_level_threshold_pc);
        }

        float lv_raw = sma_get_avg(lv_vol__func__e_SMA);
        float lv_raw_v = lv_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float lv_v = lround((lv_raw_v * COEFF_V_TO_LV_M + COEFF_V_TO_LV_P) * 10.0) / 10.0;

        // Debug output
        ESP_LOGD(__func__, "Fuel : raw=%.1f vref=%.3fV R=%.1f Ohm (full=%u Ohm, cal=%s) -> %.1fpc | lowFuel=%d OC=%d\t|\t12V: %.2fV",
                 fuel_level_raw, vref_3v3, fuel_level_R, fuel_full_r, is_hi_cal ? "HIGH" : "LOW", fuel_level_pc,
                 interface_board_st.lowFuel, fuel_sensor_open_circuit, lv_v);

        binocan_itf_slow_metrics.itf_coolant_temp = binocan_itf_slow_metrics_itf_coolant_temp_encode(coolant_degC);
        binocan_itf_slow_metrics.itf_fuel_level_pc = binocan_itf_slow_metrics_itf_fuel_level_pc_encode(fuel_level_pc);
        binocan_itf_slow_metrics.itf_lv_voltage_v = binocan_itf_slow_metrics_itf_lv_voltage_v_encode(lv_v);
        binocan_itf_slow_metrics_pack(tx_msg.data, &binocan_itf_slow_metrics, BINOCAN_ITF_SLOW_METRICS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue slow metrics message in queue");
        }
    }
}

/// @brief Packaging task for base_fast_metrics
/// @param pvParameters
inline void itf_fast_metrics_PKG(void *pvParameters)
{
    // Only faster metrics
    // speed_kph
    // rpm
    esp_err_t compute_err;
    binocan_itf_fast_metrics_t binocan_itf_fast_metrics;
    binocan_itf_fast_metrics_init(&binocan_itf_fast_metrics);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_FAST_METRICS_FRAME_ID,
        .data_length_code = BINOCAN_ITF_FAST_METRICS_LENGTH};

    while (true)
    {
        // Transport some of the MCPWM logic in there
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_FAST_METRICS_CYCLE_TIME_MS));
        compute_err = compute_freq_dut(&pwm_cap_rpm);
        // if (compute_err != ESP_OK)
        //     ESP_LOGW(__func__,"RPM DutFreq Compute error : %s",esp_err_to_name(compute_err));
        compute_err = compute_freq_dut(&pwm_cap_speed);
        // if (compute_err != ESP_OK)
        //     ESP_LOGW(__func__,"Speed DutFreq Compute error : %s",esp_err_to_name(compute_err));
        float rpm = COEFF_FREQ_TO_RPM_M * pwm_cap_rpm.frequency + COEFF_FREQ_TO_RPM_P;
        float speed = COEFF_FREQ_TO_SPEED_KPH_M * pwm_cap_speed.frequency + COEFF_FREQ_TO_SPEED_KPH_P;
        ESP_LOGD(__func__, "RPM : %.2f - %.1f - %.2f Speed: %.2f - %.1f - %.2f", pwm_cap_rpm.frequency, pwm_cap_rpm.duty_cycle, rpm, pwm_cap_speed.frequency, pwm_cap_speed.duty_cycle, speed);

        binocan_itf_fast_metrics.itf_rpm = binocan_itf_fast_metrics_itf_rpm_encode(rpm);
        binocan_itf_fast_metrics.itf_speed_kph = binocan_itf_fast_metrics_itf_speed_kph_encode(speed);
        // Placeholder for the gear position, for now always showing Neutral
        binocan_itf_fast_metrics.itf_gear_position_st = binocan_itf_fast_metrics_itf_gear_position_st_encode(BINOCAN_ITF_FAST_METRICS_ITF_GEAR_POSITION_ST_NEUTRAL_CHOICE);
        binocan_itf_fast_metrics_pack(tx_msg.data, &binocan_itf_fast_metrics, BINOCAN_ITF_FAST_METRICS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue fast metrics message in queue");
        }
    }
}

/// @brief Packaging task for base_active_hi_lo
/// @param pvParameters
inline void itf_active_hilo_PKG(void *pvParameters)
{
    // Will need to be using notifications and delay
    // Essentially direct from expander + a couple virtual telltales
    uint16_t raw;

    binocan_itf_active_hi_lo_t binocan_itf_active_hi_lo;
    binocan_itf_active_hi_lo_init(&binocan_itf_active_hi_lo);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_ACTIVE_HI_LO_FRAME_ID,
        .data_length_code = BINOCAN_ITF_ACTIVE_HI_LO_LENGTH};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_ACTIVE_HI_LO_CYCLE_TIME_MS));
        if (tca95x5_port_read(&tca_slave, &raw) != ESP_OK)
        {
            ESP_LOGE(__func__, "Impossible to fetch register from expander");
            interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
            ESP_LOGW(__func__, "Entering degraded mode at line %lu", __LINE__);
            interface_board_st.expander_ST = false;
        }
        else
        {
            interface_board_st.expander_ST = true;
            if (xSemaphoreTake(exp_act_hilo_semaphore, pdMS_TO_TICKS(1)) == pdTRUE)
            {
                active_hi_lo_grp.AL_brake_low = read_bitmask(raw, EXP_IO_0_BITMASK);
                active_hi_lo_grp.AL_parking_brake = read_bitmask(raw, EXP_IO_1_BITMASK);
                active_hi_lo_grp.AH_ignition = !read_bitmask(raw, EXP_IO_2_BITMASK); // Inverted
                active_hi_lo_grp.AL_oil_pressure = read_bitmask(raw, EXP_IO_3_BITMASK);
                active_hi_lo_grp.AL_airbag = read_bitmask(raw, EXP_IO_4_BITMASK);
                active_hi_lo_grp.AL_CEL = read_bitmask(raw, EXP_IO_5_BITMASK);
                active_hi_lo_grp.AH_backlight = !read_bitmask(raw, EXP_IO_6_BITMASK); // Inverted
                active_hi_lo_grp.AH_alarm = !read_bitmask(raw, EXP_IO_7_BITMASK);     // Inverted
                active_hi_lo_grp.AH_hi_beams = !read_bitmask(raw, EXP_IO_8_BITMASK);  // Inverted
                active_hi_lo_grp.AL_button = read_bitmask(raw, EXP_IO_9_BITMASK);
                active_hi_lo_grp.AL_alternator = read_bitmask(raw, EXP_IO_10_BITMASK);
                active_hi_lo_grp.AH_coolant_low = !read_bitmask(raw, EXP_IO_11_BITMASK); // Inverted
                active_hi_lo_grp.AH_left_turn = !read_bitmask(raw, EXP_IO_12_BITMASK);   // Inverted
                active_hi_lo_grp.AL_door = read_bitmask(raw, EXP_IO_13_BITMASK);
                active_hi_lo_grp.AH_right_turn = !read_bitmask(raw, EXP_IO_14_BITMASK); // Inverted
                active_hi_lo_grp.AL_ABS = read_bitmask(raw, EXP_IO_15_BITMASK);

                binocan_itf_active_hi_lo.itf_ignition_ah_st = binocan_itf_active_hi_lo_itf_ignition_ah_st_encode(active_hi_lo_grp.AH_ignition);
                binocan_itf_active_hi_lo.itf_hi_beams_ah_tt = binocan_itf_active_hi_lo_itf_hi_beams_ah_tt_encode(active_hi_lo_grp.AH_hi_beams);
                binocan_itf_active_hi_lo.itf_alternator_al_tt = binocan_itf_active_hi_lo_itf_alternator_al_tt_encode(active_hi_lo_grp.AL_alternator);
                binocan_itf_active_hi_lo.itf_brake_low_al_tt = binocan_itf_active_hi_lo_itf_brake_low_al_tt_encode(active_hi_lo_grp.AL_brake_low);
                binocan_itf_active_hi_lo.itf_parking_brake_al_tt = binocan_itf_active_hi_lo_itf_parking_brake_al_tt_encode(active_hi_lo_grp.AL_parking_brake);
                binocan_itf_active_hi_lo.itf_oil_pressure_al_tt = binocan_itf_active_hi_lo_itf_oil_pressure_al_tt_encode(active_hi_lo_grp.AL_oil_pressure);
                binocan_itf_active_hi_lo.itf_airbag_al_tt = binocan_itf_active_hi_lo_itf_airbag_al_tt_encode(active_hi_lo_grp.AL_airbag);
                binocan_itf_active_hi_lo.itf_cel_al_tt = binocan_itf_active_hi_lo_itf_cel_al_tt_encode(active_hi_lo_grp.AL_CEL);
                binocan_itf_active_hi_lo.itf_right_turn_ah_tt = binocan_itf_active_hi_lo_itf_right_turn_ah_tt_encode(active_hi_lo_grp.AH_right_turn);
                binocan_itf_active_hi_lo.itf_left_turn_ah_tt = binocan_itf_active_hi_lo_itf_left_turn_ah_tt_encode(active_hi_lo_grp.AH_left_turn);
                binocan_itf_active_hi_lo.itf_abs_al_tt = binocan_itf_active_hi_lo_itf_abs_al_tt_encode(active_hi_lo_grp.AL_ABS);
                binocan_itf_active_hi_lo.itf_door_al_tt = binocan_itf_active_hi_lo_itf_door_al_tt_encode(active_hi_lo_grp.AL_door);
                binocan_itf_active_hi_lo.itf_coolant_low_ah_tt = binocan_itf_active_hi_lo_itf_coolant_low_ah_tt_encode(active_hi_lo_grp.AH_coolant_low);
                binocan_itf_active_hi_lo.itf_button_al = binocan_itf_active_hi_lo_itf_button_al_encode(active_hi_lo_grp.AL_button);
                binocan_itf_active_hi_lo.itf_alarm_ah = binocan_itf_active_hi_lo_itf_alarm_ah_encode(active_hi_lo_grp.AH_alarm);
                binocan_itf_active_hi_lo.itf_backlight_ah = binocan_itf_active_hi_lo_itf_backlight_ah_encode(active_hi_lo_grp.AH_backlight);
                // Virtual tell tales
                binocan_itf_active_hi_lo.itf_over_temperature_tt = binocan_itf_active_hi_lo_itf_over_temperature_tt_encode(interface_board_st.overTemp);
                binocan_itf_active_hi_lo.itf_fuel_low_tt = binocan_itf_active_hi_lo_itf_fuel_low_tt_encode(interface_board_st.lowFuel);
                binocan_itf_active_hi_lo_pack(tx_msg.data, &binocan_itf_active_hi_lo, BINOCAN_ITF_ACTIVE_HI_LO_LENGTH);

                if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
                {
                    ESP_LOGW(__func__, "Could not queue active hi/lo message in queue");
                }
                xSemaphoreGive(exp_act_hilo_semaphore);
            }
        }
    }
}

/// @brief Packaging task for the odometer and trip
/// @param pvParameters
inline void itf_odometer_PKG(void *pvParameters)
{
    binocan_itf_odometer_t binocan_itf_odometer;
    binocan_itf_odometer_init(&binocan_itf_odometer);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_ODOMETER_FRAME_ID,
        .data_length_code = BINOCAN_ITF_ODOMETER_LENGTH};

    float pulse_m = 0;

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_ODOMETER_CYCLE_TIME_MS));
        // Check if number of pulses exceed 100m, if yes update trip_m and odometer_m to their new values, and propagate to NVS and CAN, if not just repeat values over CAN and do nothing on NVS side
        pulse_m = COEFF_PULSES_TO_METER * pwm_cap_speed.pulse_counter;

        if (pulse_m >= 100.0)
        {
            pwm_cap_speed.pulse_counter = 0;
            odometer_m += lround(pulse_m);
            trip_m += lround(pulse_m);
            if (odometer_set(odometer_m) != ESP_OK)
            {
                ESP_LOGW(__func__, "Could not set odometer_m in NVS");
                interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
                ESP_LOGW(__func__, "Entering degraded mode at line %lu", __LINE__);
            }
            if (trip_set(trip_m) != ESP_OK)
            {
                ESP_LOGW(__func__, "Could not set trip_m in NVS");
                interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
                ESP_LOGW(__func__, "Entering degraded mode at line %lu", __LINE__);
            }
        }
        binocan_itf_odometer.itf_odometer_km = binocan_itf_odometer_itf_odometer_km_encode((float)(odometer_m / 1000));
        binocan_itf_odometer.itf_odo_rem_m = binocan_itf_odometer_itf_odo_rem_m_encode((float)(odometer_m % 1000));
        binocan_itf_odometer.itf_trip_km = binocan_itf_odometer_itf_trip_km_encode((float)(trip_m / 1000));
        binocan_itf_odometer.itf_trip_rem_m = binocan_itf_odometer_itf_trip_rem_m_encode((float)(trip_m % 1000));
        binocan_itf_odometer_pack(tx_msg.data, &binocan_itf_odometer, BINOCAN_ITF_ODOMETER_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue odometer message in queue");
        }
    }
}

/// @brief Gathering and packaging task for the internal state indicators
/// @param pvParameters
void itf_board_st_PKG(void *pvParameters)
{
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensor_enable(temp_sensor));

    bool ads1115_busy = false;

    binocan_itf_board_st_t binocan_itf_board_st;
    binocan_itf_board_st_init(&binocan_itf_board_st);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_BOARD_ST_FRAME_ID,
        .data_length_code = BINOCAN_ITF_BOARD_ST_LENGTH};

    gpio_set_direction((gpio_num_t)CONFIG_LD_ALIVE_IO, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_LD_ALIVE_IO, GPIO_PULLDOWN_ONLY);
    gpio_pulldown_en((gpio_num_t)CONFIG_LD_ALIVE_IO);

    gpio_set_direction((gpio_num_t)CONFIG_RD_ALIVE_IO, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_RD_ALIVE_IO, GPIO_PULLDOWN_ONLY);
    gpio_pulldown_en((gpio_num_t)CONFIG_RD_ALIVE_IO);

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_BOARD_ST_CYCLE_TIME_MS));

        interface_board_st.LDB_check_alive_ST = gpio_get_level((gpio_num_t)CONFIG_LD_ALIVE_IO);
        interface_board_st.RDB_check_alive_ST = gpio_get_level((gpio_num_t)CONFIG_RD_ALIVE_IO);
        interface_board_st.EN_5_V_AUX_ST = gpio_get_level((gpio_num_t)CONFIG_5V_AUX_EN_GPIO);
        interface_board_st.EN_5_V_ST = gpio_get_level((gpio_num_t)CONFIG_5V_EN_GPIO);
        interface_board_st.EN_hi_R_sense_ST = gpio_get_level((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO);
        ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensor_get_celsius(temp_sensor, &(interface_board_st.mcu_temperature)));
        if (ads111x_is_busy(&adc_slave, &ads1115_busy) != ESP_OK)
        {
            interface_board_st.adc_ST = false;
        }
        else
            interface_board_st.adc_ST = true;

        binocan_itf_board_st.itf_hi_r_sense_st = binocan_itf_board_st_itf_hi_r_sense_st_encode(interface_board_st.EN_hi_R_sense_ST);
        binocan_itf_board_st.itf_5_v_aux_st = binocan_itf_board_st_itf_5_v_aux_st_encode(interface_board_st.EN_5_V_AUX_ST);
        binocan_itf_board_st.itf_5_v_st = binocan_itf_board_st_itf_5_v_st_encode(interface_board_st.EN_5_V_ST);
        binocan_itf_board_st.itf_sm_st = binocan_itf_board_st_itf_sm_st_encode(interface_board_st.internal_ST);
        binocan_itf_board_st.itf_ld_check_alive_st = binocan_itf_board_st_itf_ld_check_alive_st_encode(interface_board_st.LDB_check_alive_ST);
        binocan_itf_board_st.itf_rd_check_alive_st = binocan_itf_board_st_itf_rd_check_alive_st_encode(interface_board_st.RDB_check_alive_ST);
        binocan_itf_board_st.itf_mcu_temp = binocan_itf_board_st_itf_mcu_temp_encode(interface_board_st.mcu_temperature);
        binocan_itf_board_st.itf_adc_st = binocan_itf_board_st_itf_adc_st_encode(interface_board_st.adc_ST);
        binocan_itf_board_st.itf_expander_st = binocan_itf_board_st_itf_expander_st_encode(interface_board_st.expander_ST);
        binocan_itf_board_st_pack(tx_msg.data, &binocan_itf_board_st, BINOCAN_ITF_BOARD_ST_LENGTH);

        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue internal state message in queue");
        }
    }
}

/// @brief Generates the commit and version information and regularly broadcasts them on the bus
/// @param pvParameters
inline void itf_board_version_PKG(void *pvParameters)
{
    uint8_t message_mux = 0;
    binocan_itf_board_version_t binocan_itf_board_version;
    binocan_itf_board_version_init(&binocan_itf_board_version);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_BOARD_VERSION_FRAME_ID,
        .data_length_code = BINOCAN_ITF_BOARD_VERSION_LENGTH};

    binocan_itf_board_version.itf_version_major = binocan_itf_board_version_itf_version_major_encode((uint8_t)(interface_board_st.app_metadata->base_version[0]) - 48);
    binocan_itf_board_version.itf_version_minor = binocan_itf_board_version_itf_version_minor_encode((uint8_t)(interface_board_st.app_metadata->base_version[2]) - 48);
    binocan_itf_board_version.itf_version_patch = binocan_itf_board_version_itf_version_patch_encode((uint8_t)(interface_board_st.app_metadata->base_version[4]) - 48);
    binocan_itf_board_version.itf_version_dirty = binocan_itf_board_version_itf_version_dirty_encode((uint8_t)interface_board_st.app_metadata->is_dirty);
    // Build a 64-bit value from up to 8 bytes of commitID and pass to the encode helper.
    // This avoids memcpy into a numeric field and is lightweight.
    if (interface_board_st.app_metadata && interface_board_st.app_metadata->commitID)
    {
        // ESP_LOGI(__func__,"Commit is valid");
        uint64_t commit_val = 0;
        size_t src_len = interface_board_st.app_metadata->commit_len;
        if (src_len > 8)
            src_len = 8;
        for (size_t i = 0; i < src_len; ++i)
        {
            commit_val = (commit_val << 8) | (uint8_t)interface_board_st.app_metadata->commitID[src_len - i - 1];
        }
        binocan_itf_board_version.itf_version_commit = binocan_itf_board_version_itf_version_commit_encode(commit_val);
    }
    else
    {
        // ESP_LOGI(__func__,"Commit will be 0");
        binocan_itf_board_version.itf_version_commit = binocan_itf_board_version_itf_version_commit_encode(0ULL);
    }

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_BOARD_VERSION_CYCLE_TIME_MS));
        // Switch the mux indicator
        message_mux = (message_mux + 1) % 2; // Currently only two values to the MUX
        binocan_itf_board_version.itf_version_mux = binocan_itf_board_version_itf_version_mux_encode(message_mux);
        binocan_itf_board_version_pack(tx_msg.data, &binocan_itf_board_version, BINOCAN_ITF_BOARD_VERSION_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(__func__, "Could not queue internal state message in queue");
        }
    }
}

#pragma endregion

#pragma region Initialization

/// @brief Initialiser for all TWAI operations.
/// @return ESP_FAIL if one of the key tasks fails to be created, ESP_OK otherwise.
inline esp_err_t twai_ops_init()
{
    esp_err_t ret = ESP_OK;

    // Set up the packaging and queuing tasks
    if (xTaskCreatePinnedToCore(itf_board_st_PKG, "ITF_ST", 4096, NULL, 3, &itf_board_st_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create interface state package task");
        ret = ESP_FAIL;
    }
    if (xTaskCreatePinnedToCore(itf_active_hilo_PKG, "ITF_AHL", 4096, NULL, 3, &exp_act_hilo_proc_task_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create base ActHiLo package task");
        ret = ESP_FAIL;
    }
    if (xTaskCreatePinnedToCore(itf_slow_metrics_PKG, "ITF_SLO_M", 4096+2048, NULL, 3, &itf_slow_metrics_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create base slow metrics package task");
        ret = ESP_FAIL;
    }
    if (xTaskCreatePinnedToCore(itf_fast_metrics_PKG, "ITF_FST_M", 4096, NULL, 3, &itf_fast_metrics_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create base fast metrics package task");
        ret = ESP_FAIL;
    }
    if (xTaskCreatePinnedToCore(itf_odometer_PKG, "ITF_ODO", 4096, NULL, 3, &itf_odometer_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create base odometer package task");
        ret = ESP_FAIL;
    }
    if (xTaskCreatePinnedToCore(itf_board_version_PKG, "ITF_VER", 4096, NULL, 3, &itf_board_version_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create interface board version package task");
        ret = ESP_FAIL;
    }

// Debug message packaging and queuing tasks
#ifdef CONFIG_DBG_SPEED_MSG
    if (xTaskCreatePinnedToCore(dbg_itf_speed_PKG, "DBG_ITF_SPD", 4096, NULL, 3, &dbg_itf_speed_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create debug speed package task");
    }
#endif

#ifdef CONFIG_DBG_RPM_MSG
    if (xTaskCreatePinnedToCore(dbg_itf_rpm_PKG, "DBG_ITF_RPM", 4096, NULL, 3, &dbg_itf_rpm_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create debug RPM package task");
    }
#endif

#ifdef CONFIG_DBG_COOL_MSG
    if (xTaskCreatePinnedToCore(dbg_itf_coolant_PKG, "DBG_ITF_COOL", 4096, NULL, 3, &dbg_itf_coolant_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create debug coolant package task");
    }
#endif

#ifdef CONFIG_DBG_ADC_MSG
    if (xTaskCreatePinnedToCore(dbg_itf_adc_raw_PKG, "DBG_ITF_ADC", 4096, NULL, 3, &dbg_itf_adc_raw_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create debug ADC package task");
    }
#endif

#ifdef CONFIG_DBG_PULSE_COUNTS_MSG
    if (xTaskCreatePinnedToCore(dbg_itf_pulse_counts_PKG, "DBG_ITF_PLS", 4096, NULL, 3, &dbg_itf_pulse_counts_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create debug pulse counts package task");
    }
#endif

    return ret;
}

#pragma endregion