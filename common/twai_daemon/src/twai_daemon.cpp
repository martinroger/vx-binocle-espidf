/**
 * @file twai_daemon.cpp
 * @brief Implementation of the TWAI Router Daemon and ISR callbacks.
 */

#include "twai_daemon.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "CAN Daemon";

bool CAN_RX_TimedOut = false;

twai_node_handle_t can_node_hdl = nullptr;

/** @brief Maximum number of simultaneous CAN route subscriptions. */
#define MAX_CAN_ROUTES 32

/**
 * @struct can_route_t
 * @brief Internal routing table entry mapping a CAN ID/mask filter to a destination queue.
 */
struct can_route_t {
    uint32_t id;                /**< Base CAN identifier to match */
    uint32_t mask;              /**< Bitmask filter (1 = check bit, 0 = ignore) */
    QueueHandle_t target_queue; /**< Destination FreeRTOS queue handle */
    bool active;                /**< True if this route slot is active */
};

/** @brief Static route registry storage array. */
static can_route_t route_table[MAX_CAN_ROUTES];

esp_err_t twai_daemon_register_route(uint32_t id, uint32_t mask, QueueHandle_t queue) {
    for (int i = 0; i < MAX_CAN_ROUTES; i++) {
        if (!route_table[i].active) {
            route_table[i].id = id;
            route_table[i].mask = mask;
            route_table[i].target_queue = queue;
            route_table[i].active = true;
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG, "Failed to register route: Table Full");
    return ESP_ERR_NO_MEM;
}

void twai_daemon_unregister_route(QueueHandle_t queue) {
    for (int i = 0; i < MAX_CAN_ROUTES; i++) {
        if (route_table[i].active && route_table[i].target_queue == queue) {
            route_table[i].active = false;
        }
    }
}

/**
 * @brief ISR Event Callback triggered when TWAI frame reception completes.
 * @details Drains all received frames from hardware FIFO, matches them against active
 *          routes in `route_table`, and posts matching frames to subscribed FreeRTOS queues.
 * @param[in] handle Handle to the TWAI node emitting the interrupt.
 * @param[in] edata Pointer to event data containing RX metrics.
 * @param[in] user_ctx Context pointer (unused).
 * @return True if a higher-priority task was woken and a yield is requested; false otherwise.
 */
static bool on_rx_done(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    twai_frame_t rx_frame;
    bool need_yield = false;
    
    while (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
        twai_message_t rxMessage;
        rxMessage.identifier = rx_frame.header.id;
        rxMessage.data_length_code = rx_frame.header.dlc;
        rxMessage.extd = rx_frame.header.ide;
        rxMessage.rtr = rx_frame.header.rtr;
        rxMessage.ss = false;
        
        if (!rx_frame.header.rtr && rx_frame.buffer_len > 0 && rx_frame.buffer_len <= 8) {
            memcpy(rxMessage.data, rx_frame.buffer, rx_frame.buffer_len);
        }
        
        for (int i = 0; i < MAX_CAN_ROUTES; i++) {
            if (route_table[i].active && 
               (rx_frame.header.id & route_table[i].mask) == (route_table[i].id & route_table[i].mask)) {
                
                BaseType_t woken = pdFALSE;
                xQueueSendFromISR(route_table[i].target_queue, &rxMessage, &woken);
                if (woken == pdTRUE) {
                    need_yield = true;
                }
            }
        }
    }
    return need_yield;
}

esp_err_t twai_daemon_transmit(const twai_message_t *msg, TickType_t ticks_to_wait)
{
    if (can_node_hdl == nullptr) return ESP_FAIL;
    
    twai_frame_t tx_frame = {};
    tx_frame.header.id = msg->identifier;
    tx_frame.header.dlc = msg->data_length_code;
    tx_frame.header.ide = msg->extd;
    tx_frame.header.rtr = msg->rtr;
    tx_frame.header.fdf = 0;
    tx_frame.header.brs = 0;
    tx_frame.header.esi = 0;
    
    tx_frame.buffer = (uint8_t*)msg->data;
    tx_frame.buffer_len = msg->data_length_code;
    
    return twai_node_transmit(can_node_hdl, &tx_frame, ticks_to_wait);
}

esp_err_t initCAN(void)
{
#ifdef TWAI_WATCHDOG
    unsigned long twai_wdg_rx_dropped = 0;
#endif

    // Route table is statically zero-initialized, which safely sets .active = false

    twai_onchip_node_config_t node_config = {};
    node_config.io_cfg.tx = (gpio_num_t)CONFIG_CAN_TX;
    node_config.io_cfg.rx = (gpio_num_t)CONFIG_CAN_RX;
    node_config.io_cfg.quanta_clk_out = (gpio_num_t)-1;
    node_config.io_cfg.bus_off_indicator = (gpio_num_t)-1;
    
    node_config.clk_src = TWAI_CLK_SRC_DEFAULT;
    node_config.bit_timing.bitrate = 500000;
    node_config.bit_timing.sp_permill = 0;
    node_config.bit_timing.ssp_permill = 0;
    node_config.fail_retry_cnt = -1;
    node_config.tx_queue_depth = 256;
    node_config.intr_priority = 0;
    
    node_config.flags.enable_self_test = 0;
    node_config.flags.enable_loopback = 0;
    node_config.flags.enable_listen_only = 0;
    node_config.flags.no_receive_rtr = 0;

    if (twai_new_node_onchip(&node_config, &can_node_hdl) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install TWAI driver");
        return ESP_FAIL;
    }

    twai_event_callbacks_t cbs = {};
    cbs.on_tx_done = NULL;
    cbs.on_rx_done = on_rx_done;
    cbs.on_state_change = NULL;
    cbs.on_error = NULL;
    
    twai_node_register_event_callbacks(can_node_hdl, &cbs, NULL);

    if (twai_node_enable(can_node_hdl) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start TWAI driver");
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(TAG, "TWAI driver started successfully");
    }

    // All checks passed
    return ESP_OK;
}

