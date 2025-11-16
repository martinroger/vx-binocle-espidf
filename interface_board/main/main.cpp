#include <stdio.h>
// #include <string.h> // Required for version and SHA extraction
// #include "gpio_defs.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/mcpwm_prelude.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
// #include "esp_app_desc.h"
#include "version_parser.h"
#include "mcpwm_capture_helpers.h"
#include "coefficients.h"

#include "nvs_storage.h"
#include "odometer.h"

#include "twai_daemon.h"
#include "binocan.h"

#include "adc_processor.h"
#include "active_hi_low_processor.h"
#include "sma_filter.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "MAIN"

#pragma region Global variables
// Should be extended to more ?
struct board_ST
{
    bool EN_HI_R_SENSE_ST = false;
    bool EN_5_V_ST = false;
    bool EN_5_V_AUX_ST = false;
    uint8_t internal_ST = 0x00;
    bool expander_ST = false;
    bool adc_ST = false;
    uint8_t mcu_temperature = 0;
    bool LD_CHECK_ALIVE_ST = false;
    bool RD_CHECK_ALIVE_ST = false;
    bool lowFuel = false;
    bool overTemp = false;
    parsed_app_meta_t *app_metadata;
} interface_board_st;

// const esp_app_desc_t *app_metadata;

#pragma endregion

#pragma region GPIO interrupts

// Button ISR. Starts a timer on falling edge, mutes itself and resets trip_m after 500ms, whence the timer reenables the ISR.
// Timer used to confirm long press / debounce and perform trip reset
static TimerHandle_t trip_reset_timer = NULL;

// Timer callback runs in timer task context (not ISR)
static void trip_reset_timer_cb(TimerHandle_t xTimer)
{
    // If button still low, reset trip
    if (gpio_get_level((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO) == 0)
    {
        ESP_LOGI(TAG, "Button long-press detected: resetting trip_m to 0");
        trip_m = 0;
        if (trip_set(trip_m) != ESP_OK)
        {
            ESP_LOGW(TAG, "Could not set trip_m in NVS");
            interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        }
    }

    // Re-enable GPIO interrupt so future presses are detected
    gpio_intr_enable((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO);
}

// ISR: disable further interrupts and start the confirmation timer
static void button_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Disable further interrupts on this pin while we debounce / confirm
    gpio_intr_disable((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO);

    if (trip_reset_timer != NULL)
    {
        // Start one-shot timer for 500ms from ISR
        if (xTimerStartFromISR(trip_reset_timer, &xHigherPriorityTaskWoken) != pdPASS)
        {
            ESP_EARLY_LOGW(TAG, "Could not start trip reset timer from ISR");
            // re-enable interrupt to avoid missing future presses
            gpio_intr_enable((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#pragma endregion

#pragma region 5V management
// 5V control functions
esp_err_t init_5V_ctrl(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "Initializing 5V and 5V Aux control pins");
    ret = gpio_set_direction((gpio_num_t)CONFIG_5V_EN_GPIO, GPIO_MODE_INPUT_OUTPUT);
    ret = gpio_set_direction((gpio_num_t)CONFIG_5V_AUX_EN_GPIO, GPIO_MODE_INPUT_OUTPUT);
    ret = gpio_set_pull_mode((gpio_num_t)CONFIG_5V_EN_GPIO, GPIO_PULLDOWN_ONLY);
    ret = gpio_set_pull_mode((gpio_num_t)CONFIG_5V_AUX_EN_GPIO, GPIO_PULLDOWN_ONLY);
    ret = gpio_pulldown_en((gpio_num_t)CONFIG_5V_EN_GPIO);
    ret = gpio_pulldown_en((gpio_num_t)CONFIG_5V_AUX_EN_GPIO);
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_EN_GPIO, 0);
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_AUX_EN_GPIO, 0);
    interface_board_st.EN_5_V_AUX_ST = false;
    interface_board_st.EN_5_V_ST = false;
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Issue setting up control pins for 5V outputs");
    }
    return ret;
}

esp_err_t enable_5V(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGD(TAG, "Enabling 5V output.");
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_EN_GPIO, 1);
    if (ret != ESP_OK)
    {
        interface_board_st.EN_5_V_ST = false;
        ESP_LOGW(TAG, "Could not enable 5V output, continuing.");
    }
    else
    {
        interface_board_st.EN_5_V_ST = true;
    }
    return ret;
}

esp_err_t enable_5V_AUX(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGD(TAG, "Enabling 5V auxiliary output.");
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_AUX_EN_GPIO, 1);
    if (ret != ESP_OK)
    {
        interface_board_st.EN_5_V_AUX_ST = false;
        ESP_LOGW(TAG, "Could not enable 5V auxiliary output, continuing.");
    }
    else
    {
        interface_board_st.EN_5_V_AUX_ST = true;
    }
    return ret;
}

esp_err_t disable_5V(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGD(TAG, "Disabling 5V output.");
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_EN_GPIO, 0);
    if (ret != ESP_OK)
    {
        interface_board_st.EN_5_V_ST = true;
        ESP_LOGW(TAG, "Could not disable 5V output, continuing.");
    }
    else
    {
        interface_board_st.EN_5_V_ST = false;
    }
    return ret;
}

esp_err_t disable_5V_AUX(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGD(TAG, "Disabling 5V auxiliary output.");
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_AUX_EN_GPIO, 0);
    if (ret != ESP_OK)
    {
        interface_board_st.EN_5_V_AUX_ST = true;
        ESP_LOGW(TAG, "Could not disable 5V auxiliary output, continuing.");
    }
    else
    {
        interface_board_st.EN_5_V_AUX_ST = false;
    }
    return ret;
}
#pragma endregion

#pragma region MCPWM declarations
// PWM stats structures for coolant, rpm and speed captures
static volatile pwm_info_t pwm_cap_coolant, pwm_cap_rpm, pwm_cap_speed = {.pos_edge_ts = 0, .prev_pos_edge_ts = 0, .period_ticks = 0, .neg_edge_ts = 0, .deltaT = 0};

// Capture channels
mcpwm_cap_channel_handle_t cap_chan_coolant = NULL;
mcpwm_cap_channel_handle_t cap_chan_rpm = NULL;
mcpwm_cap_channel_handle_t cap_chan_speed = NULL;
#pragma endregion

#pragma region FreeRTOS tasks for CAN packaging
// FreeRTOS handles
TaskHandle_t base_slow_metrics_PKG_hdl;
TaskHandle_t base_fast_metrics_PKG_hdl;
// exp_act_hilo_proc_task_hdl for active high lows is already defined in tca9555_helpers because of interrupt wrapping
TaskHandle_t base_odometer_PKG_hdl;
TaskHandle_t interface_brd_ST_PKG_hdl;
TaskHandle_t interface_brd_version_PKG_hdl;

/// @brief Packaging task for base_slow_metrics
/// @param pvParameters
void base_slow_metrics_PKG(void *pvParameters)
{
    // SMA-ed slower metrics :
    // coolant_temp
    // fuel_level_pc
    // lv_voltage_v

    binocan_base_slow_metrics_t binocan_base_slow_metrics;
    binocan_base_slow_metrics_init(&binocan_base_slow_metrics);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_BASE_SLOW_METRICS_FRAME_ID,
        .data_length_code = BINOCAN_BASE_SLOW_METRICS_LENGTH};

    while (true)
    {
        // SMAs should already be protected, and some of the MCPWM logic can be brought in here.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_SLOW_METRICS_PKG_RATE_MS)); // Waits for notification or one cyclic for frame message
        compute_freq_dut(&pwm_cap_coolant);
        float coolant_degC = 100.0 * pwm_cap_coolant.duty_cycle * COEFF_DUTY_TO_COOLANT_DEGC_M + COEFF_DUTY_TO_COOLANT_DEGC_P;
        if (coolant_degC < 70)
            coolant_degC = 70;
        if (coolant_degC > 130)
            coolant_degC = 130;
        interface_board_st.overTemp = (coolant_degC >= 106 ? true : false);
        // Comment in for debug
        ESP_LOGD(TAG, "Coolant: %.2f - %.1f - %.2f", pwm_cap_coolant.frequency, pwm_cap_coolant.duty_cycle, coolant_degC);

        float fuel_level_raw = sma_get_avg(adc_channels[0].sma);
        float fuel_level_v = fuel_level_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float fuel_level_pc = COEFF_FUEL_V_TO_PC_M * fuel_level_v + COEFF_FUEL_V_TO_PC_P;
        if (fuel_level_pc > 100.0)
            fuel_level_pc = 100;
        if (fuel_level_pc < 0)
            fuel_level_pc = 0;
        interface_board_st.lowFuel = (fuel_level_pc < 20 ? true : false);

        float lv_raw = sma_get_avg(adc_channels[1].sma);
        float lv_raw_v = lv_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float lv_v = lv_raw_v * COEFF_V_TO_LV_M + COEFF_V_TO_LV_P;

        // Comment in for debug
        ESP_LOGD(TAG, "Fuel : %.2f - %.2fV - %.2fpc\t|\t 12V: %.2f - %.2fV - %.2fV", fuel_level_raw, fuel_level_v, fuel_level_pc, lv_raw, lv_raw_v, lv_v);

        binocan_base_slow_metrics.coolant_temp = binocan_base_slow_metrics_coolant_temp_encode(coolant_degC);
        binocan_base_slow_metrics.fuel_level_pc = binocan_base_slow_metrics_fuel_level_pc_encode(fuel_level_pc);
        binocan_base_slow_metrics.lv_voltage_v = binocan_base_slow_metrics_lv_voltage_v_encode(lv_v);
        binocan_base_slow_metrics_pack(tx_msg.data, &binocan_base_slow_metrics, BINOCAN_BASE_SLOW_METRICS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue slow metrics message in queue");
        }
    }
}

/// @brief Packaging task for base_fast_metrics
/// @param pvParameters
void base_fast_metrics_PKG(void *pvParameters)
{
    // Only faster metrics
    // speed_kph
    // rpm

    binocan_base_fast_metrics_t binocan_base_fast_metrics;
    binocan_base_fast_metrics_init(&binocan_base_fast_metrics);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_BASE_FAST_METRICS_FRAME_ID,
        .data_length_code = BINOCAN_BASE_FAST_METRICS_LENGTH};

    while (true)
    {
        // Transport some of the MCPWM logic in there
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_FAST_METRICS_PKG_RATE_MS));
        compute_freq_dut(&pwm_cap_rpm);
        compute_freq_dut(&pwm_cap_speed);
        float rpm = COEFF_FREQ_TO_RPM_M * pwm_cap_rpm.frequency + COEFF_FREQ_TO_RPM_P;
        float speed = COEFF_FREQ_TO_SPEED_KPH_M * pwm_cap_speed.frequency + COEFF_FREQ_TO_SPEED_KPH_P;
        ESP_LOGD(TAG, "RPM : %.2f - %.1f - %.2f Speed: %.2f - %.1f - %.2f", pwm_cap_rpm.frequency, pwm_cap_rpm.duty_cycle, rpm, pwm_cap_speed.frequency, pwm_cap_speed.duty_cycle, speed);

        binocan_base_fast_metrics.rpm = binocan_base_fast_metrics_rpm_encode(rpm);
        binocan_base_fast_metrics.speed_kph = binocan_base_fast_metrics_speed_kph_encode(speed);
        binocan_base_fast_metrics_pack(tx_msg.data, &binocan_base_fast_metrics, BINOCAN_BASE_FAST_METRICS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue fast metrics message in queue");
        }
    }
}

/// @brief Packaging task for base_active_hi_lo
/// @param pvParameters
void base_active_hilo_PKG(void *pvParameters)
{
    // Will need to be using notifications and delay
    // Essentially direct from expander + a couple virtual telltales
    uint16_t raw;

    binocan_base_active_hi_lo_t binocan_base_active_hi_lo;
    binocan_base_active_hi_lo_init(&binocan_base_active_hi_lo);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_BASE_ACTIVE_HI_LO_FRAME_ID,
        .data_length_code = BINOCAN_BASE_ACTIVE_HI_LO_LENGTH};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_ACTIVE_HILO_PKG_RATE_MS));
        if (tca95x5_port_read(&tca_slave, &raw) != ESP_OK)
        {
            ESP_LOGE(TAG, "Impossible to fetch register from expander");
            interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        }
        else
        {

            if (xSemaphoreTake(exp_act_hilo_semaphore, pdMS_TO_TICKS(1)) == pdTRUE)
            {
                active_hi_lo_grp.AL_brake_low = read_bitmask(raw, EXP_IO_0_BITMASK);
                active_hi_lo_grp.AL_parking_brake = read_bitmask(raw, EXP_IO_1_BITMASK);
                active_hi_lo_grp.AH_ignition = !read_bitmask(raw, EXP_IO_2_BITMASK); // Inverted
                active_hi_lo_grp.AL_oil_pressure = read_bitmask(raw, EXP_IO_3_BITMASK);
                active_hi_lo_grp.AL_airbag = read_bitmask(raw, EXP_IO_4_BITMASK);
                active_hi_lo_grp.AL_CEL = read_bitmask(raw, EXP_IO_5_BITMASK);
                active_hi_lo_grp.AH_backlight = !read_bitmask(raw, EXP_IO_6_BITMASK); // Inverted
                active_hi_lo_grp.AH_alarm = !read_bitmask(raw, EXP_IO_7_BITMASK);     // Inverted
                active_hi_lo_grp.AH_hi_beams = !read_bitmask(raw, EXP_IO_8_BITMASK);  // Inverted
                active_hi_lo_grp.AL_button = read_bitmask(raw, EXP_IO_9_BITMASK);
                active_hi_lo_grp.AL_alternator = read_bitmask(raw, EXP_IO_10_BITMASK);
                active_hi_lo_grp.AL_coolant_low = !read_bitmask(raw, EXP_IO_11_BITMASK); // Inverted
                active_hi_lo_grp.AH_left_turn = !read_bitmask(raw, EXP_IO_12_BITMASK);   // Inverted
                active_hi_lo_grp.AL_door = read_bitmask(raw, EXP_IO_13_BITMASK);
                active_hi_lo_grp.AH_right_turn = !read_bitmask(raw, EXP_IO_14_BITMASK); // Inverted
                active_hi_lo_grp.AL_ABS = read_bitmask(raw, EXP_IO_15_BITMASK);

                binocan_base_active_hi_lo.ignition_ah_st = binocan_base_active_hi_lo_ignition_ah_st_encode(active_hi_lo_grp.AH_ignition);
                binocan_base_active_hi_lo.hi_beams_ah_tt = binocan_base_active_hi_lo_hi_beams_ah_tt_encode(active_hi_lo_grp.AH_hi_beams);
                binocan_base_active_hi_lo.alternator_al_tt = binocan_base_active_hi_lo_alternator_al_tt_encode(active_hi_lo_grp.AL_alternator);
                binocan_base_active_hi_lo.brake_low_al_tt = binocan_base_active_hi_lo_brake_low_al_tt_encode(active_hi_lo_grp.AL_brake_low);
                binocan_base_active_hi_lo.parking_brake_al_tt = binocan_base_active_hi_lo_parking_brake_al_tt_encode(active_hi_lo_grp.AL_parking_brake);
                binocan_base_active_hi_lo.oil_pressure_al_tt = binocan_base_active_hi_lo_oil_pressure_al_tt_encode(active_hi_lo_grp.AL_oil_pressure);
                binocan_base_active_hi_lo.airbag_al_tt = binocan_base_active_hi_lo_airbag_al_tt_encode(active_hi_lo_grp.AL_airbag);
                binocan_base_active_hi_lo.cel_al_tt = binocan_base_active_hi_lo_cel_al_tt_encode(active_hi_lo_grp.AL_CEL);
                binocan_base_active_hi_lo.right_turn_ah_tt = binocan_base_active_hi_lo_right_turn_ah_tt_encode(active_hi_lo_grp.AH_right_turn);
                binocan_base_active_hi_lo.left_turn_ah_tt = binocan_base_active_hi_lo_left_turn_ah_tt_encode(active_hi_lo_grp.AH_left_turn);
                binocan_base_active_hi_lo.abs_al_tt = binocan_base_active_hi_lo_abs_al_tt_encode(active_hi_lo_grp.AL_ABS);
                binocan_base_active_hi_lo.door_al_tt = binocan_base_active_hi_lo_door_al_tt_encode(active_hi_lo_grp.AL_door);
                binocan_base_active_hi_lo.coolant_low_al_tt = binocan_base_active_hi_lo_coolant_low_al_tt_encode(active_hi_lo_grp.AL_coolant_low);
                binocan_base_active_hi_lo.button_al = binocan_base_active_hi_lo_button_al_encode(active_hi_lo_grp.AL_button);
                binocan_base_active_hi_lo.alarm_ah = binocan_base_active_hi_lo_alarm_ah_encode(active_hi_lo_grp.AH_alarm);
                binocan_base_active_hi_lo.backlight_ah = binocan_base_active_hi_lo_backlight_ah_encode(active_hi_lo_grp.AH_backlight);
                // Virtual tell tales
                binocan_base_active_hi_lo.over_temperature_tt = binocan_base_active_hi_lo_over_temperature_tt_encode(interface_board_st.overTemp);
                binocan_base_active_hi_lo.fuel_low_tt = binocan_base_active_hi_lo_fuel_low_tt_encode(interface_board_st.lowFuel);
                binocan_base_active_hi_lo_pack(tx_msg.data, &binocan_base_active_hi_lo, BINOCAN_BASE_ACTIVE_HI_LO_LENGTH);

                if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
                {
                    ESP_LOGW(TAG, "Could not queue active hi/lo message in queue");
                }

                // For debugging purposes
                // ESP_LOGI(TAG, "Ignition: %s", active_hi_lo_grp.AH_ignition ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Hi beams: %s", active_hi_lo_grp.AH_hi_beams ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Alternator: %s", active_hi_lo_grp.AL_alternator ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Brake low level: %s", active_hi_lo_grp.AL_brake_low ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Parking brake: %s", active_hi_lo_grp.AL_parking_brake ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Oil alarm: %s", active_hi_lo_grp.AL_oil_pressure ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Airbag: %s", active_hi_lo_grp.AL_airbag ? "ON" : "OFF");
                // ESP_LOGI(TAG, "CEL: %s", active_hi_lo_grp.AL_CEL ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Right turn: %s", active_hi_lo_grp.AH_right_turn ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Left turn: %s", active_hi_lo_grp.AH_left_turn ? "ON" : "OFF");
                // ESP_LOGI(TAG, "ABS: %s", active_hi_lo_grp.AL_ABS ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Door: %s", active_hi_lo_grp.AL_door ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Low coolant: %s", active_hi_lo_grp.AL_coolant_low ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Button: %s", active_hi_lo_grp.AL_button ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Alarm: %s", active_hi_lo_grp.AH_alarm ? "ON" : "OFF");
                // ESP_LOGI(TAG, "Backlight: %s", active_hi_lo_grp.AH_backlight ? "ON" : "OFF");
                xSemaphoreGive(exp_act_hilo_semaphore);
            }
        }
    }
}

/// @brief Packaging task for the odometer and trip
/// @param pvParameters
void base_odometer_PKG(void *pvParameters)
{
    binocan_base_odometer_t binocan_base_odometer;
    binocan_base_odometer_init(&binocan_base_odometer);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_BASE_ODOMETER_FRAME_ID,
        .data_length_code = BINOCAN_BASE_ODOMETER_LENGTH};

    uint32_t pulse_m = 0;

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_BASE_ODOMETER_CYCLE_TIME_MS));
        // Check if number of pulses exceed 100m, if yes update trip_m and odometer_m to their new values, and propagate to NVS and CAN, if not just repeat values over CAN and do nothing on NVS side
        pulse_m = COEFF_PULSES_TO_METER * pwm_cap_speed.pulse_counter;

        if (pulse_m > 100)
        {
            pwm_cap_speed.pulse_counter = 0;
            odometer_m += pulse_m;
            trip_m += pulse_m;
            if (odometer_set(odometer_m) != ESP_OK)
            {
                ESP_LOGW(TAG, "Could not set odometer_m in NVS");
                interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
            }
            if (trip_set(trip_m) != ESP_OK)
            {
                ESP_LOGW(TAG, "Could not set trip_m in NVS");
                interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
            }
            binocan_base_odometer.odometer_km = binocan_base_odometer_odometer_km_encode((float)(odometer_m / 1000));
            binocan_base_odometer.odo_rem_m = binocan_base_odometer_odo_rem_m_encode((float)(odometer_m % 1000));
            binocan_base_odometer.trip_km = binocan_base_odometer_trip_km_encode((float)(trip_m / 1000));
            binocan_base_odometer.trip_rem_m = binocan_base_odometer_trip_rem_m_encode((float)(trip_m % 1000));
            binocan_base_odometer_pack(tx_msg.data, &binocan_base_odometer, BINOCAN_BASE_ODOMETER_LENGTH);
        }
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue odometer message in queue");
        }
    }
}

/// @brief Gathering and packaging task for the internal state indicators
/// @param pvParameters
void interface_brd_ST_PKG(void *pvParameters)
{
    binocan_interface_brd_st_t binocan_interface_brd_st;
    binocan_interface_brd_st_init(&binocan_interface_brd_st);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_INTERFACE_BRD_ST_FRAME_ID,
        .data_length_code = BINOCAN_INTERFACE_BRD_ST_LENGTH};

    gpio_set_direction((gpio_num_t)CONFIG_LD_ALIVE_IO, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_LD_ALIVE_IO, GPIO_PULLDOWN_ONLY);
    gpio_pulldown_en((gpio_num_t)CONFIG_LD_ALIVE_IO);

    gpio_set_direction((gpio_num_t)CONFIG_RD_ALIVE_IO, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_RD_ALIVE_IO, GPIO_PULLDOWN_ONLY);
    gpio_pulldown_en((gpio_num_t)CONFIG_RD_ALIVE_IO);

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_INTERFACE_BRD_ST_CYCLE_TIME_MS));

        interface_board_st.LD_CHECK_ALIVE_ST = gpio_get_level((gpio_num_t)CONFIG_LD_ALIVE_IO);
        interface_board_st.RD_CHECK_ALIVE_ST = gpio_get_level((gpio_num_t)CONFIG_RD_ALIVE_IO);
        interface_board_st.EN_5_V_AUX_ST = gpio_get_level((gpio_num_t)CONFIG_5V_AUX_EN_GPIO);
        interface_board_st.EN_5_V_ST = gpio_get_level((gpio_num_t)CONFIG_5V_EN_GPIO);

        binocan_interface_brd_st.en_5_v_aux_st = binocan_interface_brd_st_en_5_v_aux_st_encode(interface_board_st.EN_5_V_AUX_ST);
        binocan_interface_brd_st.en_5_v_st = binocan_interface_brd_st_en_5_v_st_encode(interface_board_st.EN_5_V_ST);
        binocan_interface_brd_st.itfc_board_st = binocan_interface_brd_st_itfc_board_st_encode(interface_board_st.internal_ST);
        binocan_interface_brd_st.ld_check_alive_st = binocan_interface_brd_st_ld_check_alive_st_encode(interface_board_st.LD_CHECK_ALIVE_ST);
        binocan_interface_brd_st.rd_check_alive_st = binocan_interface_brd_st_rd_check_alive_st_encode(interface_board_st.RD_CHECK_ALIVE_ST);
        binocan_interface_brd_st_pack(tx_msg.data, &binocan_interface_brd_st, BINOCAN_INTERFACE_BRD_ST_LENGTH);

        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue internal state message in queue");
        }
    }
}

#pragma endregion

#pragma region Main App
extern "C" void app_main(void)
{

    interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_INIT_CHOICE; // Move to Init state

#pragma region App metadata parse
    // Allocate memory for app_metadata and fill it by reference
    interface_board_st.app_metadata = (parsed_app_meta_t *)malloc(sizeof(parsed_app_meta_t));
    if (interface_board_st.app_metadata)
    {
        esp_err_t meta_err = parse_app_metadata(interface_board_st.app_metadata);
        if (meta_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to parse app metadata");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to allocate memory for app_metadata");
    }
    // vTaskDelay(pdMS_TO_TICKS(30000)); // Only for debug

#pragma endregion

#pragma region Escape sequence
    // Still in placeholder setup
    ESP_LOGI(TAG, "Setting up escape sequence detection.");
    esp_err_t ret = ESP_FAIL;
    ret = gpio_set_direction((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO, GPIO_MODE_INPUT);
    ret = gpio_set_direction((gpio_num_t)CONFIG_FULL_BEAMS_IRQ_GPIO, GPIO_MODE_INPUT);
    ret = gpio_set_pull_mode((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO, GPIO_PULLUP_ONLY);     // Non inverted
    ret = gpio_set_pull_mode((gpio_num_t)CONFIG_FULL_BEAMS_IRQ_GPIO, GPIO_PULLUP_ONLY); // Inverted
    ret = gpio_pullup_en((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO);
    ret = gpio_pullup_en((gpio_num_t)CONFIG_FULL_BEAMS_IRQ_GPIO);
    ESP_LOGI(TAG, "Escape sequence GPIOs set.");
    if (gpio_get_level((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO) == 0 && gpio_get_level((gpio_num_t)CONFIG_FULL_BEAMS_IRQ_GPIO) == 0)
    {
        ESP_LOGI(TAG, "Escape sequence detected, starting debounce countdown");
        int64_t start_esc_seq_ts = esp_timer_get_time();
        while (gpio_get_level((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO) == 0 && gpio_get_level((gpio_num_t)CONFIG_FULL_BEAMS_IRQ_GPIO) == 0)
        {
            if (esp_timer_get_time() - start_esc_seq_ts > 1000 * 1000)
            {
                ESP_LOGW(TAG, "Escape sequence successfully completed, recording current OTA partition ID.");
                // Record current active OTA partition ID
                ESP_LOGW(TAG, "Setting target boot partition to factory app (updater)");
                // Set target boot partition to factory
                ESP_LOGW(TAG, "Rebooting.");
                // Rebooting
                break;
            }
        }
    }
#pragma endregion

#pragma region Setup Sequence
    // Start 5V Channels
    if (init_5V_ctrl() != ESP_OK)
    {
        ESP_LOGW(TAG, "Impossible to initialize 5V outputs. Continuing");
    }
    if (enable_5V() != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not fire up main 5V output. Continuing.");
    }
    if (enable_5V_AUX() != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not fire up secondary 5V output. Continuing.");
    }

    // Start TWAI
    if (initCAN(NULL) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialize TWAI daemon, aborting...");
        return;
    }

    // Initialize NVS and try to read persisted u32 values: odometer_m and trip_m
    esp_err_t nvs_err = nvs_init_flash();
    if (nvs_err != ESP_OK)
    {
        ESP_LOGW(TAG, "NVS init failed (%d). Continuing without persisted odometer/trip.", nvs_err);
    }
    else
    {
        bool found = false;
        odometer_m = odometer_get(&found);
        if (found)
        {
            ESP_LOGI(TAG, "NVS: odometer_m = %u m", odometer_m);
        }
        else
        {
            ESP_LOGI(TAG, "NVS: odometer_m not found");
            if (odometer_set(odometer_m) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not set odometer in NVS!");
            }
        }

        trip_m = trip_get(&found);
        if (found)
        {
            ESP_LOGI(TAG, "NVS: trip_m = %u m", trip_m);
        }
        else
        {
            ESP_LOGI(TAG, "NVS: trip_m not found");
            if (trip_set(trip_m) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not set trip in NVS!");
            }
        }
    }

    // Set up the capture channels
    if (set_capture_channel(cap_chan_coolant, (gpio_num_t)CONFIG_COOLANT_PWM_CAP_GPIO, &pwm_cap_coolant) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set Coolant capture channel");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }
    if (set_capture_channel(cap_chan_rpm, (gpio_num_t)CONFIG_RPM_PWM_CAP_GPIO, &pwm_cap_rpm) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set RPM capture channel");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }
    if (set_capture_channel(cap_chan_speed, (gpio_num_t)CONFIG_SPEED_PWM_CAP_GPIO, &pwm_cap_speed) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set Speed capture channel");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }

    // Set up the IO Expander

    if (i2cdev_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start I²C bus");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }
    if (initialize_adc_processor() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start ADC processor");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }
    if (initialize_exp_active_hi_lo_proc() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start ActHiLo processor");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }

    // Set up the packaging and queuing tasks
    if (xTaskCreate(interface_brd_ST_PKG, "ITFC_ST_PKG", 4096, NULL, 3, &interface_brd_ST_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create interface state package task");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }
    if (xTaskCreate(base_active_hilo_PKG, "B_AHL_PKG", 4096, NULL, 3, &exp_act_hilo_proc_task_hdl) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base ActHiLo package task");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }
    if (xTaskCreate(base_slow_metrics_PKG, "B_SLO_M_PKG", 4096, NULL, 3, &base_slow_metrics_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base slow metrics package task");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }
    if (xTaskCreate(base_fast_metrics_PKG, "B_FST_M_PKG", 4096, NULL, 3, &base_fast_metrics_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base fast metrics package task");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }
    if (xTaskCreate(base_odometer_PKG, "B_ODO_PKG", 4096, NULL, 3, &base_odometer_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base odometer package task");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
        // return;
    }

#pragma region Register Button ISR and timer
    // Create the trip reset debounce/confirmation timer (one-shot 500 ms)
    trip_reset_timer = xTimerCreate("trip_rst", pdMS_TO_TICKS(500), pdFALSE, NULL, trip_reset_timer_cb);
    if (trip_reset_timer == NULL)
    {
        ESP_LOGW(TAG, "Could not create trip reset timer; button long-press will not reset trip");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
    }

    if (gpio_set_intr_type((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO, GPIO_INTR_NEGEDGE) != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not set Button IO interrupt type");
        interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
    }
    else
    {
        ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
        switch (ret)
        {
        case ESP_ERR_INVALID_STATE:
            ESP_LOGW(TAG, "ISR Service already started");
            break;
        case ESP_OK:
            ESP_LOGD(TAG, "ISR Service starting");
            break;
        default:
            ESP_LOGE(TAG, "Could not start ISR service.");
            interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
            break;
        }
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE)
        {
            if (gpio_isr_handler_add((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO, button_isr_handler, NULL) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not add ISR handler for button to service");
                interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_DEGRADED_CHOICE;
            }
        }
    }

#pragma endregion

#pragma endregion
    interface_board_st.internal_ST = BINOCAN_INTERFACE_BRD_ST_ITFC_BOARD_ST_OK_CHOICE;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        // Only used to log MCPWM output
        /* #ifdef CONFIG_LOOP_LOG_MCPWM
                vTaskDelay(pdMS_TO_TICKS(LOG_INTERVAL_MS));
                compute_freq_dut(&pwm_cap_coolant);
                compute_freq_dut(&pwm_cap_rpm);
                compute_freq_dut(&pwm_cap_speed);
                ESP_LOGI(TAG, "Coolant:\t %.2f Hz, %.1f%% \t|\tRPM:\t %.2f Hz, %.1f%% \t|\tSpeed:\t %.2f Hz, %.1f%%",
                         pwm_cap_coolant.frequency, pwm_cap_coolant.duty_cycle * 100.0,
                         pwm_cap_rpm.frequency, pwm_cap_rpm.duty_cycle * 100.0,
                         pwm_cap_speed.frequency, pwm_cap_speed.duty_cycle * 100.0);

                pwm_cap_coolant.deltaT = 0;
                pwm_cap_coolant.period_ticks = 0;
                pwm_cap_rpm.deltaT = 0;
                pwm_cap_rpm.period_ticks = 0;
                pwm_cap_speed.deltaT = 0;
                pwm_cap_speed.period_ticks = 0;
        #endif */
    }
}

#pragma endregion