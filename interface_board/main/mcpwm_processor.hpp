/**
 * @file mcpwm_processor.hpp
 * @brief MCPWM capture signal processing and Bayesian gear estimation background tasks.
 *
 * Runs a deterministic 25 ms periodic ingestion loop that calculates frequency and duty cycle
 * for coolant, engine RPM, and wheel speed capture channels, then feeds instantaneous frequencies
 * into the kinematic Bayesian gear estimator.
 */

#pragma once
#ifndef MCPWM_PROCESSOR_HPP
#define MCPWM_PROCESSOR_HPP

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "gear_estimator_params.h"
#include "mcpwm_capture_helpers.h"
#include "global_vars.hpp"

/**
 * @brief Sampling cycle time for MCPWM pulse calculation and gear estimation in milliseconds.
 */
#define MCPWM_PROCESSOR_CYCLE_TIME_MS (25)

/**
 * @brief Sampling delta time in seconds passed to the gear estimator update.
 */
#define MCPWM_PROCESSOR_SAMPLE_DT_S   (0.025f)

/**
 * @brief Periodic FreeRTOS worker task for computing MCPWM frequencies and running gear estimation.
 *
 * Runs deterministically every 25 ms using vTaskDelayUntil(), updates duty cycle and frequency
 * for coolant, RPM, and speed capture channels, and feeds these into gear_bayesian_update().
 *
 * @param[in] pvParameters Unused FreeRTOS task parameter (Default: NULL).
 *
 * @note Thread Safety: Reads volatile capture buffers updated by ISRs; writes to global gear_estimator.
 * @note Side Effects: Updates frequency fields of pwm_cap_coolant, pwm_cap_rpm, pwm_cap_speed, and gear_estimator state.
 */
inline void acquire_mcpwm(void *pvParameters)
{
    (void)pvParameters;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (true)
    {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MCPWM_PROCESSOR_CYCLE_TIME_MS));
        compute_freq_dut(&pwm_cap_coolant);
        compute_freq_dut(&pwm_cap_rpm);
        compute_freq_dut(&pwm_cap_speed);
        gear_bayesian_update(&gear_estimator, pwm_cap_speed.frequency, pwm_cap_rpm.frequency, MCPWM_PROCESSOR_SAMPLE_DT_S);
    }
}

/**
 * @brief Initialize the Bayesian gear estimator and launch the MCPWM acquisition background task.
 *
 * @return esp_err_t ESP_OK on successful task creation, ESP_FAIL if xTaskCreatePinnedToCore() fails.
 *
 * @note Thread Safety: Must be called during system initialization before starting telemetry packaging tasks.
 * @note Side Effects: Allocates FreeRTOS task with 4096 bytes stack pinned to CONFIG_CAN_CORE_AFFINITY.
 */
inline esp_err_t mcpwm_processor_init(void)
{
    esp_err_t ret = ESP_OK;

    gear_bayesian_init(&gear_estimator);

    if (xTaskCreatePinnedToCore(acquire_mcpwm, "PWM PROC", 4096, NULL, 4, &acquire_mcpwm_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create MCPWM processor task");
        ret = ESP_FAIL;
    }

    return ret;
}

#endif /* MCPWM_PROCESSOR_HPP */
