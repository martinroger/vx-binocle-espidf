#pragma once
#include <stdio.h>
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "MCPWM_CAP"

/// @brief Holder structure for the raw metrics of a pwm (single sample) and computed metrics
typedef struct
{
    uint32_t pos_edge_ts;      // Tick timestamp of the last positive edge
    uint32_t prev_pos_edge_ts; // Tick tmestamp of the previous positive edge
    uint32_t period_ticks;     // Period in ticks between two positive edges
    uint32_t neg_edge_ts;      // Tick timestamp of the last negative edge
    uint32_t deltaT;           // Tick difference between the last negative and positive edge
    uint32_t pulse_counter;
    float duty_cycle = 0.0;
    float frequency = 0.0;
} pwm_info_t;

/// @brief Compute frequency and duty cycle
/// @param pwm_info Target for a specific PWM capture channel
/// @param clock_Hz Capture source clock (natively locked at 80MHz)
/// @return ESP_OK if there is a successful compute, ESP_FAIL if the compute failed because of timeout or 0 ticks.
esp_err_t compute_freq_dut(volatile pwm_info_t *pwm_info, uint32_t clock_Hz = 80000000)
{
    // Get current time in microseconds
    uint64_t now_us = esp_timer_get_time();
    // Convert last positive edge timestamp to microseconds (assuming timer ticks at clock_Hz)
    // Timestamp is updated by ISR mcpwm_capture_cb_generic
    uint64_t last_edge_us = ((uint64_t)pwm_info->pos_edge_ts * 1000000ULL) / clock_Hz;
    // If last edge was more than 1000ms ago, return 0 -> Case of a 1s timeout
    if ((now_us > last_edge_us) && ((now_us - last_edge_us) > CONFIG_PWM_DETECT_TIMEOUT * 1000ULL))
    {
        pwm_info->frequency = 0;
        pwm_info->duty_cycle = 0;
        return ESP_FAIL;
    }
    // Rare case where the period between two positive edges is 0 ticks
    if (pwm_info->period_ticks == 0)
    {
        pwm_info->frequency = 0;
        pwm_info->duty_cycle = 0;
        return ESP_FAIL;
    }
    // Normal case
    pwm_info->frequency = 1.0 / ((float)pwm_info->period_ticks / (float)(clock_Hz)); // Intended to force correct float calculation
    pwm_info->duty_cycle = (float)pwm_info->deltaT / (float)pwm_info->period_ticks;
    return ESP_OK;
}

/// @brief MCPWM generic capture ISR that updates a PWM data structure
/// @param cap_chan Capture channel
/// @param edata Event data
/// @param user_data Target PWM data structure to update
/// @return Always false because currently not triggering a context switch
static bool IRAM_ATTR mcpwm_capture_cb_generic(mcpwm_cap_channel_handle_t cap_chan,
                                               const mcpwm_capture_event_data_t *edata,
                                               void *user_data)
{
    // Cast the correct pwm_info_t
    pwm_info_t *target_pwm_signal = static_cast<pwm_info_t *>(user_data);

    // portENTER_CRITICAL_ISR(&counter_mux);    // Not sure if needed, seems OK without ?

    if (edata->cap_edge == MCPWM_CAP_EDGE_POS) // Event data is a positive edge
    {
        // Shift the previous positive edge timestamp to the relevant follower property
        target_pwm_signal->prev_pos_edge_ts = target_pwm_signal->pos_edge_ts;
        // Record the tick timestamp of the newly detected positive edge
        target_pwm_signal->pos_edge_ts = edata->cap_value;
        // Get the number of ticks of one period
        target_pwm_signal->period_ticks = target_pwm_signal->pos_edge_ts - target_pwm_signal->prev_pos_edge_ts;
        // Increase the pulse counter
        target_pwm_signal->pulse_counter++;
    }
    else if (edata->cap_edge == MCPWM_CAP_EDGE_NEG) // Event data is a negative edge
    {
        // Get falling edge tick timestamp
        target_pwm_signal->neg_edge_ts = edata->cap_value;
        // Calculate "ON" time in ticks to get the duty cycle
        target_pwm_signal->deltaT = target_pwm_signal->neg_edge_ts - target_pwm_signal->pos_edge_ts;
    }
    // portEXIT_CRITICAL_ISR(&counter_mux);     // Not sure if needed, seems OK without ?
    return false;
}

/// @brief Creator and initialisator for a single MCPWM capture channel, using APB 80MHz clock
/// @param target_cap_chan Handle of the capture channel to initialise and start
/// @param cap_gpio Physical GPIO associated for capture
/// @param pwm_info_buffer Target PWM info buffer that will be updated by the capture callback
/// @return ESP_OK if all started correctly, ESP_FAIL otherwise (not implemented yet)
esp_err_t set_capture_channel(mcpwm_cap_channel_handle_t target_cap_chan,
                              gpio_num_t cap_gpio,
                              volatile pwm_info_t *pwm_info_buffer)
{
    // MCPWM Capture timer set up. Only one used.
    static mcpwm_cap_timer_handle_t cap_timer = NULL;
    if (cap_timer == NULL)
    {
        ESP_LOGI(TAG, "Creating new capture timer");
        mcpwm_capture_timer_config_t cap_timer_config = {
            .group_id = 0,
            .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
        };
        // Create capture timer
        if (mcpwm_new_capture_timer(&cap_timer_config, &cap_timer) != ESP_OK)
        {
            ESP_LOGE(TAG, "Capture timer could not be created, aborting.");
            return ESP_FAIL;
        }
        // Enable capture timer
        switch (mcpwm_capture_timer_enable(cap_timer))
        {
        case ESP_OK:
            ESP_LOGI(TAG, "MCPWM Capture timer enabled.");
            break;
        case ESP_ERR_INVALID_STATE:
            ESP_LOGW(TAG, "MCPWM Capture timer already enabled.");
            break;
        default:
            ESP_LOGE(TAG, "Could not enable MCPWM Capture timer. Aborting.");
            return ESP_FAIL;
            break;
        }
        // Start the capture timer
        if (mcpwm_capture_timer_start(cap_timer) != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not start MCPWM Capture timer, aborting.");
            return ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGI(TAG, "Reusing existing capture timer");
    }

    // MCPWM Capture channel setup. Assuming all signals are positive when active (could be wrong)
    mcpwm_capture_channel_config_t cap_chan_config = {
        .gpio_num = cap_gpio,
        .intr_priority = 1,
        .prescale = 1,
        .flags = {
            .pos_edge = true,
            .neg_edge = true,
            .pull_up = false,
            .pull_down = true,
            .invert_cap_signal = false,
            .io_loop_back = false}};
    // Create capture channel with target handle, common timer and config.
    if (mcpwm_new_capture_channel(cap_timer, &cap_chan_config, &target_cap_chan) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not create MCPWM Capture channel, aborting.");
        return ESP_FAIL;
    }

    // Attach capture even callback using the generic CB and passing the target pwm info buffer for the given channel
    mcpwm_capture_event_callbacks_t cap_cbs = {
        .on_cap = mcpwm_capture_cb_generic};
    if (mcpwm_capture_channel_register_event_callbacks(target_cap_chan, &cap_cbs, (void *)pwm_info_buffer) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could register capture callback for MCPWM capture channel");
        return ESP_FAIL;
    }
    // Start the bloody MCPWM capture channel
    switch (mcpwm_capture_channel_enable(target_cap_chan))
    {
    case ESP_OK:
        ESP_LOGI(TAG, "MCPWM Capture channel enabled.");
        break;
    case ESP_ERR_INVALID_STATE:
        ESP_LOGW(TAG, "MCPWM Capture channel somehow already enabled, continuing...");
        break;
    default:
        ESP_LOGE(TAG, "Could not enable capture channel. Aborting.");
        return ESP_FAIL;
        break;
    }
    return ESP_OK;
}