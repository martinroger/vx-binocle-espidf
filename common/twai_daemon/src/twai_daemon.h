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



typedef esp_err_t frameDispatcher_t(twai_message_t *messageToDispatch);

// Pointer to dispatcher function, attached on init and defined externally
extern frameDispatcher_t *dispatchCANFrame ;

// RX TimeOut flag. Not necessarily used
extern bool CAN_RX_TimedOut ;

// Pointer to rx task handle
extern TaskHandle_t CAN_RX_tsk_hdl ;

// Underlying driver node handle
extern twai_node_handle_t can_node_hdl;

/// @brief Initialises the TWAI driver and attaches the frame dispatcher function
/// @param frameDispatcher 
/// @return OK when all is well, otherwise an error code
esp_err_t initCAN(frameDispatcher_t *frameDispatcher);

/// @brief FreeRTOS task that receives and send frames to the dispatcher function
/// @param pvParameters 
void CAN_RX_Task(void *pvParameters);

/// @brief Queues a CAN message to the internal driver TX queue
/// @param msg Pointer to the message to send
/// @param ticks_to_wait Ticks to wait if the internal TX queue is full
/// @return ESP_OK on success, otherwise an error code
esp_err_t twai_daemon_transmit(const twai_message_t *msg, TickType_t ticks_to_wait);