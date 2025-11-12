#include <stdio.h>
// #include "gpio_defs.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/mcpwm_prelude.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mcpwm_capture_helpers.h"
#include "coefficients.h"

#include "twai_daemon.h"
#include "binocan.h"

#include "adc_processor.h"
#include "active_hi_low_processor.h"
#include "sma_filter.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "MAIN"

// PWM stats structures for coolant, rpm and speed captures
static volatile pwm_info_t pwm_cap_coolant, pwm_cap_rpm, pwm_cap_speed = {.pos_edge_ts = 0, .prev_pos_edge_ts = 0, .period_ticks = 0, .neg_edge_ts = 0, .deltaT = 0};

// Capture channels
mcpwm_cap_channel_handle_t cap_chan_coolant = NULL;
mcpwm_cap_channel_handle_t cap_chan_rpm = NULL;
mcpwm_cap_channel_handle_t cap_chan_speed = NULL;

// FreeRTOS handles
TaskHandle_t base_slow_metrics_PKG_hdl;
TaskHandle_t base_fast_metrics_PKG_hdl;

/// @brief Packaging task for base_slow_metrics
/// @param pvParameters
void base_slow_metrics_PKG(void *pvParameters)
{
    // SMA-ed slower metrics :
    // coolant_temp
    // fuel_level_pc
    // lv_voltage_v

    binocan_base_slow_metrics_t binocan_base_slow_metrics;
    binocan_base_slow_metrics_init(&binocan_base_slow_metrics);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_BASE_SLOW_METRICS_FRAME_ID,
        .data_length_code = BINOCAN_BASE_SLOW_METRICS_LENGTH};

    while (true)
    {
        // SMAs should already be protected, and some of the MCPWM logic can be brought in here.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_SLOW_METRICS_PKG_RATE_MS));
        compute_freq_dut(&pwm_cap_coolant);
        float coolant_degC = 100.0 * pwm_cap_coolant.duty_cycle * COEFF_DUTY_TO_COOLANT_DEGC_M + COEFF_DUTY_TO_COOLANT_DEGC_P;
        if (coolant_degC < 70)
            coolant_degC = 70;
        if (coolant_degC > 130)
            coolant_degC = 130;
        ESP_LOGI(TAG, "Coolant: %.2f - %.1f - %.2f", pwm_cap_coolant.frequency, pwm_cap_coolant.duty_cycle, coolant_degC);

        float fuel_level_raw = sma_get_avg(adc_channels[0].sma);
        float fuel_level_v = fuel_level_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float fuel_level_pc = COEFF_FUEL_V_TO_PC_M * fuel_level_v + COEFF_FUEL_V_TO_PC_P;
        if (fuel_level_pc > 100.0)
            fuel_level_pc = 100;
        if (fuel_level_pc < 0)
            fuel_level_pc = 0;

        float lv_raw = sma_get_avg(adc_channels[1].sma);
        float lv_raw_v = lv_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float lv_v = lv_raw_v * COEFF_V_TO_LV_M + COEFF_V_TO_LV_P;

        ESP_LOGI(TAG, "Fuel : %.2f - %.2fV - %.2fpc\t|\t 12V: %.2f - %.2fV - %.2fV", fuel_level_raw, fuel_level_v, fuel_level_pc, lv_raw, lv_raw_v, lv_v);

        binocan_base_slow_metrics.coolant_temp = binocan_base_slow_metrics_coolant_temp_encode(coolant_degC);
        binocan_base_slow_metrics.fuel_level_pc = binocan_base_slow_metrics_fuel_level_pc_encode(fuel_level_pc);
        binocan_base_slow_metrics.lv_voltage_v = binocan_base_slow_metrics_lv_voltage_v_encode(lv_v);
        binocan_base_slow_metrics_pack(tx_msg.data, &binocan_base_slow_metrics, BINOCAN_BASE_SLOW_METRICS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue slow metrics message in queue");
        }
    }
}

/// @brief Packaging task for base_fast_metrics
/// @param pvParameters
void base_fast_metrics_PKG(void *pvParameters)
{
    // Only faster metrics
    // speed_kph
    // rpm

    binocan_base_fast_metrics_t binocan_base_fast_metrics;
    binocan_base_fast_metrics_init(&binocan_base_fast_metrics);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_BASE_FAST_METRICS_FRAME_ID,
        .data_length_code = BINOCAN_BASE_FAST_METRICS_LENGTH};

    while (true)
    {
        // Transport some of the MCPWM logic in there
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_FAST_METRICS_PKG_RATE_MS));
        compute_freq_dut(&pwm_cap_rpm);
        compute_freq_dut(&pwm_cap_speed);
        float rpm = COEFF_FREQ_TO_RPM_M * pwm_cap_rpm.frequency + COEFF_FREQ_TO_RPM_P;
        float speed = COEFF_FREQ_TO_SPEED_KPH_M * pwm_cap_speed.frequency + COEFF_FREQ_TO_SPEED_KPH_P;
        ESP_LOGI(TAG, "RPM : %.2f - %.1f - %.2f Speed: %.2f - %.1f - %.2f", pwm_cap_rpm.frequency, pwm_cap_rpm.duty_cycle, rpm, pwm_cap_speed.frequency, pwm_cap_speed.duty_cycle, speed);

        binocan_base_fast_metrics.rpm = binocan_base_fast_metrics_rpm_encode(rpm);
        binocan_base_fast_metrics.speed_kph = binocan_base_fast_metrics_speed_kph_encode(speed);
        binocan_base_fast_metrics_pack(tx_msg.data, &binocan_base_fast_metrics, BINOCAN_BASE_FAST_METRICS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue fast metrics message in queue");
        }
    }
}

/// @brief Packaging task for base_active_hi_lo
/// @param pvParameters
void base_active_hilo_PKG(void *pvParameters)
{
    // Will need to be using notifications and delay
    // Essentially direct from expander + a couple virtual telltales
    uint16_t raw;

    binocan_base_active_hi_lo_t binocan_base_active_hi_lo;
    binocan_base_active_hi_lo_init(&binocan_base_active_hi_lo);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_BASE_ACTIVE_HI_LO_FRAME_ID,
        .data_length_code = BINOCAN_BASE_ACTIVE_HI_LO_LENGTH};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_ACTIVE_HILO_PKG_RATE_MS));
        if (tca95x5_port_read(&tca_slave, &raw) != ESP_OK)
        {
            ESP_LOGE(TAG, "Impossible to fetch register from expander");
        }
        else
        {

            if (xSemaphoreTake(exp_act_hilo_semaphore, pdMS_TO_TICKS(1)) == pdTRUE)
            {
                active_hi_lo_grp.AH_ignition = read_bitmask(raw, EXP_IO_0_BITMASK);
                active_hi_lo_grp.AH_hi_beams = read_bitmask(raw, EXP_IO_1_BITMASK);
                active_hi_lo_grp.AL_alternator = read_bitmask(raw, EXP_IO_2_BITMASK);
                active_hi_lo_grp.AL_brake_low = read_bitmask(raw, EXP_IO_3_BITMASK);
                active_hi_lo_grp.AL_parking_brake = read_bitmask(raw, EXP_IO_4_BITMASK);
                active_hi_lo_grp.AL_oil_pressure = read_bitmask(raw, EXP_IO_5_BITMASK);
                active_hi_lo_grp.AL_airbag = read_bitmask(raw, EXP_IO_6_BITMASK);
                active_hi_lo_grp.AL_CEL = read_bitmask(raw, EXP_IO_7_BITMASK);
                active_hi_lo_grp.AH_right_turn = read_bitmask(raw, EXP_IO_8_BITMASK);
                active_hi_lo_grp.AH_left_turn = read_bitmask(raw, EXP_IO_9_BITMASK);
                active_hi_lo_grp.AL_ABS = read_bitmask(raw, EXP_IO_10_BITMASK);
                active_hi_lo_grp.AL_door = read_bitmask(raw, EXP_IO_11_BITMASK);
                active_hi_lo_grp.AL_coolant_low = read_bitmask(raw, EXP_IO_12_BITMASK);
                active_hi_lo_grp.AL_button = read_bitmask(raw, EXP_IO_13_BITMASK);
                active_hi_lo_grp.AH_B07 = read_bitmask(raw, EXP_IO_14_BITMASK);
                active_hi_lo_grp.AH_backlight = read_bitmask(raw, EXP_IO_15_BITMASK);

                binocan_base_active_hi_lo.ignition_ah_st = binocan_base_active_hi_lo_ignition_ah_st_encode(active_hi_lo_grp.AH_ignition);
                binocan_base_active_hi_lo.hi_beams_ah_tt = binocan_base_active_hi_lo_hi_beams_ah_tt_encode(active_hi_lo_grp.AH_hi_beams);
                binocan_base_active_hi_lo.alternator_al_tt = binocan_base_active_hi_lo_alternator_al_tt_encode(active_hi_lo_grp.AL_alternator);
                binocan_base_active_hi_lo.brake_low_al_tt = binocan_base_active_hi_lo_brake_low_al_tt_encode(active_hi_lo_grp.AL_brake_low);
                binocan_base_active_hi_lo.parking_brake_al_tt = binocan_base_active_hi_lo_parking_brake_al_tt_encode(active_hi_lo_grp.AL_parking_brake);
                binocan_base_active_hi_lo.oil_pressure_al_tt = binocan_base_active_hi_lo_oil_pressure_al_tt_encode(active_hi_lo_grp.AL_oil_pressure);
                binocan_base_active_hi_lo.airbag_al_tt = binocan_base_active_hi_lo_airbag_al_tt_encode(active_hi_lo_grp.AL_airbag);
                binocan_base_active_hi_lo.cel_al_tt = binocan_base_active_hi_lo_cel_al_tt_encode(active_hi_lo_grp.AL_CEL);
                binocan_base_active_hi_lo.right_turn_ah_tt = binocan_base_active_hi_lo_right_turn_ah_tt_encode(active_hi_lo_grp.AH_right_turn);
                binocan_base_active_hi_lo.left_turn_ah_tt = binocan_base_active_hi_lo_left_turn_ah_tt_encode(active_hi_lo_grp.AH_left_turn);
                binocan_base_active_hi_lo.abs_al_tt = binocan_base_active_hi_lo_abs_al_tt_encode(active_hi_lo_grp.AL_ABS);
                binocan_base_active_hi_lo.door_al_tt = binocan_base_active_hi_lo_door_al_tt_encode(active_hi_lo_grp.AL_door);
                binocan_base_active_hi_lo.coolant_low_al_tt = binocan_base_active_hi_lo_coolant_low_al_tt_encode(active_hi_lo_grp.AL_coolant_low);
                binocan_base_active_hi_lo.button_al = binocan_base_active_hi_lo_button_al_encode(active_hi_lo_grp.AL_button);
                binocan_base_active_hi_lo.b07_ah = binocan_base_active_hi_lo_b07_ah_encode(active_hi_lo_grp.AH_B07);
                binocan_base_active_hi_lo.backlight_ah = binocan_base_active_hi_lo_backlight_ah_encode(active_hi_lo_grp.AH_backlight);
                // Virtual tell tales
                binocan_base_active_hi_lo.over_temperature_tt = binocan_base_active_hi_lo_over_temperature_tt_encode(0); // Placeholder, no sensor
                binocan_base_active_hi_lo.fuel_low_tt = binocan_base_active_hi_lo_fuel_low_tt_encode(0);                 // Placeholder, no sensor
                binocan_base_active_hi_lo_pack(tx_msg.data, &binocan_base_active_hi_lo, BINOCAN_BASE_ACTIVE_HI_LO_LENGTH);

                if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
                {
                    ESP_LOGW(TAG, "Could not queue active hi/lo message in queue");
                }

                // Normally here we package to the CAN queue
                ESP_LOGI(TAG, "Ignition: %s", active_hi_lo_grp.AH_ignition ? "ON" : "OFF");
                ESP_LOGI(TAG, "Hi beams: %s", active_hi_lo_grp.AH_hi_beams ? "ON" : "OFF");
                ESP_LOGI(TAG, "Alternator: %s", active_hi_lo_grp.AL_alternator ? "ON" : "OFF");
                ESP_LOGI(TAG, "Brake low level: %s", active_hi_lo_grp.AL_brake_low ? "ON" : "OFF");
                ESP_LOGI(TAG, "Parking brake: %s", active_hi_lo_grp.AL_parking_brake ? "ON" : "OFF");
                ESP_LOGI(TAG, "Oil alarm: %s", active_hi_lo_grp.AL_oil_pressure ? "ON" : "OFF");
                ESP_LOGI(TAG, "Airbag: %s", active_hi_lo_grp.AL_airbag ? "ON" : "OFF");
                ESP_LOGI(TAG, "CEL: %s", active_hi_lo_grp.AL_CEL ? "ON" : "OFF");
                ESP_LOGI(TAG, "Right turn: %s", active_hi_lo_grp.AH_right_turn ? "ON" : "OFF");
                ESP_LOGI(TAG, "Left turn: %s", active_hi_lo_grp.AH_left_turn ? "ON" : "OFF");
                ESP_LOGI(TAG, "ABS: %s", active_hi_lo_grp.AL_ABS ? "ON" : "OFF");
                ESP_LOGI(TAG, "Door: %s", active_hi_lo_grp.AL_door ? "ON" : "OFF");
                ESP_LOGI(TAG, "Low coolant: %s", active_hi_lo_grp.AL_coolant_low ? "ON" : "OFF");
                ESP_LOGI(TAG, "Button: %s", active_hi_lo_grp.AL_button ? "ON" : "OFF");
                ESP_LOGI(TAG, "B07: %s", active_hi_lo_grp.AH_B07 ? "ON" : "OFF");
                ESP_LOGI(TAG, "Backlight: %s", active_hi_lo_grp.AH_backlight ? "ON" : "OFF");
                xSemaphoreGive(exp_act_hilo_semaphore);
            }
        }
    }
}

extern "C" void app_main(void)
{
    // Start TWAI first
    if (initCAN(NULL) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialize TWAI daemon, quitting...");
        return;
    }

    // Set up the capture channels
    if (set_capture_channel(cap_chan_coolant, (gpio_num_t)CONFIG_COOLANT_PWM_CAP_GPIO, &pwm_cap_coolant) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set Coolant capture channel");
        return;
    }
    if (set_capture_channel(cap_chan_rpm, (gpio_num_t)CONFIG_RPM_PWM_CAP_GPIO, &pwm_cap_rpm) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set RPM capture channel");
        return;
    }
    if (set_capture_channel(cap_chan_speed, (gpio_num_t)CONFIG_SPEED_PWM_CAP_GPIO, &pwm_cap_speed) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set Speed capture channel");
        return;
    }

    // Set up the IO Expander

    if (i2cdev_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start I²C bus");
        return;
    }
    if (initialize_adc_processor() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start ADC processor");
        return;
    }
    if (initialize_exp_active_hi_lo_proc() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start ActHiLo processor");
        return;
    }

    // Set up the packaging and queuing tasks
    if (xTaskCreate(base_active_hilo_PKG, "B_AHL_PKG", 4096, NULL, 3, &exp_act_hilo_proc_task_hdl) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base AHL package task");
        return;
    }
    if (xTaskCreate(base_slow_metrics_PKG, "B_SLO_M_PKG", 4096, NULL, 3, &base_slow_metrics_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base slow metrics package task");
        return;
    }
    if (xTaskCreate(base_fast_metrics_PKG, "B_FST_M_PKG", 4096, NULL, 3, &base_fast_metrics_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base fast metrics package task");
        return;
    }

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        // Only used to log MCPWM output
/* #ifdef CONFIG_LOOP_LOG_MCPWM
        vTaskDelay(pdMS_TO_TICKS(LOG_INTERVAL_MS));
        compute_freq_dut(&pwm_cap_coolant);
        compute_freq_dut(&pwm_cap_rpm);
        compute_freq_dut(&pwm_cap_speed);
        ESP_LOGI(TAG, "Coolant:\t %.2f Hz, %.1f%% \t|\tRPM:\t %.2f Hz, %.1f%% \t|\tSpeed:\t %.2f Hz, %.1f%%",
                 pwm_cap_coolant.frequency, pwm_cap_coolant.duty_cycle * 100.0,
                 pwm_cap_rpm.frequency, pwm_cap_rpm.duty_cycle * 100.0,
                 pwm_cap_speed.frequency, pwm_cap_speed.duty_cycle * 100.0);

        pwm_cap_coolant.deltaT = 0;
        pwm_cap_coolant.period_ticks = 0;
        pwm_cap_rpm.deltaT = 0;
        pwm_cap_rpm.period_ticks = 0;
        pwm_cap_speed.deltaT = 0;
        pwm_cap_speed.period_ticks = 0;
#endif */
    }
}