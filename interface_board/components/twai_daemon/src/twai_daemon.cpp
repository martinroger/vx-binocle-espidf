#include "twai_daemon.h"

const char *TAG = "CAN Daemon";

frameDispatcher_t *dispatchCANFrame = nullptr;

bool CAN_RX_TimedOut = false;

// Pointer to rx and dispatch task handle
TaskHandle_t CAN_RX_tsk_hdl = nullptr;
TaskHandle_t CAN_TX_tsk_hdl = nullptr;

/// @brief Queue for messages to be sent out
QueueHandle_t CAN_TX_queue_hdl = nullptr;

esp_err_t initCAN(frameDispatcher_t *frameDispatcher)
{
#ifdef TWAI_WATCHDOG
    // Set up alerts filter
    uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA |
                                TWAI_ALERT_TX_FAILED |
                                TWAI_ALERT_ERR_PASS |
                                TWAI_ALERT_BUS_ERROR |
                                TWAI_ALERT_RX_QUEUE_FULL |
                                TWAI_ALERT_ARB_LOST;
    uint32_t twai_alerts_triggered;
    twai_status_info_t twai_status;
    unsigned long twai_wdg_rx_dropped = 0;
    unsigned long twai_wdg_rx_dropped_prev = 0;
    unsigned long twai_wdg_rx_dropped_rate = 0;
#else
    uint32_t alerts_to_enable = TWAI_ALERT_NONE;
#endif

    twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = (gpio_num_t)CONFIG_CAN_TX,
        .rx_io = (gpio_num_t)CONFIG_CAN_RX,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 256,
        .rx_queue_len = 256,
        .alerts_enabled = alerts_to_enable,
        .clkout_divider = 0,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
    {
        if (twai_start() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start TWAI driver");
            return ESP_FAIL;
        }
        else
        {
            ESP_LOGI(TAG, "TWAI driver started successfully");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to install TWAI driver");
        return ESP_FAIL;
    }

    CAN_TX_queue_hdl = xQueueCreate(16, sizeof(twai_message_t));
    if (CAN_TX_queue_hdl == NULL)
    {
        ESP_LOGE(TAG, "Failed to create CAN TX queue");
        return ESP_FAIL;
    }

    BaseType_t twai_core_id = 0;
    if (xTaskCreatePinnedToCore(CAN_RX_Task, "twai RX daemon", 4096, NULL, 5, &CAN_RX_tsk_hdl, twai_core_id) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create TWAI RX task");
        return ESP_FAIL;
    }
    else if (xTaskCreatePinnedToCore(CAN_TX_Task, "twai TX daemon", 4096, NULL, 5, &CAN_TX_tsk_hdl, twai_core_id) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create TWAI TX task");
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(TAG, "CAN RX and TX Tasks created successfully");
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
        rxErr = twai_receive(&rxMessage, pdMS_TO_TICKS(CONFIG_CAN_RX_TIMEOUT_MS));
        switch (rxErr)
        {
        case ESP_OK:
        {
            CAN_RX_TimedOut = false;
            if (dispatchCANFrame == nullptr)
            {
                ESP_LOGD(TAG, "No Frame dispatcher set up !");
                break;
            }

            if (dispatchCANFrame(&rxMessage) != ESP_OK)
            {
                ESP_LOGW(TAG, "Frame dispatcher returned an error");
            }
            break;
        }
        case ESP_ERR_TIMEOUT:
        {
            CAN_RX_TimedOut = true;
            break;
        }
        default:
        {
            ESP_LOGE(__func__, "TWAI RX Task experiencing issues, pausing for 1000ms : %s",esp_err_to_name(rxErr));
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        }
        }
    }
}

void CAN_TX_Task(void *pvParameters)
{
    ESP_LOGI(TAG, "CAN_TX_Task has started");
    static twai_message_t txMessage;

    while (true)
    {
        while (xQueueReceive(CAN_TX_queue_hdl, &txMessage, pdMS_TO_TICKS(CONFIG_CAN_TX_POLLING_RATE_MS)) == pdPASS)
        {
            if (twai_transmit(&txMessage, pdMS_TO_TICKS(5)) != ESP_OK)
            {
                ESP_LOGD(TAG, "Could not TX TWAI message!");
            }
        }
        // vTaskDelay(pdMS_TO_TICKS(CAN_TX_POLL_MS));
    }
}
