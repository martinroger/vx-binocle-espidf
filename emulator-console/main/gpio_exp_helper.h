/*
GPIO expander helper functions

Defines base expander manipulation function, such as initialisation and status feedbacks
*/
#pragma once
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_io_expander.hpp>
// #include "gpio_defs.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "GPIO_EXP_HELPER"

static esp_expander::Base *expanders[3] = {nullptr};

/// @brief Initialises the two expanders (active highs and resistance mosfet controllers)
/// @return ESP_OK if both are initalised correctly, ESP_FAIL if one fails to initialise
esp_err_t initialize_expanders()
{
esp_err_t ret = ESP_OK;
    

// Primary expander is controlling the active highs and low
    expanders[0] = new esp_expander::TCA95XX_16BIT(CONFIG_SCL_PIN,CONFIG_SDA_PIN,CONFIG_PRIMARY_IO_EXPANDER_ADDRESS);
    if(expanders[0]->init() == false)
    {
        ESP_LOGE(TAG,"Failed to initialize primary IO expander");
        ret = ESP_FAIL;
    }
    else if(expanders[0]->begin() == false)
    {
        ESP_LOGE(TAG,"Failed to begin the primary IO expander");
        ret = ESP_FAIL;
    }
    else
    {
        expanders[0]->multiPinMode(0xFFFF,OUTPUT);
        expanders[0]->multiDigitalWrite(0xFFFF,LOW);
        expanders[0]->multiDigitalWrite(0xC120,HIGH);
        expanders[0]->printStatus();
    }
    

// Second expander is reserved for emulating resistor loads
    expanders[1] = new esp_expander::TCA95XX_16BIT(I2C_NUM_0,CONFIG_PRIMARY_IO_EXPANDER_ADDRESS+1);
    if(expanders[1]->init() == false)
    {
        ESP_LOGE(TAG,"Failed to initialize secondary IO expander");
        ret = ESP_FAIL;
    }
    else if(expanders[1]->begin() == false)
    {
        ESP_LOGE(TAG,"Failed to begin the secondary IO expander");
        ret = ESP_FAIL;
    }
    else
    {
        expanders[1]->multiPinMode(0xFFFF,OUTPUT);
        expanders[1]->multiDigitalWrite(0xFFFF,LOW);
        expanders[1]->printStatus();
    }

    return ret;
}
