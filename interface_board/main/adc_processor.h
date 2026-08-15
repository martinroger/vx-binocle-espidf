#pragma once
#include "adc_helpers.h"
#include "sma_filter.h"


volatile int16_t adc_raw_buffer[4] = {0}; // Buffer to hold raw ADC measurements for each channel

static TaskHandle_t adc_conversion_Hdl;        // Task handle for ADC conversion task
static SemaphoreHandle_t adc_access_Semaphore; // Semaphore to protect ADC access

/// @brief ADC pickup task, protected by local Semaphore for ADC access
/// @param pvParameters 
inline void adc_conversion_task(void *pvParameters)
{
    while (1)
    {
        // Lock in case of parallel request to access ADC
        if (xSemaphoreTake(adc_access_Semaphore, pdMS_TO_TICKS(1)) == pdTRUE)
        {
            for (int i = 0; i < 4; i++)
            {
                adc_raw_buffer[i] = adc_measure_channel_raw(i);
            }
            xSemaphoreGive(adc_access_Semaphore);
        }
        else
        {
            ESP_LOGW(__func__, "Already sampling from ADC !");
        }
        // Wait the necessary amount of time for doing all the conversions, if longer than desired polling rate
        vTaskDelay(pdMS_TO_TICKS((4 * conversion_interval_ms > CONFIG_ADC_POLLING_RATE_MS) ? 4 * conversion_interval_ms : CONFIG_ADC_POLLING_RATE_MS));
    }
}

/// @brief ADC processor initialisation. Does NOT initialize any filters.
/// @return ESP_OK on successful initialisation, ESP_FAIL otherwise.
inline esp_err_t initialize_ADC_processor()
{
    // Create semaphore
    adc_access_Semaphore = xSemaphoreCreateBinary();
    if (adc_access_Semaphore == NULL)
    {
        ESP_LOGE(__func__, "Could not create ADC access Semaphore, aborting.");
        return ESP_FAIL;
    }
    // Free Semaphore
    xSemaphoreGive(adc_access_Semaphore);

    // Initialize ADC
    if (initialize_ADC() != ESP_OK)
    {
        ESP_LOGE(__func__, "Could not initialize ADC unit.");
        return ESP_FAIL;
    }

    // Do a first read
    if (xSemaphoreTake(adc_access_Semaphore, pdMS_TO_TICKS(1)) == pdTRUE)
    {
        for (int i = 0; i < 4; i++)
        {
            adc_raw_buffer[i] = adc_measure_channel_raw(i);
        }
        xSemaphoreGive(adc_access_Semaphore);
    }
    else
    {
        ESP_LOGW(__func__, "Already sampling from ADC !");
    }

    // Sanity check that the polling rate is not faster than what is achievable.
    if (CONFIG_ADC_POLLING_RATE_MS < 4 * conversion_interval_ms)
    {
        ESP_LOGW(__func__, "ADC to SMA polling rate is faster than channel acquisition loop time. Actual polling rate will be %lu ms", 4 * conversion_interval_ms);
    }

    // Create task
    if (xTaskCreate(adc_conversion_task, "ADC Conversion Task", 2048 + 1024, NULL, 5, &adc_conversion_Hdl) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create ADC conversion task, aborting.");
        return ESP_FAIL;
    }

    return ESP_OK;
}