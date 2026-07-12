#pragma once
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdio.h"
#include "stdlib.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#ifndef CONFIG_CAN_RX
#define CONFIG_CAN_RX 3
#endif

#ifndef CONFIG_CAN_TX
#define CONFIG_CAN_TX 2
#endif

// Only active if the TWAI_WATCHDOG is used

// RX TimeOut flag. Managed externally by receiving tasks.

// RX TimeOut flag. Not necessarily used
extern bool CAN_RX_TimedOut ;

// Underlying driver node handle
extern twai_node_handle_t can_node_hdl;

/// @brief Initialises the TWAI driver
/// @return OK when all is well, otherwise an error code
esp_err_t initCAN(void);

/// @brief Register a route for a specific CAN ID and mask to be pushed to a queue
/// @param id The ID to match
/// @param mask The bitmask to apply to the incoming ID (1 = care, 0 = don't care)
/// @param queue The FreeRTOS Queue to receive matched twai_message_t items
/// @return ESP_OK on success, otherwise error
esp_err_t twai_daemon_register_route(uint32_t id, uint32_t mask, QueueHandle_t queue);

/// @brief Unregister a previously registered route by queue handle
/// @param queue The FreeRTOS Queue to unregister
void twai_daemon_unregister_route(QueueHandle_t queue);

/// @brief Queues a CAN message to the internal driver TX queue
/// @param msg Pointer to the message to send
/// @param ticks_to_wait Ticks to wait if the internal TX queue is full
/// @return ESP_OK on success, otherwise an error code
esp_err_t twai_daemon_transmit(const twai_message_t *msg, TickType_t ticks_to_wait);