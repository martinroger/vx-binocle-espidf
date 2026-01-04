#pragma once
#ifndef SMA_FILTER_H
#define SMA_FILTER_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SMA_BUFFER_SIZE 10 // Example size for the moving average window

    typedef struct
    {
        int16_t *buffer;         // Pointer to the data buffer
        uint8_t size;            // Size of the buffer
        uint8_t head;            // Index to add the next element
        int32_t sum;             // Running sum of the elements
        uint8_t count;           // Number of elements currently in the buffer
        SemaphoreHandle_t mutex; // Mutex for thread safety
    } sma_handle_t;

    /**
     * @brief Initialize the SMA filter
     *
     * @param size Size of the moving average window
     * @param startValue Value to fill the buffer vector with
     * @return Handle to the SMA filter structure, or NULL on failure
     */
    sma_handle_t *sma_init_full(uint8_t size, int16_t startValue);

    /// @brief Initialize the SMA filter
    /// @param size Size of the moving average window
    /// @return Handle to the SMA filter structure, or NULL on failure
    sma_handle_t *sma_init(uint8_t size);

    /**
     * @brief Add a new value to the SMA filter
     *
     * @param sma Handle to the SMA filter
     * @param value The new value to add
     */
    void sma_add(sma_handle_t *sma, int16_t value);

    /**
     * @brief Get the current moving average
     *
     * @param sma Handle to the SMA filter
     * @return The calculated simple moving average
     */
    float sma_get_avg(sma_handle_t *sma);

    /**
     * @brief Deinitialize the SMA filter and free resources
     * * @param sma Handle to the SMA filter
     */
    void sma_deinit(sma_handle_t *sma);

#ifdef __cplusplus
}
#endif

#endif // SMA_FILTER_H