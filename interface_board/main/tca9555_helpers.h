#pragma once
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <tca95x5.h>
#include "esp_timer.h" // Add this include

#ifdef TAG
#undef TAG
#endif
#define TAG "TCA9555_HELPER"

static i2c_dev_t tca_slave;

// Declare Mutex here to protect raw_isr with Semaphore
SemaphoreHandle_t exp_act_hilo_semaphore;
static TaskHandle_t exp_act_hilo_proc_task_hdl = NULL;

static volatile int64_t last_int_time = 0; // microseconds to do debouncing

#if CONFIG_USE_EXPANDER_INTERRUPT == true

/// @brief Interrupt function that yields to the processor in case of INT activation
/// @param arg 
/// @return 
static void IRAM_ATTR tca_int_handler(void *arg)
{
    // Get microseconds
    int64_t now = esp_timer_get_time();
    // Ignore if too fast
    if (now - last_int_time > CONFIG_INTERRUPT_DEBOUNCE_US)
    {
        last_int_time = now;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (exp_act_hilo_proc_task_hdl != NULL)
        {
            // Notify main processor task from ISR
            // vTaskNotifyGiveFromISR(exp_act_hilo_proc_task_hdl, &xHigherPriorityTaskWoken);
            xTaskNotifyFromISR(exp_act_hilo_proc_task_hdl,1,eSetValueWithoutOverwrite,&xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken == pdTRUE)
            {
                portYIELD_FROM_ISR();
            }
        }
    }
}

#endif

/// @brief Initialises the expander and sets up the IRQ 
/// @return ESP_OK for successful initialization, ESP_FAIL otherwise
esp_err_t initialize_io_expanders()
{
    // Semaphore to protect the raw reading from the expander
    exp_act_hilo_semaphore = xSemaphoreCreateBinary();
    if (exp_act_hilo_semaphore == NULL)
    {
        ESP_LOGE(TAG, "Could not create active high/low semaphore.");
    }
    xSemaphoreGive(exp_act_hilo_semaphore);

#if CONFIG_USE_EXPANDER_INTERRUPT == true
    ESP_LOGI(TAG,"Assigning GPIO %u to expander interrupt",(uint8_t)CONFIG_EXP_INT_PIN);
    esp_err_t ret = ESP_OK;
    // Setting the interrupt on Pin (gpio_num_t)CONFIG_EXP_INT_PIN
    ret = gpio_set_direction((gpio_num_t)CONFIG_EXP_INT_PIN, GPIO_MODE_INPUT);
    ret = gpio_set_pull_mode((gpio_num_t)CONFIG_EXP_INT_PIN, GPIO_PULLUP_ONLY);
    ret = gpio_pullup_en((gpio_num_t)CONFIG_EXP_INT_PIN);
    ret = gpio_set_intr_type((gpio_num_t)CONFIG_EXP_INT_PIN, GPIO_INTR_NEGEDGE);
    ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    ret = gpio_isr_handler_add((gpio_num_t)CONFIG_EXP_INT_PIN, tca_int_handler, NULL);

    if(ret != ESP_OK)
    {
        ESP_LOGW(TAG,"Could not set up interrupt on GPIO correctly, continuing");
    }
#endif

    if (tca95x5_init_desc(&tca_slave, CONFIG_PRIMARY_IO_EXPANDER_ADDRESS, (i2c_port_t)0, (gpio_num_t)CONFIG_SDA_PIN, (gpio_num_t)CONFIG_SCL_PIN) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize IO Expander");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "IO Expander initialized OK");
    tca95x5_port_set_mode(&tca_slave, 0xFFFF);
    uint16_t raw;
    tca95x5_port_read(&tca_slave, &raw);
    ESP_LOGI(TAG, "Expander value : %02X", raw);

    return ESP_OK;
}