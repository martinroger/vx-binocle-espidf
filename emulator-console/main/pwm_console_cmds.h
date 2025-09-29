#pragma once
#include "pwm_gen_helpers.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "coefficients.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "PWM_GEN_CMDS"

// Set both duty cycle and frequency for a given channel
static struct
{
    struct arg_int *channel;
    struct arg_dbl *duty;
    struct arg_int *frequency;
    struct arg_end *end;
} channel_args;

static int set_channel_duty_freq(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&channel_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, channel_args.end, argv[0]);
        return 1;
    }
    assert(channel_args.channel->count == 1);
    assert(channel_args.duty->count == 1);
    assert(channel_args.frequency->count == 1);

    const ledc_channel_t target_channel = (ledc_channel_t)(channel_args.channel->ival[0]);
    const double target_duty = (channel_args.duty->dval[0]);
    const uint32_t target_frequency = (uint32_t)(channel_args.frequency->ival[0]);

    // If target duty is 0pc or target frequency is less than minimum Hz, pause the channel
    if (target_frequency < CONFIG_RPM_PWM_BASE_FREQ_HZ || target_frequency < CONFIG_SPEED_PWM_BASE_FREQ_HZ)
    {
        printf("Target duty or frequency is too low, pausing channel\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = false;
        printf("Channel paused.\n");
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
        printf("Resuming paused channel...\n");
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f\n", target_duty);
        return 1;
    }
    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }
    // uint32_t actual_duty = (100*ledc_get_duty(LEDC_LOW_SPEED_MODE,target_channel)) / ((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]);
    vTaskDelay(pdMS_TO_TICKS(100));
    double actual_duty = 100.0 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    printf("Channel %lu set to actual duty %.2f pc at frequency %lu\n", (uint32_t)target_channel, actual_duty, actual_freq);
    return 0;
}

static void register_set_channel_duty_freq(void)
{
    channel_args.channel = arg_int1(NULL, NULL, "<chan>", "LEDC Channel number");
    channel_args.duty = arg_dbl1(NULL, NULL, "<d>", "Duty cycle, percentile");
    channel_args.frequency = arg_int1(NULL, NULL, "<f>", "Frequency, Hz");
    channel_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "setChannelDutyFreq",
        .help = "Set the percentile duty and frequency for a specific channel",
        .hint = NULL,
        .func = &set_channel_duty_freq,
        .argtable = &channel_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// Set only duty cycle for a channel
static struct
{
    struct arg_int *channel;
    struct arg_dbl *duty;
    struct arg_end *end;
} channel_duty_args;

static int set_channel_duty(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&channel_duty_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, channel_duty_args.end, argv[0]);
        return 1;
    }
    assert(channel_duty_args.channel->count == 1);
    assert(channel_duty_args.duty->count == 1);

    const ledc_channel_t target_channel = (ledc_channel_t)(channel_duty_args.channel->ival[0]);
    const double target_duty = (channel_duty_args.duty->dval[0]);

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (target_duty > 100 || target_duty < 0)
    {
        printf("Invalid duty cycle, aborting.\n");
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f\n", target_duty);
        return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    double actual_duty = 100.0 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    // uint32_t actual_duty = (100*ledc_get_duty(LEDC_LOW_SPEED_MODE,target_channel)) / ((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    printf("Channel %lu set to actual duty %.2f pc at frequency %lu\n", (uint32_t)target_channel, actual_duty, actual_freq);

    return 0;
}

static void register_set_channel_duty(void)
{
    channel_duty_args.channel = arg_int1(NULL, NULL, "<chan>", "LEDC Channel number");
    channel_duty_args.duty = arg_dbl1(NULL, NULL, "<d>", "Duty cycle, percentile");
    channel_duty_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "setChannelDuty",
        .help = "Set the percentile duty for a specific channel",
        .hint = NULL,
        .func = &set_channel_duty,
        .argtable = &channel_duty_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// Set frequency for a given channel
static struct
{
    struct arg_int *channel;
    struct arg_int *frequency;
    struct arg_end *end;
} channel_freq_args;

static int set_channel_freq(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&channel_freq_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, channel_freq_args.end, argv[0]);
        return 1;
    }
    assert(channel_freq_args.channel->count == 1);
    assert(channel_freq_args.frequency->count == 1);

    const ledc_channel_t target_channel = (ledc_channel_t)(channel_freq_args.channel->ival[0]);
    const uint32_t target_frequency = (uint32_t)(channel_freq_args.frequency->ival[0]);

    if (target_frequency < 3)
    {
        printf("Target frequency below 3Hz, pausing channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    printf("Channel %lu set to  frequency %lu\n", (uint32_t)target_channel, actual_freq);

    return 0;
}

static void register_set_channel_freq(void)
{
    channel_freq_args.channel = arg_int1(NULL, NULL, "<chan>", "LEDC Channel number");
    channel_freq_args.frequency = arg_int1(NULL, NULL, "<f>", "Frequency, Hz");
    channel_freq_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "setChannelFreq",
        .help = "Set the frequency for a specific channel",
        .hint = NULL,
        .func = &set_channel_freq,
        .argtable = &channel_freq_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// PWM generators status

static int getChannelsInfo(int argc, char **argv)
{
    for (int i = 0; i < 5; i++)
    {
        double actual_duty = (100.0 * ledc_get_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(i))) / (((uint32_t)1 << (i < 4 ? duty_resolutions_bit[i] : duty_resolutions_bit[i - 1])) - 1);
        uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)(i < 4 ? i : i - 1));
        printf("Channel %u set to actual duty %.2f pc at frequency %lu, %s\n", i, actual_duty, actual_freq, active_timers[(i < 4 ? i : i - 1)] ? "Active" : "Paused");
    }
    return 0;
}

static void register_getChannelsInfo()
{
    const esp_console_cmd_t cmd = {
        .command = "getChannelsInfo",
        .help = "Get LEDC channel metrics",
        .hint = NULL,
        .func = &getChannelsInfo,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// Coolant temperature SET function

static struct
{
    struct arg_dbl *temperature;
    struct arg_end *end;
} setCoolant_args;

static int setCoolant(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&setCoolant_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, setCoolant_args.end, argv[0]);
        return 1;
    }
    assert(setCoolant_args.temperature->count == 1);

    const ledc_channel_t target_channel = LEDC_CHANNEL_0;
    const uint32_t target_frequency = 100;

    double target_temperature = (setCoolant_args.temperature->dval[0]);

    // Run checks on the issued temperature value
    if (target_temperature < 70)
    {
        printf("Target temperature is below 70°C, clamping to minimum value.\n");
        target_temperature = 70.0;
    }
    else if (target_temperature > 130)
    {
        printf("Target temperature is over 130°C, clamping to maximum value.\n");
        target_temperature = 130.0;
    }

    double target_duty = (double)((target_temperature)*COEFF_COOLANT_DEGC_TO_DUTY_M + COEFF_COOLANT_DEGC_TO_DUTY_P);
    printf("Calculated target duty: %.2f pc\n", target_duty);

    if (target_duty > 100 || target_duty < 0)
    {
        printf("Target duty is invalid, aborting.\n");
        return 1;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused coolant channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f\n", target_duty);
        return 1;
    }
    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    double actual_duty = (100.0 * ledc_get_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)(target_channel))) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    // uint32_t actual_duty = (100*ledc_get_duty(LEDC_LOW_SPEED_MODE,target_channel)) / ((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]);
    double actual_temperature = COEFF_DUTY_TO_COOLANT_DEGC_M * actual_duty + COEFF_DUTY_TO_COOLANT_DEGC_P;
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    printf("Coolant channel %lu set to target duty %.2f pc at frequency %luHz, actual temperature %.2f\n", (uint32_t)target_channel, actual_duty, actual_freq, actual_temperature);

    return 0;
}

static void register_setCoolant(void)
{
    setCoolant_args.temperature = arg_dbl1(NULL, NULL, "<temperature>", "Integral temperature in °C");
    setCoolant_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "setCoolant",
        .help = "Set the temperature to the target temperature",
        .hint = NULL,
        .func = &setCoolant,
        .argtable = &setCoolant_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// RPM SET function
static struct
{
    struct arg_int *rpm;
    struct arg_end *end;
} setRPM_args, chgRPM_args;

static uint32_t current_rpm = 0;

static int setRPM(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&setRPM_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, setRPM_args.end, argv[0]);
        return 1;
    }
    assert(setRPM_args.rpm->count == 1);

    const ledc_channel_t target_channel = LEDC_CHANNEL_1;
    const double target_duty = 33;

    uint32_t target_rpm = (uint32_t)(setRPM_args.rpm->ival[0]);

    // Run checks on the issued rpm value
    if (target_rpm < 250 && target_rpm != 0)
    {
        printf("Target RPM is below 250, clamping to minimum value.\n");
        target_rpm = 250;
    }
    else if (target_rpm > 9000)
    {
        printf("Target RPM is over 9000, clamping to maximum value.\n");
        target_rpm = 9000;
    }

    uint32_t target_frequency = (uint32_t)((float)(target_rpm)*COEFF_RPM_TO_FREQ_M + COEFF_RPM_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing RPM channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1);
        active_timers[1] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused RPM channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    uint32_t actual_RPM = (uint32_t)(COEFF_FREQ_TO_RPM_M * actual_freq + COEFF_FREQ_TO_RPM_P);
    printf("RPM channel %lu set to target duty %.2f pc at actual frequency %lu Hz, actual RPM %lu\n", (uint32_t)target_channel, target_duty, actual_freq, actual_RPM);
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    current_rpm = actual_RPM;
    return 0;
}

static int incRPM(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&chgRPM_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, chgRPM_args.end, argv[0]);
        return 1;
    }
    assert(chgRPM_args.rpm->count < 2);

    const ledc_channel_t target_channel = LEDC_CHANNEL_1;
    const double target_duty = 33;

    // Calculate the target RPM
    uint32_t target_rpm = 0;
    if (chgRPM_args.rpm->count == 1)
    {
        target_rpm = current_rpm + (uint32_t)(chgRPM_args.rpm->ival[0]);
    }
    else
    {
        target_rpm = current_rpm + 100;
    }

    // Run checks on the issued rpm value
    if (target_rpm < 250 && target_rpm != 0)
    {
        printf("Target RPM is below 250, clamping to minimum value.\n");
        target_rpm = 250;
    }
    else if (target_rpm > 9000)
    {
        printf("Target RPM is over 9000, clamping to maximum value.\n");
        target_rpm = 9000;
    }

    uint32_t target_frequency = (uint32_t)((float)(target_rpm)*COEFF_RPM_TO_FREQ_M + COEFF_RPM_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing RPM channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1);
        active_timers[1] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused RPM channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    uint32_t actual_RPM = (uint32_t)(COEFF_FREQ_TO_RPM_M * actual_freq + COEFF_FREQ_TO_RPM_P);
    printf("RPM channel %lu set to target duty %.2f pc at actual frequency %lu Hz, actual RPM %lu\n", (uint32_t)target_channel, target_duty, actual_freq, actual_RPM);
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    current_rpm = actual_RPM;
    return 0;
}

static int decRPM(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&chgRPM_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, chgRPM_args.end, argv[0]);
        return 1;
    }
    assert(chgRPM_args.rpm->count < 1);

    const ledc_channel_t target_channel = LEDC_CHANNEL_1;
    const double target_duty = 33;

    // Calculate the target RPM
    uint32_t target_rpm = 0;
    if (chgRPM_args.rpm->count < 2)
    {
        target_rpm = current_rpm - (uint32_t)(chgRPM_args.rpm->ival[0]);
    }
    else
    {
        target_rpm = current_rpm - 100;
    }

    // Run checks on the issued rpm value
    if (target_rpm < 250 && target_rpm > 0)
    {
        printf("Target RPM is below 250, clamping to minimum value.\n");
        target_rpm = 250;
    }
    else if (target_rpm > 9000)
    {
        printf("Target RPM is over 9000, clamping to maximum value.\n");
        target_rpm = 9000;
    }

    uint32_t target_frequency = (uint32_t)((float)(target_rpm)*COEFF_RPM_TO_FREQ_M + COEFF_RPM_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing RPM channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1);
        active_timers[1] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused RPM channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    uint32_t actual_RPM = (uint32_t)(COEFF_FREQ_TO_RPM_M * actual_freq + COEFF_FREQ_TO_RPM_P);
    printf("RPM channel %lu set to target duty %.2f pc at actual frequency %lu Hz, actual RPM %lu\n", (uint32_t)target_channel, target_duty, actual_freq, actual_RPM);
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    current_rpm = actual_RPM;
    return 0;
}

static void register_setRPM(void)
{
    setRPM_args.rpm = arg_int1(NULL, NULL, "<rpm>", "Revolutions per minute");
    setRPM_args.end = arg_end(3);

    chgRPM_args.rpm = arg_int0(NULL, NULL, "<rpm_delta>", "Amount by which to change the RPM target");
    chgRPM_args.end = arg_end(3);

    const esp_console_cmd_t setCmd = {
        .command = "setRPM",
        .help = "Set RPM value (beware of quantization)",
        .hint = NULL,
        .func = &setRPM,
        .argtable = &setRPM_args};

    const esp_console_cmd_t incCmd = {
        .command = "incRPM",
        .help = "Increase RPM value (beware of quantization)",
        .hint = NULL,
        .func = &incRPM,
        .argtable = &chgRPM_args};

    const esp_console_cmd_t decCmd = {
        .command = "decRPM",
        .help = "Decrease RPM value (beware of quantization)",
        .hint = NULL,
        .func = &decRPM,
        .argtable = &chgRPM_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&setCmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&incCmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&decCmd));
}

// Set Speed function
static struct
{
    struct arg_dbl *speed;
    struct arg_end *end;
} setSpeed_args, chgSpeed_args;

static double current_speed_kph = 0.0;
static double current_speed_mph = 0.0;

static int setSpeedKPH(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&setSpeed_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, setSpeed_args.end, argv[0]);
        return 1;
    }
    assert(setSpeed_args.speed->count == 1);

    const ledc_channel_t target_channel = LEDC_CHANNEL_2;
    const double target_duty = 33;

    double target_speed = (setSpeed_args.speed->dval[0]);

    // Run checks on the issued speed value
    if (target_speed > 300)
    {
        printf("Target Speed is over 300, clamping to maximum value.\n");
        target_speed = 300;
    }
    if (target_speed < 0)
    {
        printf("Invalid target speed, aborting\n");
        return 1;
    }

    uint32_t target_frequency = (uint32_t)(target_speed * COEFF_SPEED_KPH_TO_FREQ_M + COEFF_SPEED_KPH_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing speed channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_2);
        active_timers[2] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused speed channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    double actual_speed = (COEFF_FREQ_TO_SPEED_KPH_M * (double)actual_freq + COEFF_FREQ_TO_SPEED_KPH_P);
    printf("Speed channel %lu set to target duty %.2f pc at actual frequency %lu, expected speed %.2f\n", (uint32_t)target_channel, target_duty, actual_freq, actual_speed);
    current_speed_kph = actual_speed;
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    return 0;
}

static int incSpeedKPH(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&chgSpeed_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, chgSpeed_args.end, argv[0]);
        return 1;
    }
    assert(chgSpeed_args.speed->count < 2);

    const ledc_channel_t target_channel = LEDC_CHANNEL_2;
    const double target_duty = 33;

    double target_speed = 0.0;
    if (chgSpeed_args.speed->count > 0)
    {
        target_speed = current_speed_kph + (chgSpeed_args.speed->dval[0]);
    }
    else
    {
        target_speed = current_speed_kph + 5;
    }

    // Run checks on the issued speed value
    if (target_speed > 300)
    {
        printf("Target Speed is over 300, clamping to maximum value.\n");
        target_speed = 300;
    }
    if (target_speed < 0)
    {
        printf("Negative speed, clamping to 0\n");
        target_speed = 0;
    }

    uint32_t target_frequency = (uint32_t)(target_speed * COEFF_SPEED_KPH_TO_FREQ_M + COEFF_SPEED_KPH_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing speed channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_2);
        active_timers[2] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused speed channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    double actual_speed = (COEFF_FREQ_TO_SPEED_KPH_M * (double)actual_freq + COEFF_FREQ_TO_SPEED_KPH_P);
    printf("Speed channel %lu set to target duty %.2f pc at actual frequency %lu, expected speed %.2f\n", (uint32_t)target_channel, target_duty, actual_freq, actual_speed);
    current_speed_kph = actual_speed;
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    return 0;
}

static int decSpeedKPH(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&chgSpeed_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, chgSpeed_args.end, argv[0]);
        return 1;
    }
    assert(chgSpeed_args.speed->count < 2);

    const ledc_channel_t target_channel = LEDC_CHANNEL_2;
    const double target_duty = 33;

    double target_speed = 0.0;
    if (chgSpeed_args.speed->count > 0)
    {
        target_speed = current_speed_kph - (chgSpeed_args.speed->dval[0]);
    }
    else
    {
        target_speed = current_speed_kph - 5;
    }

    // Run checks on the issued speed value
    if (target_speed > 300)
    {
        printf("Target Speed is over 300, clamping to maximum value.\n");
        target_speed = 300;
    }
    if (target_speed < 0)
    {
        printf("Negative speed, clamping to 0\n");
        target_speed = 0;
    }

    uint32_t target_frequency = (uint32_t)(target_speed * COEFF_SPEED_KPH_TO_FREQ_M + COEFF_SPEED_KPH_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing speed channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_2);
        active_timers[2] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused speed channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    double actual_speed = (COEFF_FREQ_TO_SPEED_KPH_M * (double)actual_freq + COEFF_FREQ_TO_SPEED_KPH_P);
    printf("Speed channel %lu set to target duty %.2f pc at actual frequency %lu, expected speed %.2f\n", (uint32_t)target_channel, target_duty, actual_freq, actual_speed);
    current_speed_kph = actual_speed;
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    return 0;
}

static void register_setSpeedKPH(void)
{
    setSpeed_args.speed = arg_dbl1(NULL, NULL, "<speed>", "Kilometers per hour");
    setSpeed_args.end = arg_end(3);

    chgSpeed_args.speed = arg_dbl0(NULL,NULL,"<speed_delta>", "KPH to add/remove");
    chgSpeed_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "setSpeedKPH",
        .help = "Set kph speed value (beware of quantization)",
        .hint = NULL,
        .func = &setSpeedKPH,
        .argtable = &setSpeed_args};

    const esp_console_cmd_t incCmd = {
        .command = "incSpeedKPH",
        .help = "Increase kph speed value (beware of quantization)",
        .hint = NULL,
        .func = &incSpeedKPH,
        .argtable = &chgSpeed_args};

    const esp_console_cmd_t decCmd = {
        .command = "decSpeedKPH",
        .help = "Decrease kph speed value (beware of quantization)",
        .hint = NULL,
        .func = &decSpeedKPH,
        .argtable = &chgSpeed_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&incCmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&decCmd));
}

static int setSpeedMPH(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&setSpeed_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, setSpeed_args.end, argv[0]);
        return 1;
    }
    assert(setSpeed_args.speed->count == 1);

    const ledc_channel_t target_channel = LEDC_CHANNEL_2;
    const double target_duty = 33;

    double target_speed = (setSpeed_args.speed->dval[0]);

    // Run checks on the issued speed value
    if (target_speed > 190)
    {
        printf("Target Speed is over 190, clamping to maximum value.\n");
        target_speed = 190;
    }
    if (target_speed < 0)
    {
        printf("Invalid target speed, aborting\n");
        return 1;
    }

    uint32_t target_frequency = (uint32_t)(target_speed * COEFF_SPEED_MPH_TO_FREQ_M + COEFF_SPEED_MPH_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing speed channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_2);
        active_timers[2] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused speed channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    double actual_speed = (COEFF_FREQ_TO_SPEED_MPH_M * (double)actual_freq + COEFF_FREQ_TO_SPEED_MPH_P);
    printf("Speed channel %lu set to target duty %.2f pc at actual frequency %lu, expected speed %.2f\n", (uint32_t)target_channel, target_duty, actual_freq, actual_speed);
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    current_speed_mph = actual_speed;
    return 0;
}

static int incSpeedMPH(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&chgSpeed_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, chgSpeed_args.end, argv[0]);
        return 1;
    }
    assert(chgSpeed_args.speed->count < 2);

    const ledc_channel_t target_channel = LEDC_CHANNEL_2;
    const double target_duty = 33;

    double target_speed = 0.0;
    if (chgSpeed_args.speed->count > 0)
    {
        target_speed = current_speed_mph + (chgSpeed_args.speed->dval[0]);
    }
    else
    {
        target_speed = current_speed_mph + 3;
    }

    // Run checks on the issued speed value
    if (target_speed > 190)
    {
        printf("Target Speed is over 190, clamping to maximum value.\n");
        target_speed = 190;
    }
    if (target_speed < 0)
    {
        printf("Negative speed, clamping to 0\n");
        target_speed = 0;
    }

    uint32_t target_frequency = (uint32_t)(target_speed * COEFF_SPEED_MPH_TO_FREQ_M + COEFF_SPEED_MPH_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing speed channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_2);
        active_timers[2] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused speed channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    double actual_speed = (COEFF_FREQ_TO_SPEED_MPH_M * (double)actual_freq + COEFF_FREQ_TO_SPEED_MPH_P);
    printf("Speed channel %lu set to target duty %.2f pc at actual frequency %lu, expected speed %.2f\n", (uint32_t)target_channel, target_duty, actual_freq, actual_speed);
    current_speed_mph = actual_speed;
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    return 0;
}

static int decSpeedMPH(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&chgSpeed_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, chgSpeed_args.end, argv[0]);
        return 1;
    }
    assert(chgSpeed_args.speed->count < 2);

    const ledc_channel_t target_channel = LEDC_CHANNEL_2;
    const double target_duty = 33;

    double target_speed = 0.0;
    if (chgSpeed_args.speed->count > 0)
    {
        target_speed = current_speed_mph - (chgSpeed_args.speed->dval[0]);
    }
    else
    {
        target_speed = current_speed_mph - 3;
    }

    // Run checks on the issued speed value
    if (target_speed > 190)
    {
        printf("Target Speed is over 190, clamping to maximum value.\n");
        target_speed = 190;
    }
    if (target_speed < 0)
    {
        printf("Negative speed, clamping to 0\n");
        target_speed = 0;
    }

    uint32_t target_frequency = (uint32_t)(target_speed * COEFF_SPEED_MPH_TO_FREQ_M + COEFF_SPEED_MPH_TO_FREQ_P);
    printf("Calculated target frequency: %lu Hz\n", target_frequency);

    if (target_frequency < 3)
    {
        printf("Target frequency is below 3Hz, pausing speed channel.\n");
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_2);
        active_timers[2] = false;
        return 0;
    }

    if (active_timers[(int)target_channel] == false)
    {
        printf("Resuming paused speed channel...\n");
        ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
        active_timers[(int)target_channel] = true;
    }

    if (change_frequency(target_channel, target_frequency) != ESP_OK)
    {
        printf("Cannot change frequency to %lu\n", target_frequency);
        return 1;
    }

    if (change_duty_cycle(target_channel, target_duty) != ESP_OK)
    {
        printf("Cannot change duty cycle to %.2f !\n", target_duty);
        return 1;
    }

    // uint32_t actual_duty = 1 + (100 * ledc_get_duty(LEDC_LOW_SPEED_MODE, target_channel)) / (((uint32_t)1 << duty_resolutions_bit[(uint32_t)target_channel]) - 1);
    uint32_t actual_freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)target_channel);
    double actual_speed = (COEFF_FREQ_TO_SPEED_MPH_M * (double)actual_freq + COEFF_FREQ_TO_SPEED_MPH_P);
    printf("Speed channel %lu set to target duty %.2f pc at actual frequency %lu, expected speed %.2f\n", (uint32_t)target_channel, target_duty, actual_freq, actual_speed);
    current_speed_mph = actual_speed;
    if (actual_freq != target_frequency)
    {
        printf("CAUTION : Artifact on achievable, target and actual are different.\n");
    }
    return 0;
}

static void register_setSpeedMPH(void)
{
    setSpeed_args.speed = arg_dbl1(NULL, NULL, "<speed>", "Miles per hour");
    setSpeed_args.end = arg_end(3);

    chgSpeed_args.speed = arg_dbl0(NULL, NULL, "<speed_delta>", "Miles per hour");
    chgSpeed_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "setSpeedMPH",
        .help = "Set mph speed value (beware of quantization)",
        .hint = NULL,
        .func = &setSpeedMPH,
        .argtable = &setSpeed_args};

    const esp_console_cmd_t incCmd = {
        .command = "incSpeedMPH",
        .help = "Increase mph speed value (beware of quantization)",
        .hint = NULL,
        .func = &incSpeedMPH,
        .argtable = &chgSpeed_args};

    const esp_console_cmd_t decCmd = {
        .command = "decSpeedMPH",
        .help = "Decrease mph speed value (beware of quantization)",
        .hint = NULL,
        .func = &decSpeedMPH,
        .argtable = &chgSpeed_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&incCmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&decCmd));
}
