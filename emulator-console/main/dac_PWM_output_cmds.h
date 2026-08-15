#pragma once
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "pwm_gen_helpers.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "DACPWM_OUT"

// CONFIG_DAC0_PIN
// CONFIG_DAC1_PIN

ledc_channel_t dac_pwm_fuel = LEDC_CHANNEL_3;
ledc_channel_t dac_pwm_lv = LEDC_CHANNEL_4;

esp_err_t initialize_dacpwm()
{

    if (set_ledc_generator(LEDC_TIMER_3, 5000, (gpio_num_t)CONFIG_DAC0_PIN, dac_pwm_fuel, 50) != ESP_OK)
    {
        ESP_LOGE(TAG, "Couldn't start fuel DAC PWM");
        return ESP_FAIL;
    }
    if (set_ledc_generator(LEDC_TIMER_3, 5000, (gpio_num_t)CONFIG_DAC1_PIN, dac_pwm_lv, 50) != ESP_OK)
    {
        ESP_LOGE(TAG, "Couldn't start fuel DAC PWM");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static struct
{
    struct arg_dbl *voltage;
    struct arg_end *end;
} setDACPWM_args;

static struct
{
    struct arg_dbl *level;
    struct arg_end *end;
} setfuelLevel_args;

static int set_fuel_v(int argc, char **argv)
{
    ledc_channel_t targetChannel = dac_pwm_fuel;
    int nerrors = arg_parse(argc, argv, (void **)&setDACPWM_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, setDACPWM_args.end, argv[0]);
        return 1;
    }
    assert(setDACPWM_args.voltage->count == 1);

    double target_voltage = 0.0;
    uint32_t target_duty = 0;

    if (setDACPWM_args.voltage->dval[0] < 0.0)
    {
        printf("Target voltage is negative, clamping to 0V\n");
        target_voltage = 0.0;
    }
    else if (setDACPWM_args.voltage->dval[0] > 3.3)
    {
        printf("Target voltage is over 3.3V, clamping to 3.3V\n");
        target_voltage = 3.3;
    }
    else
    {
        target_voltage = setDACPWM_args.voltage->dval[0];
    }


    target_duty = (uint32_t)((((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1)*(target_voltage/3.3));
    printf("Calculated target duty: %.2f pc\n", (100.0*target_duty)/(((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1));

    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, targetChannel,target_duty) != ESP_OK)
    {
        printf("Could not set duty \n");
        return 1;
    }
    if (ledc_update_duty(LEDC_LOW_SPEED_MODE, targetChannel) != ESP_OK)
    {
        printf("Could not update duty \n");
        return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("Actual target voltage: %.4f\n",(3.3*ledc_get_duty(LEDC_LOW_SPEED_MODE,targetChannel))/(((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1));
    return 0;
}

static int set_fuel_pc(int argc, char **argv)
{
    ledc_channel_t targetChannel = dac_pwm_fuel;
    int nerrors = arg_parse(argc, argv, (void **)&setfuelLevel_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, setfuelLevel_args.end, argv[0]);
        return 1;
    }
    assert(setfuelLevel_args.level->count == 1);

    double target_level_pc = 0.0;
    uint32_t target_duty = 0;

    if (setfuelLevel_args.level->dval[0] < 0.0)
    {
        printf("Target level is negative, clamping to 0\n");
        target_level_pc = 0.0;
    }
    else if (setfuelLevel_args.level->dval[0] > 100)
    {
        printf("Target level is over 100, clamping to 100\n");
        target_level_pc = 100;
    }
    else
    {
        target_level_pc = setfuelLevel_args.level->dval[0];
    }

    double target_voltage = COEFF_FUEL_PC_TO_V_M*target_level_pc+COEFF_FUEL_PC_TO_V_P;

    target_duty = (uint32_t)((((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1)*(target_voltage/3.3));
    printf("Calculated target duty: %.2f pc\n", (100.0*target_duty)/(((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1));

    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, targetChannel,target_duty) != ESP_OK)
    {
        printf("Could not set duty \n");
        return 1;
    }
    if (ledc_update_duty(LEDC_LOW_SPEED_MODE, targetChannel) != ESP_OK)
    {
        printf("Could not update duty \n");
        return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("Actual target voltage: %.4f\n",(3.3*ledc_get_duty(LEDC_LOW_SPEED_MODE,targetChannel))/(((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1));
    return 0;
}

static int set_lv_v(int argc, char **argv)
{
    ledc_channel_t targetChannel = dac_pwm_lv;
    int nerrors = arg_parse(argc, argv, (void **)&setDACPWM_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, setDACPWM_args.end, argv[0]);
        return 1;
    }
    assert(setDACPWM_args.voltage->count == 1);

    double target_voltage = 0.0;
    uint32_t target_duty = 0;

    if (setDACPWM_args.voltage->dval[0] < 0.0)
    {
        printf("Target voltage is negative, clamping to 0V\n");
        target_voltage = 0.0;
    }
    else if (setDACPWM_args.voltage->dval[0] > 15)
    {
        printf("Target voltage is over 15V, clamping to 15V\n");
        target_voltage = 15;
    }
    else
    {
        target_voltage = setDACPWM_args.voltage->dval[0];
    }

    double target_lv_voltage = COEFF_LV_TO_V_M*target_voltage+COEFF_LV_TO_V_P;

    target_duty = (uint32_t)((((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1)*(target_lv_voltage/3.3));
    printf("Calculated target duty: %.2f pc\n", (100.0*target_duty)/(((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1));

    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, targetChannel,target_duty) != ESP_OK)
    {
        printf("Could not set duty \n");
        return 1;
    }
    if (ledc_update_duty(LEDC_LOW_SPEED_MODE, targetChannel) != ESP_OK)
    {
        printf("Could not update duty \n");
        return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("Actual target voltage: %.4f\n",(3.3*ledc_get_duty(LEDC_LOW_SPEED_MODE,targetChannel))/(((uint32_t)1 << ledc_duty_resolutions_bit[(uint32_t)targetChannel]) - 1));
    return 0;
}

static void register_dac_pwms(void)
{
    setDACPWM_args.voltage = arg_dbl1(NULL,NULL,"<voltage>","Target voltage 0 - 3.3V");
    setDACPWM_args.end = arg_end(3);

    setfuelLevel_args.level = arg_dbl1(NULL,NULL,"<level>","Target level 0 - 100pc");
    setfuelLevel_args.end = arg_end(3);

    const esp_console_cmd_t cmd_fuel = {
        .command = "setFuel_V",
        .help = "Set the target voltage for the Fuel level sender",
        .hint = NULL,
        .func = &set_fuel_v,
        .argtable = &setDACPWM_args};

    const esp_console_cmd_t cmd_fuel_level = {
        .command = "setFuel_pc",
        .help = "Set the target fuel level",
        .hint = NULL,
        .func = &set_fuel_pc,
        .argtable = &setfuelLevel_args};

    const esp_console_cmd_t cmd_lv = {
        .command = "setLV_V",
        .help = "Set the target voltage for the 12V level",
        .hint = NULL,
        .func = &set_lv_v,
        .argtable = &setDACPWM_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_fuel));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_fuel_level));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_lv));
    }

