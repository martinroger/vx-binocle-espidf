#include "sma_filter.h"
#include <stdlib.h> // For malloc and free

#ifdef TAG
#undef TAG
#endif
#define TAG "SMA"

sma_handle_t *sma_init_full(uint8_t size, int16_t startValue)
{
    ESP_LOGI(TAG, "Initializing SMA of %u elements with start value %d. Buffer size %lu bytes.", size, startValue, (uint32_t)(size * 16));

    // Sanity check the size
    if (size == 0)
    {
        ESP_LOGE(TAG, "SMA Size is invalid, aborting init.");
        return NULL;
    }
    // Allocate space in memory for the handle itself via its pointer
    sma_handle_t *sma = (sma_handle_t *)malloc(sizeof(sma_handle_t));
    if (sma == NULL)
    {
        ESP_LOGE(TAG, "Impossible to malloc for the SMA. Aborting.");
        return NULL;
    }
    // Allocate zeroed space for the buffer itself
    sma->buffer = (int16_t *)calloc(size, sizeof(int16_t));
    if (sma->buffer == NULL)
    {
        free(sma);
        ESP_LOGE(TAG, "Could not calloc space for the buffer. Aborting.");
        return NULL;
    }

    // Set start value in the buffer then derive the other structure components
    for (int i = 0; i < size; i++)
    {
        sma->buffer[i] = startValue;
    }
    sma->size = size;
    sma->head = 0; // Current cursor is at position 0
    sma->sum = startValue * size;
    sma->count = size; // SMA is considered full
    sma->mutex = xSemaphoreCreateMutex();
    if (sma->mutex == NULL)
    {
        ESP_LOGE(TAG, "Could not generate Mutex for SMA, aborting.");
        free(sma->buffer);
        free(sma);
        return NULL;
    }
    ESP_LOGI(TAG, "Successfully initialized SMA.");
    return sma;
}

sma_handle_t *sma_init(uint8_t size)
{
    return sma_init_full(size, 0);
}

void sma_add(sma_handle_t *sma, int16_t value)
{
    // Check SMA exists and write Semaphore is available.
    if (sma == NULL || xSemaphoreTake(sma->mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Could not add to SMA");
        return;
    }

    // Subtract the old value that is being replaced
    sma->sum -= sma->buffer[sma->head];

    // Add the new value to the buffer and the sum
    sma->buffer[sma->head] = value;
    sma->sum += value;

    // Move head to the next position
    sma->head = (sma->head + 1) % sma->size;

    // Keep track of the number of elements until the buffer is full
    if (sma->count < sma->size)
    {
        ESP_LOGI(TAG, "SMA buffer not full, count %u vs size %u", sma->count, sma->size);
        sma->count++;
    }
    // Release the write Semaphore
    xSemaphoreGive(sma->mutex);
}

float sma_get_avg(sma_handle_t *sma)
{
    // Sanity checks and special empty SMA case.
    if (sma == NULL)
    {
        ESP_LOGW(TAG, "Invalid SMA handle");
        return 0.0f;
    }
    if (sma->count == 0)
    {
        return 0.0f;
    }
    // Normal case
    float avg = 0.0f;
    // Prevent modifications to the SMA while the average is being calculated !!
    if (xSemaphoreTake(sma->mutex, portMAX_DELAY) == pdTRUE)
    {
        avg = (float)sma->sum / sma->count;
        xSemaphoreGive(sma->mutex);
    }
    return avg;
}

void sma_deinit(sma_handle_t *sma)
{
    if (sma == NULL)
    {
        ESP_LOGW(TAG, "SMA does not exist, nothing to deinit !");
        return;
    }

    vSemaphoreDelete(sma->mutex);
    free(sma->buffer);
    free(sma);
}