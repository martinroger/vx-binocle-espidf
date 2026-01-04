/*
Binding and essential functions to pass values from the ADC to the SMA.
Limitation/simplification : although there are 4 channels available on the ADS1115, the mux has to be redirected so they need to be read in sequence.
Rather than juggling vTaskDelays and conversion-waiting semaphores for each channel, they are all read sequentially in one loop that is kept synchronous.
A preservation Semaphore remains at the high level to avoid other functions calling for measurements in parallel.
Given there is only one task, it should not happen...
*/

#pragma once
#include "adc_helpers.h"
#include "sma_filter.h"
#ifdef TAG
#undef TAG
#endif
#define TAG "ADC Processor"

// Number of channels. Currently cannot be changed  because of the sma_sizes
#define NUM_ADC_CHANNELS 4
// Pre-set SMA sizes pulled from the KConfig
uint8_t sma_sizes[NUM_ADC_CHANNELS] = {CONFIG_SMA_0_SIZE, CONFIG_SMA_1_SIZE, CONFIG_SMA_2_SIZE, CONFIG_SMA_3_SIZE};

// Binding structure for each SMA to their respective channel
typedef struct
{
    sma_handle_t *sma; // Pointer to SMA accumulator
    uint8_t channel;   // Channel index (0-3)
} adc_channel_ctx_t;

// Array of context structures for each SMA-ADC pair.
static adc_channel_ctx_t adc_channels[NUM_ADC_CHANNELS] = {0};

// Task used to transfer ADC readings to the SMA. Only one task for all four channels !
TaskHandle_t adc_to_sma_handle;                // Task to regroup ADC readings towards their dedicated channel (round robin)
static SemaphoreHandle_t adc_to_sma_Semaphore; // ADC access semaphore

/// @brief Cyclic task that measures all channels in sequence, and passes the value to the respective SMA
/// @param pvParameters
inline void adc_to_sma_task(void *pvParameters)
{
    while (1)
    {
        // Lock in case of parallel request to access ADC or SMA
        if (xSemaphoreTake(adc_to_sma_Semaphore, pdMS_TO_TICKS(1)) == pdTRUE)
        {
            for (int i = 0; i < NUM_ADC_CHANNELS; i++)
            {
                int16_t adc_measurement = adc_measure_channel_raw(adc_channels[i].channel);
                sma_add(adc_channels[i].sma, adc_measurement);
            }
            xSemaphoreGive(adc_to_sma_Semaphore);
        }
        else
        {
            ESP_LOGW(TAG, "Already sampling from ADC to SMA !");
        }
        vTaskDelay(pdMS_TO_TICKS((NUM_ADC_CHANNELS * conversion_interval_ms > CONFIG_ADC_TO_SMA_POLLING_RATE_MS) ? NUM_ADC_CHANNELS * conversion_interval_ms : CONFIG_ADC_TO_SMA_POLLING_RATE_MS)); // Wait the necessary amount of time for doing all the conversions, if longer than desired polling rate
    }
}

/// @brief Initialization function for the ADC processor, sets up task and Semaphore
/// @return
inline esp_err_t initialize_adc_processor()
{
    // Create semaphore
    adc_to_sma_Semaphore = xSemaphoreCreateBinary();
    if (adc_to_sma_Semaphore == NULL)
    {
        ESP_LOGE(TAG, "Could not create ADC SMA Semaphore, aborting.");
        return ESP_FAIL;
    }
    // Free Semaphore
    xSemaphoreGive(adc_to_sma_Semaphore);

    // Initialize ADC
    if (initialize_ADC() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialize ADC unit.");
        return ESP_FAIL;
    }

    // Initialize each ADC channel and SMA
    if (xSemaphoreTake(adc_to_sma_Semaphore, pdMS_TO_TICKS(1)) == pdTRUE)
    {
        for (uint8_t i = 0; i < NUM_ADC_CHANNELS; ++i)
        {
            ESP_LOGI(TAG, "Initialising channel and SMA #%u", i);
            adc_channels[i].channel = i;
            uint16_t startValue = adc_measure_channel_raw(i); // Fill with a first measurement
            adc_channels[i].sma = sma_init_full(sma_sizes[i], startValue);

            if (adc_channels[i].sma == NULL)
            {
                ESP_LOGE(TAG, "Could not initialize SMA for channel %d", i);
                xSemaphoreGive(adc_to_sma_Semaphore);
                return ESP_FAIL;
            }
        }
        xSemaphoreGive(adc_to_sma_Semaphore);
    }
    else
    {
        ESP_LOGE(TAG, "Could not lock ADC-SMA Semaphore for initialization. Aborting.");
        return ESP_FAIL;
    }

    // Sanity check that the polling rate is not faster than what is achievable.
    if (CONFIG_ADC_TO_SMA_POLLING_RATE_MS < NUM_ADC_CHANNELS * conversion_interval_ms)
    {
        ESP_LOGW(TAG, "ADC to SMA polling rate is faster than channel acquisition loop time. Actual polling rate will be %lu ms", NUM_ADC_CHANNELS * conversion_interval_ms);
    }
    // Create task
    if (xTaskCreate(adc_to_sma_task, "ADC to SMA", 2048 + 1024, NULL, 5, &adc_to_sma_handle) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create ADC to SMA task, aborting.");
        return ESP_FAIL;
    }

    return ESP_OK;
}