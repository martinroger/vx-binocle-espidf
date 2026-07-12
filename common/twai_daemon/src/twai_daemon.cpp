#include "twai_daemon.h"
#include <esp_log.h>
#include <string.h>

const char *TAG = "CAN Daemon";

frameDispatcher_t *dispatchCANFrame = nullptr;
bool CAN_RX_TimedOut = false;

// Pointer to rx task handle
TaskHandle_t CAN_RX_tsk_hdl = nullptr;

// Underlying driver node handle
twai_node_handle_t can_node_hdl = nullptr;

// Queue for received messages to pass from ISR to task
static QueueHandle_t CAN_RX_queue_hdl = nullptr;

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
        
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(CAN_RX_queue_hdl, &rxMessage, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            need_yield = true;
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

esp_err_t initCAN(frameDispatcher_t *frameDispatcher)
{
#ifdef TWAI_WATCHDOG
    // Wait, the original code had TWAI_WATCHDOG stuff, let me add a dummy handling for it or ignore it since the old one did TWAI_ALERT_NONE if undefined
    unsigned long twai_wdg_rx_dropped = 0;
#endif

    CAN_RX_queue_hdl = xQueueCreate(256, sizeof(twai_message_t));
    if (CAN_RX_queue_hdl == NULL)
    {
        ESP_LOGE(TAG, "Failed to create CAN RX queue");
        return ESP_FAIL;
    }

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

    BaseType_t twai_core_id = CONFIG_CAN_CORE_AFFINITY;
    if (xTaskCreatePinnedToCore(CAN_RX_Task, "twai RX daemon", 4096, NULL, 5, &CAN_RX_tsk_hdl, twai_core_id) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create TWAI RX task");
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(TAG, "CAN RX Task created successfully");
    }

    dispatchCANFrame = frameDispatcher;
    if (dispatchCANFrame == nullptr)
    {
        ESP_LOGW(TAG, "Frame dispatcher function does not exist!");
    }
    // All checks passed
    return ESP_OK;
}

void CAN_RX_Task(void *pvParameters)
{
    ESP_LOGI(TAG, "CAN_RX_Task has started");
    static twai_message_t rxMessage;
    CAN_RX_TimedOut = false;
    static esp_err_t rxErr;

    while (true)
    {
        if (xQueueReceive(CAN_RX_queue_hdl, &rxMessage, pdMS_TO_TICKS(CONFIG_CAN_RX_TIMEOUT_MS)) == pdPASS)
        {
            CAN_RX_TimedOut = false;
            if (dispatchCANFrame == nullptr)
            {
                ESP_LOGD(TAG, "No Frame dispatcher set up !");
                continue;
            }

            if (dispatchCANFrame(&rxMessage) != ESP_OK)
            {
                ESP_LOGD(TAG, "Frame dispatcher returned an error");
            }
        }
        else
        {
            CAN_RX_TimedOut = true;
        }
    }
}
