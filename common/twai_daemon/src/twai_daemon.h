/**
 * @file twai_daemon.h
 * @brief Real-time FreeRTOS TWAI / CAN Bus Router Daemon Interface.
 * @details Provides an ISR-driven CAN message routing engine, hardware node initialization,
 *          and frame transmission helpers using ESP-IDF v6+ esp_driver_twai APIs.
 */

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

/** @brief Default TWAI RX GPIO pin assignment (if not configured via Kconfig). */
#ifndef CONFIG_CAN_RX
#define CONFIG_CAN_RX 3
#endif

/** @brief Default TWAI TX GPIO pin assignment (if not configured via Kconfig). */
#ifndef CONFIG_CAN_TX
#define CONFIG_CAN_TX 2
#endif

/** @brief External timeout indicator flag managed by receiving tasks. */
extern bool CAN_RX_TimedOut;

/** @brief Global handle for the underlying TWAI driver node. */
extern twai_node_handle_t can_node_hdl;

/**
 * @brief Initialises the TWAI driver hardware node and registers event callbacks.
 * @details Configures GPIO pins, timing (500 kbps), TX queue depth (256 frames),
 *          registers the ISR-driven `on_rx_done` callback, and enables the TWAI peripheral.
 * @return ESP_OK on success, or an error code from the underlying TWAI driver.
 */
esp_err_t initCAN(void);

/**
 * @brief Register a routing rule for matching incoming CAN IDs to a target FreeRTOS queue.
 * @details When a CAN frame arrives, the ISR compares `(rx_id & mask) == (id & mask)`.
 *          If matched, the frame is posted directly to @p queue from ISR context.
 * @param[in] id The base CAN ID to match against incoming messages.
 * @param[in] mask Bitmask applied to incoming ID (1 = bit must match, 0 = don't care).
 * @param[in] queue The FreeRTOS QueueHandle_t to receive matched @ref twai_message_t items.
 * @return ESP_OK if route registered successfully, ESP_ERR_NO_MEM if route table is full.
 */
esp_err_t twai_daemon_register_route(uint32_t id, uint32_t mask, QueueHandle_t queue);

/**
 * @brief Unregister all active routing rules associated with a target FreeRTOS queue.
 * @param[in] queue The FreeRTOS QueueHandle_t to unregister from the router table.
 */
void twai_daemon_unregister_route(QueueHandle_t queue);

/**
 * @brief Transmit a CAN message onto the TWAI bus via the hardware driver TX queue.
 * @param[in] msg Pointer to the @ref twai_message_t frame to transmit.
 * @param[in] ticks_to_wait FreeRTOS tick duration to block if the hardware TX queue is full.
 * @return ESP_OK on successful enqueue, ESP_FAIL if driver uninitialized, or ESP_ERR_TIMEOUT.
 */
esp_err_t twai_daemon_transmit(const twai_message_t *msg, TickType_t ticks_to_wait);