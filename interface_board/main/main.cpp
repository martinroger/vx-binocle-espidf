#include <stdio.h>
// #include <string.h> // Required for version and SHA extraction
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/temperature_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "coefficients.h"
#include "nvs_settings.hpp"

#include "esp_partition.h"
#include "esp_ota_ops.h"

#include "adc_processor.h"
// #include "sma_filter.h"

#include "global_vars.hpp"
#include "twai_ops.hpp"

#include "mcpwm_processor.hpp"

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
static void onTripResetTimer(TimerHandle_t xTimer)
{
    // If button still low, reset trip
    if (gpio_get_level((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO) == 0)
    {
        ESP_LOGI(TAG, "Button long-press detected: resetting trip_m to 0");
        trip_m = 0;
        if (trip_set(trip_m) != ESP_OK)
        {
            ESP_LOGW(TAG, "Could not set trip_m in NVS");
            itfBoard.smState = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
            ESP_LOGW(__func__, "Entering degraded mode at line %lu", __LINE__);
        }
    }

    // Re-enable GPIO interrupt so future presses are detected
    gpio_intr_enable((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO);
}

// ISR: disable further interrupts and start the confirmation timer
static void onButtonIsr(void *arg)
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
esp_err_t init5VCtrl(void)
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
    itfBoard.en5VAux = false;
    itfBoard.en5V = false;
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Issue setting up control pins for 5V outputs");
    }
    return ret;
}

/// @brief Initialiser for fuel resistance sensing caliber GPIO
/// @return ESP_OK if all set up correctly, ESP_FAIL otherwise
esp_err_t initFuelSenseCtrl(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "Initializing fuel sensing caliber control pin (GPIO %d)", CONFIG_SET_HIGH_CAL_GPIO);
    ret = gpio_set_direction((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO, GPIO_MODE_INPUT_OUTPUT);
    ret = gpio_set_pull_mode((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO, GPIO_PULLDOWN_ONLY);
    ret = gpio_pulldown_en((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO);
    ret = gpio_set_level((gpio_num_t)CONFIG_SET_HIGH_CAL_GPIO, 0);
    itfBoard.hiRangeSense = false;
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Issue setting up fuel sensing caliber pin");
    }
    return ret;
}

/// @brief Enables (after initialisation) the main 5V power supply, necessary for CAN communication and RDB
/// @param
/// @return ESP_OK if all started OK, ESP_FAIL otherwise.
esp_err_t enable5V(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGD(TAG, "Enabling 5V output.");
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_EN_GPIO, 1);
    if (ret != ESP_OK)
    {
        itfBoard.en5V = false;
        ESP_LOGW(TAG, "Could not enable 5V output, continuing.");
    }
    else
    {
        itfBoard.en5V = true;
    }
    return ret;
}

/// @brief Enables (after initialisation) the auxiliary 5V power supply, necessary for LDB
/// @param
/// @return ESP_OK if all started OK, ESP_FAIL otherwise.
esp_err_t enable5VAux(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGD(TAG, "Enabling 5V auxiliary output.");
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_AUX_EN_GPIO, 1);
    if (ret != ESP_OK)
    {
        itfBoard.en5VAux = false;
        ESP_LOGW(TAG, "Could not enable 5V auxiliary output, continuing.");
    }
    else
    {
        itfBoard.en5VAux = true;
    }
    return ret;
}

/// @brief Disables the main 5V power supply. CAN and RDB will go off.
/// @param
/// @return
esp_err_t disable5V(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGD(TAG, "Disabling 5V output.");
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_EN_GPIO, 0);
    if (ret != ESP_OK)
    {
        itfBoard.en5V = true;
        ESP_LOGW(TAG, "Could not disable 5V output, continuing.");
    }
    else
    {
        itfBoard.en5V = false;
    }
    return ret;
}

/// @brief Disables the auxiliary 5V power supply. LDB will go off.
/// @param
/// @return
esp_err_t disable5VAux(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGD(TAG, "Disabling 5V auxiliary output.");
    ret = gpio_set_level((gpio_num_t)CONFIG_5V_AUX_EN_GPIO, 0);
    if (ret != ESP_OK)
    {
        itfBoard.en5VAux = true;
        ESP_LOGW(TAG, "Could not disable 5V auxiliary output, continuing.");
    }
    else
    {
        itfBoard.en5VAux = false;
    }
    return ret;
}
#pragma endregion

#pragma region Calibration settings
/// @brief Opens one settings namespace read-write, runs @p load on it and closes it.
/// @return ESP_OK, or the first NVS error
static esp_err_t withNamespace(const char *ns, esp_err_t (*load)(nvs_handle_t))
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &h);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not open NVS namespace %s: %s", ns, esp_err_to_name(err));
        return err;
    }
    err = load(h);
    nvs_close(h);
    return err;
}

/// @brief Loads the fuel sender calibration, self-initialising missing keys.
static esp_err_t loadFuelSettings(nvs_handle_t h)
{
    namespace k = vx::nvs::fuel;
    uint8_t learnEn = fuelLearnEnabled ? 1 : 0;
    esp_err_t ret = vx::nvs::loadOrInit<uint8_t>(h, k::LEARN_EN, learnEn, [](uint8_t v) { return v <= 1; });
    fuelLearnEnabled = (learnEn != 0);
    ESP_LOGI(TAG, "Fuel self-learning enabled: %s", fuelLearnEnabled ? "YES" : "NO");

    esp_err_t err = vx::nvs::loadOrInit<uint16_t>(h, k::FULL_OHM, fuelFullOhm);
    if (ret == ESP_OK)
        ret = err;
    ESP_LOGI(TAG, "Fuel full resistance: %u Ohm", fuelFullOhm);

    err = vx::nvs::loadOrInit<uint16_t>(h, k::LOW_PCT, fuelLowPct);
    if (ret == ESP_OK)
        ret = err;
    ESP_LOGI(TAG, "Low fuel threshold : %u pc", fuelLowPct);
    return ret;
}

/// @brief Loads the coolant overtemperature threshold, self-initialising it when missing.
static esp_err_t loadCoolantSettings(nvs_handle_t h)
{
    esp_err_t err = vx::nvs::loadOrInit<uint16_t>(h, vx::nvs::coolant::HOT_DEGC, coolantHotDegC);
    ESP_LOGI(TAG, "Coolant overtemp threshold: %u degC", coolantHotDegC);
    return err;
}

/// @brief Loads the ECE R39 compliance settings into r39Params, self-initialising missing or out-of-range keys.
/// @return ESP_OK, or the first NVS write error (r39Params then holds the defaults for the failed keys)
static esp_err_t loadR39Params(nvs_handle_t h)
{
    namespace k = vx::nvs::r39;
    r39::RawParams raw = r39::defaults();
    esp_err_t ret = ESP_OK;
    esp_err_t err;

    err = vx::nvs::loadOrInit<uint8_t>(h, k::EN, raw.en, [](uint8_t v) { return v <= 1; });
    if (err != ESP_OK && ret == ESP_OK)
        ret = err;
    err = vx::nvs::loadOrInit<uint16_t>(h, k::A_BP, raw.a_bp, [](uint16_t v) { return v <= r39::A_BP_MAX; });
    if (err != ESP_OK && ret == ESP_OK)
        ret = err;
    err = vx::nvs::loadOrInit<int16_t>(h, k::B_CKPH, raw.b_ckph, [](int16_t v) { return v >= r39::B_CKPH_MIN && v <= r39::B_CKPH_MAX; });
    if (err != ESP_OK && ret == ESP_OK)
        ret = err;

    r39Params = raw.to_params(CONFIG_R39_BLEND_END_KPH);
    if (r39Params.enabled)
        ESP_LOGI(TAG, "ECE R39 mode: ON, A = %.4f (+%.2f %%), B = %.2f km/h, blend to %d km/h", r39Params.a, raw.a_pct(), r39Params.b_kph, CONFIG_R39_BLEND_END_KPH);
    else
        ESP_LOGI(TAG, "ECE R39 mode: OFF (stored A = +%.2f %%, B = %.2f km/h)", raw.a_pct(), raw.b_kph());
    return ret;
}

/// @brief Loads every calibration setting of the default NVS partition (fuel, coolant, ECE R39).
/// @return ESP_OK, or the first error; every setting is still loaded or defaulted
static esp_err_t loadCalibration()
{
    esp_err_t ret = withNamespace(vx::nvs::fuel::NS, loadFuelSettings);
    esp_err_t err = withNamespace(vx::nvs::coolant::NS, loadCoolantSettings);
    if (ret == ESP_OK)
        ret = err;
    err = withNamespace(vx::nvs::r39::NS, loadR39Params);
    if (ret == ESP_OK)
        ret = err;
    return ret;
}
#pragma endregion

#pragma region Main App
/// @brief Wrapper function to attempt a rollback
/// @return ESP_OK if rollback scheduled, some NOT_OK error otherwise
esp_err_t attemptRollback()
{
    itfBoard.smState = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
    if (rollbackPossible)
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

esp_err_t sendAnimation()
{
    esp_err_t ret = ESP_FAIL;
    // Acquisition tasks are continuing in the back
    // We just stop the sender tasks. Probably should guard against empty pointers
    vTaskSuspend(itf_fast_metrics_PKG_hdl);
    vTaskSuspend(exp_act_hilo_proc_task_hdl);

    float speed = 0;
    float rpm = 0;
    float speed_delta = 50.0 * 270.0 / 2000.0;
    float rpm_delta = 50.0 * 8000.0 / 2000.0;

    while ((speed < 270) && (rpm < 8000))
    {
        fullTellTales();
        sendFastMetrics(speed, rpm);
        speed += speed_delta;
        rpm += rpm_delta;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    uint8_t loop_counter = 0;
    while (loop_counter < 50)
    {
        fullTellTales();
        sendFastMetrics(speed, rpm);
        loop_counter++;
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskResume(exp_act_hilo_proc_task_hdl);
    vTaskResume(itf_fast_metrics_PKG_hdl);

    animationDone = true;
    return ret;
}

extern "C" void app_main(void)
{
#pragma region OTA pre-checks
    rollbackPossible = esp_ota_check_rollback_is_possible();
    ESP_LOGI(__func__, "Rollback Possible ? %u", rollbackPossible);
    const esp_partition_t *runningPart = esp_ota_get_running_partition();
    esp_ota_img_states_t imageState;
    esp_ota_get_state_partition(runningPart, &imageState);
    firstBoot = (imageState == ESP_OTA_IMG_PENDING_VERIFY);
    ESP_LOGI(__func__, "First boot ? %u", firstBoot);
#pragma endregion

    itfBoard.smState = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_INIT_CHOICE;

#pragma region App metadata parse
    // Allocate memory for appMeta and fill it by reference
    itfBoard.appMeta = (parsed_app_meta_t *)malloc(sizeof(parsed_app_meta_t));
    if (itfBoard.appMeta)
    {
        esp_err_t meta_err = parse_app_metadata(itfBoard.appMeta);
        if (meta_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to parse app metadata");
            attemptRollback();
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to allocate memory for app_metadata");
        attemptRollback();
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
        if (rollbackPossible)
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
                vx::nvs::migrateLegacyKeys();
                int8_t slot = -1;
                if (strcmp("ota_0", runningPart->label) == 0)
                {
                    ESP_LOGI(__func__, "Running partition is ota_0");
                    slot = 0;
                }
                else if (strcmp("ota_1", runningPart->label) == 0)
                {
                    ESP_LOGI(__func__, "Running partition is ota_1");
                    slot = 1;
                }
                else
                    ESP_LOGW(__func__, "Current running partition could not be identified, defaulting to factory.");
                if (vx::nvs::recordBootSlot(slot) != ESP_OK)
                {
                    ESP_LOGE(__func__, "Cannot record the boot slot in NVS");
                    itfBoard.smState = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
                    attemptRollback();
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
                        if (rollbackPossible)
                            esp_ota_mark_app_invalid_rollback_and_reboot();
                        else
                            ESP_LOGE(TAG, "Rollback not possible");
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Could not find factory partition. WTF ! Rollback and reboot.");
                    if (rollbackPossible)
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
    if (init5VCtrl() != ESP_OK)
    {
        ESP_LOGW(TAG, "Impossible to initialize 5V outputs. Continuing in invalid.");
        attemptRollback();
    }
    if (enable5V() != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not fire up main 5V output. Continuing in Invalid.");
        attemptRollback();
    }
    if (enable5VAux() != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not fire up secondary 5V output. Continuing in Invalid.");
        attemptRollback();
    }
    if (initFuelSenseCtrl() != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not initialize fuel sense caliber pin. Continuing in Invalid.");
        attemptRollback();
    }

    // Start TWAI
    if (initCAN(&dispatchFrame) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialize TWAI daemon, marking invalid and rebooting.");
        attemptRollback();
    }

    // Initialize NVS and try to read persisted u32 values: odometer_m and trip_m
    esp_err_t nvs_err = nvs_init_odo_flash(); // initialises the odo partition, subcases handled internally
    if (nvs_err != ESP_OK)
    {
        ESP_LOGW(TAG, "NVS init failed (%d). Continuing without persisted odometer/trip, invalidated and rollback on next boot.", nvs_err);
        attemptRollback();
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
                attemptRollback();
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
                attemptRollback();
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
            attemptRollback();
        }
        else // Successfully entered base NVS
        {
            vx::nvs::migrateLegacyKeys();
            if (loadCalibration() != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not initialise the calibration settings in NVS");
                attemptRollback();
            }
        }
    }

    // Set up the capture channels
    if (set_capture_channel(capChanCoolant, (gpio_num_t)CONFIG_COOLANT_PWM_CAP_GPIO, &pwmCapCoolant) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set Coolant capture channel. Rollback on reboot");
        attemptRollback();
    }
    if (set_capture_channel(capChanRpm, (gpio_num_t)CONFIG_RPM_PWM_CAP_GPIO, &pwmCapRpm) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set RPM capture channel. Rollback on reboot");
        attemptRollback();
    }
    if (set_capture_channel(capChanSpeed, (gpio_num_t)CONFIG_SPEED_PWM_CAP_GPIO, &pwmCapSpeed) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not set Speed capture channel. Rollback on reboot");
        attemptRollback();
    }

    // Set up the IO Expander

    if (i2cdev_init() != ESP_OK) // General I²C bus access
    {
        ESP_LOGE(TAG, "Could not start I²C bus, rollback on next reboot");
        itfBoard.expanderOk = false;
        itfBoard.adcOk = false;
        attemptRollback();
    }
    if (initialize_ADC_processor() != ESP_OK) // ADC processor
    {
        ESP_LOGE(TAG, "Could not start ADC processor");
        itfBoard.adcOk = false;
        if (firstBoot)
            attemptRollback();
    }
    if (initialize_exp_active_hi_lo_proc() != ESP_OK) // Active hi/lo inputs processor
    {
        ESP_LOGE(TAG, "Could not start ActHiLo processor");
        itfBoard.expanderOk = false;
        if (firstBoot)
            attemptRollback();
    }

    if (initTwaiOps() != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start TWAI operations fully");
        attemptRollback();
    }

    if (initMcpwmProcessor() != ESP_OK)
    {
        ESP_LOGE(TAG, "MCPWM processor failed to init");
        attemptRollback();
    }

#pragma region Register Button ISR and timer
    // Create the trip reset debounce/confirmation timer (one-shot 500 ms)
    trip_reset_timer = xTimerCreate("trip_rst", pdMS_TO_TICKS(500), pdFALSE, NULL, onTripResetTimer);
    if (trip_reset_timer == NULL)
    {
        ESP_LOGW(TAG, "Could not create trip reset timer; button long-press will not reset trip");
        attemptRollback();
    }
    if (gpio_set_intr_type((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO, GPIO_INTR_NEGEDGE) != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not set Button IO interrupt type");
        attemptRollback();
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
            attemptRollback();
            break;
        }
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE)
        {
            if (gpio_isr_handler_add((gpio_num_t)CONFIG_BUTTON_IRQ_GPIO, onButtonIsr, NULL) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not add ISR handler for button to service");
                attemptRollback();
            }
        }
    }

#pragma endregion

#pragma endregion
    if (itfBoard.smState != BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE)
    {
        itfBoard.smState = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_OK_CHOICE;
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
    xTaskCreate(printSystemStats, "RUNSTATS", 4096, NULL, 1, &print_runtime_stats_Hdl);
#endif

    while (!(ldbReady && rdbReady))
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (ldbReady && rdbReady && !animationDone)
        sendAnimation();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#pragma endregion