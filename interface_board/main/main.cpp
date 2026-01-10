#include <stdio.h>
#include <string.h> // Required for version and SHA extraction
// #include "gpio_defs.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/mcpwm_prelude.h"
#include "driver/temperature_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
// #include "esp_app_desc.h"
#include "version_parser.h"
#include "mcpwm_capture_helpers.h"
#include "coefficients.h"

#include "esp_partition.h"
#include "esp_ota_ops.h"

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

#ifdef CONFIG_ENABLE_RUNTIME_STATS_OUTPUT
#include "runtime_stats.hpp"
#endif

#pragma region Global variables

struct board_ST
{
    bool EN_hi_R_sense_ST = false;   // Is the high Resistance caliber on
    bool EN_5_V_ST = false;          // Is the main 5V power supply enabled
    bool EN_5_V_AUX_ST = false;      // Is the Auxiliary 5V power supply enabled
    uint8_t internal_ST = 0x00;      // Internal state of the board
    bool expander_ST = true;         // Is the IO Expander available and running
    bool adc_ST = true;              // Is the ADC available and running
    float mcu_temperature = 0.0;     // Internally measured MCU temperature (°C)
    bool LDB_check_alive_ST = false; // Is the Left Display reporting 3V3 on the control GPIO
    bool RDB_check_alive_ST = false; // Is the Right Display reporting 3V3 on the control GPIO
    bool lowFuel = false;            // Is the low fuel flag internally set
    bool overTemp = false;           // Is the internal overtemp flag set
    parsed_app_meta_t *app_metadata; // Parsed app metadata for serving over CAN
} interface_board_st;

bool rollBackPossible; // Is rollback possible ?
bool firstBoot;        // Is this the first boot after OTA ?

// Placeholders editable in factory mode
uint16_t fuel_lvl_comp_factor = 1000; // Is divided by 1000.0 later
uint16_t fuel_low_level_threshold_pc = 20;
uint16_t coolant_overtemp_threshold_degC = 106;

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
            interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
            ESP_LOGW(__func__, "Entering degraded mode at line %lu", __LINE__);
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

/// @brief Initialiser for the control GPIOs of the 5V supplies
/// @param
/// @return ESP_OK if all set up correctly. ESP_FAIL otherwise
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

/// @brief Enables (after initialisation) the main 5V power supply, necessary for CAN communication and RDB
/// @param
/// @return ESP_OK if all started OK, ESP_FAIL otherwise.
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

/// @brief Enables (after initialisation) the auxiliary 5V power supply, necessary for LDB
/// @param
/// @return ESP_OK if all started OK, ESP_FAIL otherwise.
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

/// @brief Disables the main 5V power supply. CAN and RDB will go off.
/// @param
/// @return
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

/// @brief Disables the auxiliary 5V power supply. LDB will go off.
/// @param
/// @return
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
TaskHandle_t itf_slow_metrics_PKG_hdl;
TaskHandle_t itf_fast_metrics_PKG_hdl;
// exp_act_hilo_proc_task_hdl for active high lows is already defined in tca9555_helpers because of interrupt wrapping
TaskHandle_t itf_odometer_PKG_hdl;
TaskHandle_t itf_board_st_PKG_hdl;
TaskHandle_t itf_board_version_PKG_hdl;

/// @brief Packaging task for base_slow_metrics
/// @param pvParameters
void itf_slow_metrics_PKG(void *pvParameters)
{
    // SMA-ed slower metrics :
    // coolant_temp
    // fuel_level_pc
    // lv_voltage_v

    binocan_itf_slow_metrics_t binocan_itf_slow_metrics;
    binocan_itf_slow_metrics_init(&binocan_itf_slow_metrics);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_SLOW_METRICS_FRAME_ID,
        .data_length_code = BINOCAN_ITF_SLOW_METRICS_LENGTH};
    esp_err_t compute_err;

    while (true)
    {
        // SMAs should already be protected, and some of the MCPWM logic can be brought in here.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_SLOW_METRICS_CYCLE_TIME_MS)); // Waits for notification or one cyclic for frame message
        compute_err = compute_freq_dut(&pwm_cap_coolant);
        // if (compute_err != ESP_OK)
        //     ESP_LOGW(__func__,"Speed DutFreq Compute error : %s",esp_err_to_name(compute_err));
        float coolant_degC = 100.0 * pwm_cap_coolant.duty_cycle * COEFF_DUTY_TO_COOLANT_DEGC_M + COEFF_DUTY_TO_COOLANT_DEGC_P;
        if (coolant_degC < 70)
            coolant_degC = 70;
        if (coolant_degC > 130)
            coolant_degC = 130;
        interface_board_st.overTemp = (coolant_degC >= coolant_overtemp_threshold_degC ? true : false);
        // Comment in for debug
        ESP_LOGD(TAG, "Coolant: %.2f - %.2f - %.2f", pwm_cap_coolant.frequency, pwm_cap_coolant.duty_cycle * 100.0, coolant_degC);

        float fuel_level_raw = sma_get_avg(adc_channels[0].sma);
        float fuel_level_v = fuel_level_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float fuel_level_pc = (float)(fuel_lvl_comp_factor / 1000.0) * COEFF_FUEL_V_TO_PC_M * fuel_level_v + COEFF_FUEL_V_TO_PC_P;
        if (fuel_level_pc > 100.0)
            fuel_level_pc = 100;
        if (fuel_level_pc < 0)
            fuel_level_pc = 0;
        interface_board_st.lowFuel = (fuel_level_pc < fuel_low_level_threshold_pc ? true : false);

        float lv_raw = sma_get_avg(adc_channels[1].sma);
        float lv_raw_v = lv_raw * ads111x_gain_values[ADS111X_GAIN_4V096] / ADS111X_MAX_VALUE;
        float lv_v = lv_raw_v * COEFF_V_TO_LV_M + COEFF_V_TO_LV_P;

        // Comment in for debug
        ESP_LOGD(TAG, "Fuel : %.2f - %.3fV/%.3f - %.2fR- %.2fpc\t|\t 12V: %.2f - %.2fV - %.2fV", fuel_level_raw, fuel_level_v, COEFF_FUEL_FULL_V, 1000.0 * fuel_level_v / COEFF_LOW_CALIBER_CURRENT, fuel_level_pc, lv_raw, lv_raw_v, lv_v);

        binocan_itf_slow_metrics.itf_coolant_temp = binocan_itf_slow_metrics_itf_coolant_temp_encode(coolant_degC);
        binocan_itf_slow_metrics.itf_fuel_level_pc = binocan_itf_slow_metrics_itf_fuel_level_pc_encode(fuel_level_pc);
        binocan_itf_slow_metrics.itf_lv_voltage_v = binocan_itf_slow_metrics_itf_lv_voltage_v_encode(lv_v);
        binocan_itf_slow_metrics_pack(tx_msg.data, &binocan_itf_slow_metrics, BINOCAN_ITF_SLOW_METRICS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue slow metrics message in queue");
        }
    }
}

/// @brief Packaging task for base_fast_metrics
/// @param pvParameters
void itf_fast_metrics_PKG(void *pvParameters)
{
    // Only faster metrics
    // speed_kph
    // rpm
    esp_err_t compute_err;
    binocan_itf_fast_metrics_t binocan_itf_fast_metrics;
    binocan_itf_fast_metrics_init(&binocan_itf_fast_metrics);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_FAST_METRICS_FRAME_ID,
        .data_length_code = BINOCAN_ITF_FAST_METRICS_LENGTH};

    while (true)
    {
        // Transport some of the MCPWM logic in there
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_FAST_METRICS_CYCLE_TIME_MS));
        compute_err = compute_freq_dut(&pwm_cap_rpm);
        // if (compute_err != ESP_OK)
        //     ESP_LOGW(__func__,"RPM DutFreq Compute error : %s",esp_err_to_name(compute_err));
        compute_err = compute_freq_dut(&pwm_cap_speed);
        // if (compute_err != ESP_OK)
        //     ESP_LOGW(__func__,"Speed DutFreq Compute error : %s",esp_err_to_name(compute_err));
        float rpm = COEFF_FREQ_TO_RPM_M * pwm_cap_rpm.frequency + COEFF_FREQ_TO_RPM_P;
        float speed = COEFF_FREQ_TO_SPEED_KPH_M * pwm_cap_speed.frequency + COEFF_FREQ_TO_SPEED_KPH_P;
        ESP_LOGD(TAG, "RPM : %.2f - %.1f - %.2f Speed: %.2f - %.1f - %.2f", pwm_cap_rpm.frequency, pwm_cap_rpm.duty_cycle, rpm, pwm_cap_speed.frequency, pwm_cap_speed.duty_cycle, speed);

        binocan_itf_fast_metrics.itf_rpm = binocan_itf_fast_metrics_itf_rpm_encode(rpm);
        binocan_itf_fast_metrics.itf_speed_kph = binocan_itf_fast_metrics_itf_speed_kph_encode(speed);
        binocan_itf_fast_metrics_pack(tx_msg.data, &binocan_itf_fast_metrics, BINOCAN_ITF_FAST_METRICS_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue fast metrics message in queue");
        }
    }
}

/// @brief Packaging task for base_active_hi_lo
/// @param pvParameters
void itf_active_hilo_PKG(void *pvParameters)
{
    // Will need to be using notifications and delay
    // Essentially direct from expander + a couple virtual telltales
    uint16_t raw;

    binocan_itf_active_hi_lo_t binocan_itf_active_hi_lo;
    binocan_itf_active_hi_lo_init(&binocan_itf_active_hi_lo);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_ACTIVE_HI_LO_FRAME_ID,
        .data_length_code = BINOCAN_ITF_ACTIVE_HI_LO_LENGTH};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_ACTIVE_HI_LO_CYCLE_TIME_MS));
        if (tca95x5_port_read(&tca_slave, &raw) != ESP_OK)
        {
            ESP_LOGE(TAG, "Impossible to fetch register from expander");
            interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
            ESP_LOGW(__func__, "Entering degraded mode at line %lu", __LINE__);
            interface_board_st.expander_ST = false;
        }
        else
        {
            interface_board_st.expander_ST = true;
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
                active_hi_lo_grp.AH_coolant_low = !read_bitmask(raw, EXP_IO_11_BITMASK); // Inverted
                active_hi_lo_grp.AH_left_turn = !read_bitmask(raw, EXP_IO_12_BITMASK);   // Inverted
                active_hi_lo_grp.AL_door = read_bitmask(raw, EXP_IO_13_BITMASK);
                active_hi_lo_grp.AH_right_turn = !read_bitmask(raw, EXP_IO_14_BITMASK); // Inverted
                active_hi_lo_grp.AL_ABS = read_bitmask(raw, EXP_IO_15_BITMASK);

                binocan_itf_active_hi_lo.itf_ignition_ah_st = binocan_itf_active_hi_lo_itf_ignition_ah_st_encode(active_hi_lo_grp.AH_ignition);
                binocan_itf_active_hi_lo.itf_hi_beams_ah_tt = binocan_itf_active_hi_lo_itf_hi_beams_ah_tt_encode(active_hi_lo_grp.AH_hi_beams);
                binocan_itf_active_hi_lo.itf_alternator_al_tt = binocan_itf_active_hi_lo_itf_alternator_al_tt_encode(active_hi_lo_grp.AL_alternator);
                binocan_itf_active_hi_lo.itf_brake_low_al_tt = binocan_itf_active_hi_lo_itf_brake_low_al_tt_encode(active_hi_lo_grp.AL_brake_low);
                binocan_itf_active_hi_lo.itf_parking_brake_al_tt = binocan_itf_active_hi_lo_itf_parking_brake_al_tt_encode(active_hi_lo_grp.AL_parking_brake);
                binocan_itf_active_hi_lo.itf_oil_pressure_al_tt = binocan_itf_active_hi_lo_itf_oil_pressure_al_tt_encode(active_hi_lo_grp.AL_oil_pressure);
                binocan_itf_active_hi_lo.itf_airbag_al_tt = binocan_itf_active_hi_lo_itf_airbag_al_tt_encode(active_hi_lo_grp.AL_airbag);
                binocan_itf_active_hi_lo.itf_cel_al_tt = binocan_itf_active_hi_lo_itf_cel_al_tt_encode(active_hi_lo_grp.AL_CEL);
                binocan_itf_active_hi_lo.itf_right_turn_ah_tt = binocan_itf_active_hi_lo_itf_right_turn_ah_tt_encode(active_hi_lo_grp.AH_right_turn);
                binocan_itf_active_hi_lo.itf_left_turn_ah_tt = binocan_itf_active_hi_lo_itf_left_turn_ah_tt_encode(active_hi_lo_grp.AH_left_turn);
                binocan_itf_active_hi_lo.itf_abs_al_tt = binocan_itf_active_hi_lo_itf_abs_al_tt_encode(active_hi_lo_grp.AL_ABS);
                binocan_itf_active_hi_lo.itf_door_al_tt = binocan_itf_active_hi_lo_itf_door_al_tt_encode(active_hi_lo_grp.AL_door);
                binocan_itf_active_hi_lo.itf_coolant_low_ah_tt = binocan_itf_active_hi_lo_itf_coolant_low_ah_tt_encode(active_hi_lo_grp.AH_coolant_low);
                binocan_itf_active_hi_lo.itf_button_al = binocan_itf_active_hi_lo_itf_button_al_encode(active_hi_lo_grp.AL_button);
                binocan_itf_active_hi_lo.itf_alarm_ah = binocan_itf_active_hi_lo_itf_alarm_ah_encode(active_hi_lo_grp.AH_alarm);
                binocan_itf_active_hi_lo.itf_backlight_ah = binocan_itf_active_hi_lo_itf_backlight_ah_encode(active_hi_lo_grp.AH_backlight);
                // Virtual tell tales
                binocan_itf_active_hi_lo.itf_over_temperature_tt = binocan_itf_active_hi_lo_itf_over_temperature_tt_encode(interface_board_st.overTemp);
                binocan_itf_active_hi_lo.itf_fuel_low_tt = binocan_itf_active_hi_lo_itf_fuel_low_tt_encode(interface_board_st.lowFuel);
                binocan_itf_active_hi_lo_pack(tx_msg.data, &binocan_itf_active_hi_lo, BINOCAN_ITF_ACTIVE_HI_LO_LENGTH);

                if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
                {
                    ESP_LOGW(TAG, "Could not queue active hi/lo message in queue");
                }
                xSemaphoreGive(exp_act_hilo_semaphore);
            }
        }
    }
}

/// @brief Packaging task for the odometer and trip
/// @param pvParameters
void itf_odometer_PKG(void *pvParameters)
{
    binocan_itf_odometer_t binocan_itf_odometer;
    binocan_itf_odometer_init(&binocan_itf_odometer);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_ODOMETER_FRAME_ID,
        .data_length_code = BINOCAN_ITF_ODOMETER_LENGTH};

    uint32_t pulse_m = 0;

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_ODOMETER_CYCLE_TIME_MS));
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
                interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
                ESP_LOGW(__func__, "Entering degraded mode at line %lu", __LINE__);
            }
            if (trip_set(trip_m) != ESP_OK)
            {
                ESP_LOGW(TAG, "Could not set trip_m in NVS");
                interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
                ESP_LOGW(__func__, "Entering degraded mode at line %lu", __LINE__);
            }
        }
        binocan_itf_odometer.itf_odometer_km = binocan_itf_odometer_itf_odometer_km_encode((float)(odometer_m / 1000));
        binocan_itf_odometer.itf_odo_rem_m = binocan_itf_odometer_itf_odo_rem_m_encode((float)(odometer_m % 1000));
        binocan_itf_odometer.itf_trip_km = binocan_itf_odometer_itf_trip_km_encode((float)(trip_m / 1000));
        binocan_itf_odometer.itf_trip_rem_m = binocan_itf_odometer_itf_trip_rem_m_encode((float)(trip_m % 1000));
        binocan_itf_odometer_pack(tx_msg.data, &binocan_itf_odometer, BINOCAN_ITF_ODOMETER_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue odometer message in queue");
        }
    }
}

/// @brief Gathering and packaging task for the internal state indicators
/// @param pvParameters
void itf_board_st_PKG(void *pvParameters)
{
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensor_enable(temp_sensor));

    bool ads1115_busy = false;

    binocan_itf_board_st_t binocan_itf_board_st;
    binocan_itf_board_st_init(&binocan_itf_board_st);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_BOARD_ST_FRAME_ID,
        .data_length_code = BINOCAN_ITF_BOARD_ST_LENGTH};

    gpio_set_direction((gpio_num_t)CONFIG_LD_ALIVE_IO, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_LD_ALIVE_IO, GPIO_PULLDOWN_ONLY);
    gpio_pulldown_en((gpio_num_t)CONFIG_LD_ALIVE_IO);

    gpio_set_direction((gpio_num_t)CONFIG_RD_ALIVE_IO, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_RD_ALIVE_IO, GPIO_PULLDOWN_ONLY);
    gpio_pulldown_en((gpio_num_t)CONFIG_RD_ALIVE_IO);

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_BOARD_ST_CYCLE_TIME_MS));

        interface_board_st.LDB_check_alive_ST = gpio_get_level((gpio_num_t)CONFIG_LD_ALIVE_IO);
        interface_board_st.RDB_check_alive_ST = gpio_get_level((gpio_num_t)CONFIG_RD_ALIVE_IO);
        interface_board_st.EN_5_V_AUX_ST = gpio_get_level((gpio_num_t)CONFIG_5V_AUX_EN_GPIO);
        interface_board_st.EN_5_V_ST = gpio_get_level((gpio_num_t)CONFIG_5V_EN_GPIO);
        interface_board_st.EN_hi_R_sense_ST = gpio_get_level((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO);
        ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensor_get_celsius(temp_sensor, &(interface_board_st.mcu_temperature)));
        if (ads111x_is_busy(&adc_slave, &ads1115_busy) != ESP_OK)
        {
            interface_board_st.adc_ST = false;
        }
        else
            interface_board_st.adc_ST = true;

        binocan_itf_board_st.itf_hi_r_sense_st = binocan_itf_board_st_itf_hi_r_sense_st_encode(interface_board_st.EN_hi_R_sense_ST);
        binocan_itf_board_st.itf_5_v_aux_st = binocan_itf_board_st_itf_5_v_aux_st_encode(interface_board_st.EN_5_V_AUX_ST);
        binocan_itf_board_st.itf_5_v_st = binocan_itf_board_st_itf_5_v_st_encode(interface_board_st.EN_5_V_ST);
        binocan_itf_board_st.itf_sm_st = binocan_itf_board_st_itf_sm_st_encode(interface_board_st.internal_ST);
        binocan_itf_board_st.itf_ld_check_alive_st = binocan_itf_board_st_itf_ld_check_alive_st_encode(interface_board_st.LDB_check_alive_ST);
        binocan_itf_board_st.itf_rd_check_alive_st = binocan_itf_board_st_itf_rd_check_alive_st_encode(interface_board_st.RDB_check_alive_ST);
        binocan_itf_board_st.itf_mcu_temp = binocan_itf_board_st_itf_mcu_temp_encode(interface_board_st.mcu_temperature);
        binocan_itf_board_st.itf_adc_st = binocan_itf_board_st_itf_adc_st_encode(interface_board_st.adc_ST);
        binocan_itf_board_st.itf_expander_st = binocan_itf_board_st_itf_expander_st_encode(interface_board_st.expander_ST);
        binocan_itf_board_st_pack(tx_msg.data, &binocan_itf_board_st, BINOCAN_ITF_BOARD_ST_LENGTH);

        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue internal state message in queue");
        }
    }
}

/// @brief Generates the commit and version information and regularly broadcasts them on the bus
/// @param pvParameters
void itf_board_version_PKG(void *pvParameters)
{
    uint8_t message_mux = 0;
    binocan_itf_board_version_t binocan_itf_board_version;
    binocan_itf_board_version_init(&binocan_itf_board_version);
    twai_message_t tx_msg = {
        .identifier = BINOCAN_ITF_BOARD_VERSION_FRAME_ID,
        .data_length_code = BINOCAN_ITF_BOARD_VERSION_LENGTH};

    binocan_itf_board_version.itf_version_major = binocan_itf_board_version_itf_version_major_encode((uint8_t)(interface_board_st.app_metadata->base_version[0]) - 48);
    binocan_itf_board_version.itf_version_minor = binocan_itf_board_version_itf_version_minor_encode((uint8_t)(interface_board_st.app_metadata->base_version[2]) - 48);
    binocan_itf_board_version.itf_version_patch = binocan_itf_board_version_itf_version_patch_encode((uint8_t)(interface_board_st.app_metadata->base_version[4]) - 48);
    binocan_itf_board_version.itf_version_dirty = binocan_itf_board_version_itf_version_dirty_encode((uint8_t)interface_board_st.app_metadata->is_dirty);
    // Build a 64-bit value from up to 8 bytes of commitID and pass to the encode helper.
    // This avoids memcpy into a numeric field and is lightweight.
    if (interface_board_st.app_metadata && interface_board_st.app_metadata->commitID)
    {
        // ESP_LOGI(TAG,"Commit is valid");
        uint64_t commit_val = 0;
        size_t src_len = interface_board_st.app_metadata->commit_len;
        if (src_len > 8)
            src_len = 8;
        for (size_t i = 0; i < src_len; ++i)
        {
            commit_val = (commit_val << 8) | (uint8_t)interface_board_st.app_metadata->commitID[src_len - i - 1];
        }
        binocan_itf_board_version.itf_version_commit = binocan_itf_board_version_itf_version_commit_encode(commit_val);
    }
    else
    {
        // ESP_LOGI(TAG,"Commit will be 0");
        binocan_itf_board_version.itf_version_commit = binocan_itf_board_version_itf_version_commit_encode(0ULL);
    }

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_ITF_BOARD_VERSION_CYCLE_TIME_MS));
        // Switch the mux indicator
        message_mux = (message_mux + 1) % 2; // Currently only two values to the MUX
        binocan_itf_board_version.itf_version_mux = binocan_itf_board_version_itf_version_mux_encode(message_mux);
        binocan_itf_board_version_pack(tx_msg.data, &binocan_itf_board_version, BINOCAN_ITF_BOARD_VERSION_LENGTH);
        if (xQueueSend(CAN_TX_queue_hdl, &tx_msg, pdMS_TO_TICKS(1)) != pdTRUE)
        {
            ESP_LOGW(TAG, "Could not queue internal state message in queue");
        }
    }
}
#pragma endregion

#pragma region Main App
/// @brief Wrapper function to attempt a rollback
/// @return ESP_OK if rollback scheduled, some NOT_OK error otherwise
esp_err_t attemptRollBack()
{
    interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
    if (rollBackPossible)
    {
        ESP_LOGW(__func__, "Activating rollback on next reboot.");
        return esp_ota_mark_app_invalid_rollback();
    }
    else
    {
        ESP_LOGW(__func__, "Rollback impossible, image could not be invalidated.");
        return ESP_FAIL;
    }
}

extern "C" void app_main(void)
{
#pragma region OTA pre-checks
    rollBackPossible = esp_ota_check_rollback_is_possible();
    ESP_LOGI(__func__, "Rollback Possible ? %u", rollBackPossible);
    const esp_partition_t *runningPart = esp_ota_get_running_partition();
    esp_ota_img_states_t imageState;
    esp_ota_get_state_partition(runningPart, &imageState);
    firstBoot = (imageState == ESP_OTA_IMG_PENDING_VERIFY);
    ESP_LOGI(__func__, "First boot ? %u", firstBoot);
#pragma endregion

    interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_INIT_CHOICE;

#pragma region App metadata parse
    // Allocate memory for app_metadata and fill it by reference
    interface_board_st.app_metadata = (parsed_app_meta_t *)malloc(sizeof(parsed_app_meta_t));
    if (interface_board_st.app_metadata)
    {
        esp_err_t meta_err = parse_app_metadata(interface_board_st.app_metadata);
        if (meta_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to parse app metadata");
            attemptRollBack();
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to allocate memory for app_metadata");
        attemptRollBack();
    }

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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Issue during escape sequence GPIO setup, attempt rollback and reboot.");
        if (rollBackPossible)
            esp_ota_mark_app_invalid_rollback_and_reboot();
        else
            ESP_LOGE(TAG, "Rollback not possible");
    }

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
                esp_err_t err = nvs_flash_init();
                if (err != ESP_OK)
                    ESP_LOGE(__func__, "Cannot init default NVS");
                if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
                {
                    ESP_LOGE(__func__, "Could not init default NVS, erasing and retrying.");
                    nvs_flash_erase();
                    nvs_flash_init();
                }
                nvs_handle_t h;
                if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
                {
                    ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
                    interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
                    attemptRollBack();
                }
                else
                {
                    if (strcmp("ota_0", runningPart->label) == 0)
                    {
                        ESP_LOGI(__func__, "Running partition is ota_0");
                        nvs_set_i8(h, "lastPart", 0);
                    }
                    else if (strcmp("ota_1", runningPart->label) == 0)
                    {
                        ESP_LOGI(__func__, "Running partition is ota_1");
                        nvs_set_i8(h, "lastPart", 1);
                    }
                    else
                    {
                        ESP_LOGW(__func__, "Current running partition could not be identified, defaulting to factory.");
                        nvs_set_i8(h, "lastPart", -1);
                    }
                    nvs_commit(h);
                    nvs_close(h);
                }

                ESP_LOGW(TAG, "Setting target boot partition to factory app (updater)");
                const esp_partition_t *factoryPart = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
                // We are assuming that the factory app is ALWAYS good.
                if (factoryPart != NULL)
                {
                    esp_err_t err = esp_ota_set_boot_partition(factoryPart);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not set factory partition as boot partition, wtf bro. Attempting rollback and reboot.");
                        if (rollBackPossible)
                            esp_ota_mark_app_invalid_rollback_and_reboot();
                        else
                            ESP_LOGE(TAG, "Rollback not possible");
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Could not find factory partition. WTF ! Rollback and reboot.");
                    if (rollBackPossible)
                        esp_ota_mark_app_invalid_rollback_and_reboot();
                    else
                        ESP_LOGE(TAG, "Rollback not possible");
                }
                ESP_LOGW(TAG, "Rebooting.");
                esp_restart();
                break;
            }
        }
    }
#pragma endregion

#pragma region Setup Sequence
    // Start 5V Channels
    if (init_5V_ctrl() != ESP_OK)
    {
        ESP_LOGW(TAG, "Impossible to initialize 5V outputs. Continuing in invalid.");
        attemptRollBack();
    }
    if (enable_5V() != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not fire up main 5V output. Continuing in Invalid.");
        attemptRollBack();
    }
    if (enable_5V_AUX() != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not fire up secondary 5V output. Continuing in Invalid.");
        attemptRollBack();
    }

    // Start TWAI
    if (initCAN(NULL) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialize TWAI daemon, marking invalid and rebooting.");
        attemptRollBack();
    }

    // Initialize NVS and try to read persisted u32 values: odometer_m and trip_m
    esp_err_t nvs_err = nvs_init_odo_flash(); // initialises the odo partition, subcases handled internally
    if (nvs_err != ESP_OK)
    {
        ESP_LOGW(TAG, "NVS init failed (%d). Continuing without persisted odometer/trip, invalidated and rollback on next boot.", nvs_err);
        attemptRollBack();
    }
    else // Successfully initialized the odo NVS
    {
        bool found = false;
        // Look for odometer
        odometer_m = odometer_get(&found);
        if (found)
            ESP_LOGI(TAG, "NVS: odometer_m = %u m", odometer_m);
        else
        {
            ESP_LOGI(TAG, "NVS: odometer_m not found");
            if (odometer_set(odometer_m) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not set odometer in NVS!");
                attemptRollBack();
            }
        }
        // Look for trip
        trip_m = trip_get(&found);
        if (found)
            ESP_LOGI(TAG, "NVS: trip_m = %u m", trip_m);
        else
        {
            ESP_LOGI(TAG, "NVS: trip_m not found");
            if (trip_set(trip_m) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not set trip in NVS!");
                attemptRollBack();
            }
        }
        // Switch back to normal NVS and look for calibration values
        esp_err_t nvs_err;
        nvs_err = nvs_flash_init();
        if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            nvs_err = nvs_flash_erase();
            nvs_err = nvs_flash_init();
        }
        if (nvs_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not init normal NVS partition");
            attemptRollBack();
        }
        else // Successfully entered base NVS
        {
            nvs_handle_t nvs_h;
            nvs_err = nvs_open("storage", NVS_READWRITE, &nvs_h);
            if (nvs_err != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not open normal NVS partition");
                attemptRollBack();
            }
            else // "storage" namespace is open for business
            {
                // FUel compensation factor
                nvs_err = nvs_get_u16(nvs_h, "fuel_comp", &fuel_lvl_comp_factor);
                if (nvs_err == ESP_ERR_NVS_NOT_FOUND) // Write if not found
                {
                    ESP_LOGW(TAG, "Fuel compensation factor not found in memory, initializing");
                    nvs_err = nvs_set_u16(nvs_h, "fuel_comp", fuel_lvl_comp_factor);
                    if (nvs_err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not set Fuel Comp in NVS");
                        attemptRollBack();
                    }
                    else
                        nvs_commit(nvs_h);
                }

                ESP_LOGI(TAG, "Fuel compensation factor : %u / 1000", fuel_lvl_comp_factor);
                // Low fuel threshold
                nvs_err = nvs_get_u16(nvs_h, "lo_fuel_thr", &fuel_low_level_threshold_pc); // Write if not found
                if (nvs_err == ESP_ERR_NVS_NOT_FOUND)
                {
                    ESP_LOGW(TAG, "Fuel low threshold not found in memory, initializing");
                    nvs_err = nvs_set_u16(nvs_h, "lo_fuel_thr", fuel_low_level_threshold_pc);
                    if (nvs_err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not set Fuel Low LVL in NVS");
                        attemptRollBack();
                    }
                    else
                        nvs_commit(nvs_h);
                }

                ESP_LOGI(TAG, "Low fuel threshold : %u pc", fuel_low_level_threshold_pc);
                // Coolant overtemp threshold
                nvs_err = nvs_get_u16(nvs_h, "overtemp_th", &coolant_overtemp_threshold_degC); // Write if not found
                if (nvs_err == ESP_ERR_NVS_NOT_FOUND)
                {
                    ESP_LOGW(TAG, "Coolant overtemp threshold not found in memory, initializing");
                    nvs_err = nvs_set_u16(nvs_h, "overtemp_th", coolant_overtemp_threshold_degC);
                    if (nvs_err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not set Overtmp in NVS");
                        attemptRollBack();
                    }
                    else
                        nvs_commit(nvs_h);
                }

                ESP_LOGI(TAG, "Coolant overtemp threshold: %u degC", fuel_low_level_threshold_pc);
                nvs_close(nvs_h);
            }
        }
    }

    // Set up the capture channels
    if (set_capture_channel(cap_chan_coolant, (gpio_num_t)CONFIG_COOLANT_PWM_CAP_GPIO, &pwm_cap_coolant) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set Coolant capture channel. Rollback on reboot");
        attemptRollBack();
    }
    if (set_capture_channel(cap_chan_rpm, (gpio_num_t)CONFIG_RPM_PWM_CAP_GPIO, &pwm_cap_rpm) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set RPM capture channel. Rollback on reboot");
        attemptRollBack();
    }
    if (set_capture_channel(cap_chan_speed, (gpio_num_t)CONFIG_SPEED_PWM_CAP_GPIO, &pwm_cap_speed) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set Speed capture channel. Rollback on reboot");
        attemptRollBack();
    }

    // Set up the IO Expander

    if (i2cdev_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start I²C bus, rollback on next reboot");
        interface_board_st.expander_ST = false;
        interface_board_st.adc_ST = false;
        attemptRollBack();
    }
    if (initialize_adc_processor() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start ADC processor");
        interface_board_st.adc_ST = false;
        if (firstBoot)
            attemptRollBack();
    }
    if (initialize_exp_active_hi_lo_proc() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start ActHiLo processor");
        interface_board_st.expander_ST = false;
        if (firstBoot)
            attemptRollBack();
    }

    // Set up the packaging and queuing tasks
    if (xTaskCreatePinnedToCore(itf_board_st_PKG, "ITF_ST", 4096, NULL, 3, &itf_board_st_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create interface state package task");
        attemptRollBack();
    }
    if (xTaskCreatePinnedToCore(itf_active_hilo_PKG, "ITF_AHL", 4096, NULL, 3, &exp_act_hilo_proc_task_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base ActHiLo package task");
        attemptRollBack();
    }
    if (xTaskCreatePinnedToCore(itf_slow_metrics_PKG, "ITF_SLO_M", 4096, NULL, 3, &itf_slow_metrics_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base slow metrics package task");
        attemptRollBack();
    }
    if (xTaskCreatePinnedToCore(itf_fast_metrics_PKG, "ITF_FST_M", 4096, NULL, 3, &itf_fast_metrics_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base fast metrics package task");
        attemptRollBack();
    }
    if (xTaskCreatePinnedToCore(itf_odometer_PKG, "ITF_ODO", 4096, NULL, 3, &itf_odometer_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create base odometer package task");
        attemptRollBack();
    }
    if (xTaskCreatePinnedToCore(itf_board_version_PKG, "ITF_VER", 4096, NULL, 3, &itf_board_version_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create interface board version package task");
        attemptRollBack();
    }

#pragma region Register Button ISR and timer
    // Create the trip reset debounce/confirmation timer (one-shot 500 ms)
    trip_reset_timer = xTimerCreate("trip_rst", pdMS_TO_TICKS(500), pdFALSE, NULL, trip_reset_timer_cb);
    if (trip_reset_timer == NULL)
    {
        ESP_LOGW(TAG, "Could not create trip reset timer; button long-press will not reset trip");
        attemptRollBack();
    }
    if (gpio_set_intr_type((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO, GPIO_INTR_NEGEDGE) != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not set Button IO interrupt type");
        attemptRollBack();
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
            attemptRollBack();
            break;
        }
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE)
        {
            if (gpio_isr_handler_add((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO, button_isr_handler, NULL) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not add ISR handler for button to service");
                attemptRollBack();
            }
        }
    }

#pragma endregion

#pragma endregion
    if (interface_board_st.internal_ST != BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE)
    {
        interface_board_st.internal_ST = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_OK_CHOICE;
        if (firstBoot)
        {
            if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not mark the image as valid.");
            }
            else
            {
                ESP_LOGI(TAG, "Image marked as valid, cancelled rollback");
            }
        }
    }
    else
        ESP_LOGW(__func__, "Exit startup in degraded mode.");

#ifdef CONFIG_ENABLE_RUNTIME_STATS_OUTPUT
    xTaskCreate(print_system_stats, "RUNSTATS", 4096, NULL, 1, &print_runtime_stats_Hdl);
#endif

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#pragma endregion