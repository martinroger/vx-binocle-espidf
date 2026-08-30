#pragma once
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdio.h"
#include "stdlib.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_twai_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#ifndef CONFIG_CAN_RX
#define CONFIG_CAN_RX 3
#endif

#ifndef CONFIG_CAN_TX
#define CONFIG_CAN_TX 2
#endif

#ifndef CONFIG_CAN_CORE_AFFINITY
#define CONFIG_CAN_CORE_AFFINITY 1
#endif

#ifndef CONFIG_CAN_RX_TIMEOUT_MS
#define CONFIG_CAN_RX_TIMEOUT_MS 3000
#endif

/// @brief Queued frame container for threadsafe ISR-to-Task handoff
typedef struct {
    twai_frame_header_t header;
    uint8_t data[8];
    uint8_t buffer_len;
} twai_queued_frame_t;

typedef esp_err_t frameDispatcher_t(const twai_frame_t *messageToDispatch);

// Global TWAI node handle
extern twai_node_handle_t g_twai_node_hdl;

// Pointer to dispatcher function, attached on init and defined externally
extern frameDispatcher_t *dispatchCANFrame;

// Status and state flags
extern bool CAN_RX_TimedOut;
extern bool g_twai_bus_off;

// Task handle for RX worker
extern TaskHandle_t CAN_RX_tsk_hdl;

// RX queue handle
extern QueueHandle_t g_twai_rx_queue;

/// @brief Initialises the modern TWAI driver node and attaches optional frame dispatcher function
/// @param frameDispatcher Pointer to frame dispatcher function or nullptr if RX dispatch is not needed
/// @return ESP_OK on success, error code otherwise
esp_err_t initCAN(frameDispatcher_t *frameDispatcher = nullptr);

/// @brief Transmit a TWAI frame directly into driver queue
/// @param frame Pointer to twai_frame_t
/// @param timeout_ms Timeout in milliseconds
/// @return ESP_OK on success, error code otherwise
esp_err_t twai_transmit_frame(const twai_frame_t *frame, int timeout_ms = 10);

/// @brief Helper to transmit standard CAN message payload
/// @param can_id CAN identifier (11-bit standard or 29-bit extended)
/// @param data Pointer to payload data
/// @param dlc Data length code (0 to 8)
/// @param is_ext True if extended 29-bit ID
/// @param timeout_ms Timeout in milliseconds
/// @return ESP_OK on success, error code otherwise
esp_err_t twai_transmit_msg(uint32_t can_id, const uint8_t *data, uint8_t dlc, bool is_ext = false, int timeout_ms = 10);

/// @brief Receive a queued frame directly from the RX queue
/// @param frame Output frame struct
/// @param wait_ticks FreeRTOS ticks to wait
/// @return ESP_OK on success, ESP_ERR_TIMEOUT or ESP_FAIL otherwise
esp_err_t twai_receive_queued_frame(twai_queued_frame_t *frame, TickType_t wait_ticks);

/// @brief Clears all pending frames in the RX queue
/// @return ESP_OK on success
esp_err_t twai_clear_rx_queue();

/// @brief FreeRTOS task that ingests frames from ISR queue and sends to dispatcher
/// @param pvParameters Task parameters
void CAN_RX_Task(void *pvParameters);