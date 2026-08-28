#include <stdio.h>
// #include <string.h> // Required for version and SHA extraction
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/temperature_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "coefficients.h"

#include "esp_partition.h"
#include "esp_ota_ops.h"

#include "adc_processor.h"
// #include "sma_filter.h"

#include "global_vars.hpp"
#include "twai_ops.hpp"

#ifdef TAG
#undef TAG
#endif
#define TAG "MAIN"

#ifdef CONFIG_ENABLE_RUNTIME_STATS_OUTPUT
#include "runtime_stats.hpp"
#endif

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

/// @brief Initialiser for fuel resistance sensing caliber GPIO
/// @return ESP_OK if all set up correctly, ESP_FAIL otherwise
esp_err_t init_fuel_sense_ctrl(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "Initializing fuel sensing caliber control pin (GPIO %d)", CONFIG_SET_HIGH_CAL_GPIO);
    ret = gpio_set_direction((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO, GPIO_MODE_OUTPUT);
    ret = gpio_set_pull_mode((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO, GPIO_PULLDOWN_ONLY);
    ret = gpio_pulldown_en((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO);
    ret = gpio_set_level((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO, 0);
    interface_board_st.EN_hi_R_sense_ST = false;
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Issue setting up fuel sensing caliber pin");
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
    if (init_fuel_sense_ctrl() != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not initialize fuel sense caliber pin. Continuing in Invalid.");
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
                // Fuel self-learning enable flag
                uint8_t learn_en_u8 = fuel_learn_en ? 1 : 0;
                nvs_err = nvs_get_u8(nvs_h, "fuel_learn_en", &learn_en_u8);
                if (nvs_err == ESP_ERR_NVS_NOT_FOUND)
                {
                    ESP_LOGW(TAG, "Fuel learn enable flag not found in memory, initializing to true");
                    learn_en_u8 = 1;
                    nvs_err = nvs_set_u8(nvs_h, "fuel_learn_en", learn_en_u8);
                    if (nvs_err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not set fuel_learn_en in NVS");
                        attemptRollBack();
                    }
                    else
                        nvs_commit(nvs_h);
                }
                fuel_learn_en = (learn_en_u8 != 0);
                ESP_LOGI(TAG, "Fuel self-learning enabled: %s", fuel_learn_en ? "YES" : "NO");

                // Fuel full resistance (Ohms)
                nvs_err = nvs_get_u16(nvs_h, "fuel_full_r", &fuel_full_r);
                if (nvs_err == ESP_ERR_NVS_NOT_FOUND)
                {
                    ESP_LOGW(TAG, "Fuel full resistance not found in memory, initializing to %u Ohm", (uint16_t)COEFF_FUEL_FULL_R);
                    fuel_full_r = (uint16_t)COEFF_FUEL_FULL_R;
                    nvs_err = nvs_set_u16(nvs_h, "fuel_full_r", fuel_full_r);
                    if (nvs_err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not set fuel_full_r in NVS");
                        attemptRollBack();
                    }
                    else
                        nvs_commit(nvs_h);
                }
                ESP_LOGI(TAG, "Fuel full resistance: %u Ohm", fuel_full_r);

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

                ESP_LOGI(TAG, "Coolant overtemp threshold: %u degC", coolant_overtemp_threshold_degC);
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

    if (i2cdev_init() != ESP_OK) // General I²C bus access
    {
        ESP_LOGE(TAG, "Could not start I²C bus, rollback on next reboot");
        interface_board_st.expander_ST = false;
        interface_board_st.adc_ST = false;
        attemptRollBack();
    }
    if (initialize_ADC_processor() != ESP_OK) // ADC processor
    {
        ESP_LOGE(TAG, "Could not start ADC processor");
        interface_board_st.adc_ST = false;
        if (firstBoot)
            attemptRollBack();
    }
    if (initialize_exp_active_hi_lo_proc() != ESP_OK) // Active hi/lo inputs processor
    {
        ESP_LOGE(TAG, "Could not start ActHiLo processor");
        interface_board_st.expander_ST = false;
        if (firstBoot)
            attemptRollBack();
    }

    if (twai_ops_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start TWAI operations fully");
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