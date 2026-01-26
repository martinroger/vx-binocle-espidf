#include "esp_err.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_partition.h"

#include "global_vars.hpp"
#include "theme.hpp"
#include "updateUI.hpp"
#include "twai_ops.hpp"
#include "start_animation.hpp"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

#ifdef TAG
#undef TAG
#endif
#define TAG "Main"

#ifdef CONFIG_ENABLE_RUNTIME_STATS_OUTPUT
#include "runtime_stats.hpp"
#endif

#pragma region ACTIONS

/// @brief Reboot in factory app from debug screen (button)
/// @param e LV event
extern "C" void action_reboot_factory(lv_event_t *e)
{
    const esp_partition_t *factoryPart = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
    if (factoryPart != NULL)
    {
        esp_err_t bootsel_err = esp_ota_set_boot_partition(factoryPart);
        if (bootsel_err == ESP_OK)
            esp_restart();
        else
            ESP_LOGE(__func__, "Could not restart ESP");
    }
}

/// @brief Erases the NVS for all keys. Triggers a restart if the button sequence cannot be completed
/// @param e LV event is clicked
extern "C" void action_reset_settings(lv_event_t *e)
{
    esp_err_t nvs_err = nvs_flash_erase();
    if (nvs_err == ESP_OK)
    {
        nvs_err = nvs_flash_init();
        if (nvs_err == ESP_OK)
            return;
        else
            esp_restart();
    }
}

#ifdef CONFIG_RIGHT_SIDE_DISPLAY

/// @brief Test brightness at slider value when pressing
/// @param e LV event, assumes LV_PRESSING
extern "C" void action_test_brightness(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);
    uint8_t testBrightness = lv_slider_get_value(target);
    display_board_st.backLight->setBrightness(testBrightness);
}

/// @brief Save brightness on slider release, if the value is different from the currently saved one
/// @param e LV event, assumes LV_RELEASED
extern "C" void action_save_brightness(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);
    uint8_t targetBrightness = lv_slider_get_value(target);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (target == objects.dark_slider && targetBrightness != display_board_st.darkBrightness)
        {
            if (nvs_set_u8(h, "dark_bg", targetBrightness) != ESP_OK)
                ESP_LOGE(__func__, "Could not set dark mode brightness in NVS");
            else
            {
                display_board_st.darkBrightness = targetBrightness;
                lvgl_port_lock(-1);
                lv_label_set_text_fmt(objects.d_bright, "%u", display_board_st.darkBrightness);
                lvgl_port_unlock();
            }
        }
        else if (target == objects.light_slider && targetBrightness != display_board_st.lightBrightness)
        {
            if (nvs_set_u8(h, "light_bg", targetBrightness) != ESP_OK)
                ESP_LOGE(__func__, "Could not set light mode brightness in NVS");
            else
            {
                display_board_st.lightBrightness = targetBrightness;
                lvgl_port_lock(-1);
                lv_label_set_text_fmt(objects.l_bright, "%u", display_board_st.lightBrightness);
                lvgl_port_unlock();
            }
        }
        else
        {
            ESP_LOGW(__func__, "No value to set in NVS (wrong slider or same value)");
            lvgl_port_lock(-1);
            lv_label_set_text(objects.l_bright, "???");
            lv_label_set_text(objects.d_bright, "???");
            lvgl_port_unlock();
        }
        nvs_commit(h);
        nvs_close(h);

        if (display_board_st.lightMode)
            display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
        else
            display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
    }
}

/// @brief Switch to locked theme mode, saves value to NVS when on
/// @param e LV event, assumes LV_VALUE_CHANGED
extern "C" void action_mode_lock_switch_toggled(lv_event_t *e)
{
    display_board_st.modeLocked = lv_obj_has_state(objects.mode_lock_switch, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "th_locked", (uint8_t)(display_board_st.modeLocked)) != ESP_OK)
            ESP_LOGE(__func__, "Could not set theme lock in NVS");
        else
        {
            if (lvgl_port_lock(-1))
            {
                lv_obj_set_state(objects.theme_switch, LV_STATE_DISABLED, !(display_board_st.modeLocked)); // Free access to the switch for the theme
                lvgl_port_unlock();
            }
        }
        nvs_commit(h);
        nvs_close(h);
    }
    if (!(display_board_st.modeLocked))
    {
        p_headlightsOn = !headlightsOn;
    }
}

/// @brief Switch to selected light/dark mode, saves value to NVS
/// @param e LV event, assumes LV_VALUE_CHANGED
extern "C" void action_mode_switch_toggled(lv_event_t *e)
{
    display_board_st.lightMode = !(lv_obj_has_state(objects.theme_switch, LV_STATE_CHECKED));
    if (lvgl_port_lock(-1))
    {
        switch_theme(!(display_board_st.lightMode));
        lvgl_port_unlock();
    }
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "light_th", (uint8_t)(display_board_st.lightMode)) != ESP_OK)
            ESP_LOGE(__func__, "Could not set theme mode in NVS");
        nvs_commit(h);
        nvs_close(h);
    }
}

/// @brief Switch from MPH to KPH and vice versa
/// @param e LV event, assumes LV_VALUE_CHANGED
extern "C" void action_mph_switch_toggled(lv_event_t *e)
{
    display_board_st.mph_selected = lv_obj_has_state(objects.mph_on, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "mph_on", (uint8_t)(display_board_st.mph_selected)) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save mph selector status");
        nvs_commit(h);
        nvs_close(h);
    }
    if (lvgl_port_lock(-1))
    {
        if (!(display_board_st.mph_selected))
        {
            lv_scale_set_range(objects.speed_scale, 0, 2400);
            lv_scale_set_total_tick_count(objects.speed_scale, 49);
            lv_scale_set_major_tick_every(objects.speed_scale, 4);
            lv_scale_set_text_src(objects.speed_scale, speed_kph_scale_labels);
            lv_arc_set_range(objects.speed_arc, 0, 2400);
            lv_label_set_text(objects.speed_unit, "KPH");
        }
        else
        {
            lv_scale_set_range(objects.speed_scale, 0, 1600);
            lv_scale_set_total_tick_count(objects.speed_scale, 33);
            lv_scale_set_major_tick_every(objects.speed_scale, 4);
            lv_scale_set_text_src(objects.speed_scale, speed_mph_scale_labels);
            lv_arc_set_range(objects.speed_arc, 0, 1600);
            lv_label_set_text(objects.speed_unit, "MPH");
        }
        lvgl_port_unlock();
    }
    // Force refresh
    p_odometer_km = 0;
    p_trip_km = 0;
    p_speed_kph = 0;
}

#elifdef CONFIG_LEFT_SIDE_DISPLAY

/// @brief Switch to override the tubby RPM alarm mode
/// @param e LV event, assumes LV_VALUE_CHANGED
extern "C" void action_set_rpm_alarm_override(lv_event_t *e)
{
    display_board_st.rpm_alarm_override = lv_obj_has_state(objects.override_alarm_sw, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "rpm_al_overr", (uint8_t)display_board_st.rpm_alarm_override) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save rpm alarm override switch status");
        if (nvs_get_u32(h, "rpm_al_thr", &(display_board_st.rpm_alarm_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve RPM alarm threshold from NVS");
        nvs_commit(h);
        nvs_close(h);
        if (lvgl_port_lock(-1))
        {
            lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, (display_board_st.rpm_alarm_override));
            lv_obj_set_state(objects.blink_alarm_sw, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
            lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
            lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
            lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
            lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, !(display_board_st.rpm_alarm_override));
            lv_spinbox_set_value(objects.rpm_alarm_spinbox, (int32_t)display_board_st.rpm_alarm_threshold);
            lv_obj_set_state(objects.blink_alarm_sw, LV_STATE_CHECKED, display_board_st.rpm_alarm_blink);
            lvgl_port_unlock();
        }
    }
}

/// @brief Check switch to blink RPM reading when RPM is above threshold has been toggled
/// @param e LV event VALUE_CHANGED
extern "C" void action_blink_rpm_alarm_toggled(lv_event_t *e)
{
    display_board_st.rpm_alarm_blink = lv_obj_has_state(objects.blink_alarm_sw, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "rpm_al_blnk", (uint8_t)display_board_st.rpm_alarm_blink) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save rpm alarm blink switch status");
        nvs_commit(h);
        nvs_close(h);
    }
}

/// @brief Increment button for the RPM threshold spinbox
/// @param e
extern "C" void action_inc_rpm_spinbox(lv_event_t *e)
{
    lv_obj_t *target_spinbox = NULL;
    lv_obj_t *target_button = lv_event_get_target_obj(e);
    if (target_button == objects.inc_rpm_alarm_btn)
    {
        target_spinbox = objects.rpm_alarm_spinbox;
    }
    if (target_button == objects.inc_shift_mid_btn)
    {
        target_spinbox = objects.shift_mid_spinbox;
    }
    if (target_button == objects.inc_shift_top_btn)
    {
        target_spinbox = objects.shift_top_spinbox;
    }
    // Need to do some logic on limits too
    if (lvgl_port_lock(-1))
    {
        lv_spinbox_increment(target_spinbox);
        if (target_spinbox != objects.rpm_alarm_spinbox)
        {
            display_board_st.shift_mid_threshold = lv_spinbox_get_value(objects.shift_mid_spinbox);
            display_board_st.shift_top_threshold = lv_spinbox_get_value(objects.shift_top_spinbox);
            lv_spinbox_set_max_value(objects.shift_mid_spinbox, display_board_st.shift_top_threshold - 1);
            lv_spinbox_set_min_value(objects.shift_top_spinbox, display_board_st.shift_mid_threshold + 1);
        }

        lvgl_port_unlock();
    }
}

/// @brief Decrement button for the RPM threshold spinbox
/// @param e
extern "C" void action_dec_rpm_spinbox(lv_event_t *e)
{
    lv_obj_t *target_spinbox = NULL;
    lv_obj_t *target_button = lv_event_get_target_obj(e);
    if (target_button == objects.dec_rpm_alarm_btn)
    {
        target_spinbox = objects.rpm_alarm_spinbox;
    }
    if (target_button == objects.dec_shift_mid_btn)
    {
        target_spinbox = objects.shift_mid_spinbox;
    }
    if (target_button == objects.dec_shift_top_btn)
    {
        target_spinbox = objects.shift_top_spinbox;
    }

    if (lvgl_port_lock(-1))
    {
        lv_spinbox_decrement(target_spinbox);
        if (target_spinbox != objects.rpm_alarm_spinbox)
        {
            display_board_st.shift_mid_threshold = lv_spinbox_get_value(objects.shift_mid_spinbox);
            display_board_st.shift_top_threshold = lv_spinbox_get_value(objects.shift_top_spinbox);
            lv_spinbox_set_max_value(objects.shift_mid_spinbox, display_board_st.shift_top_threshold - 1);
            lv_spinbox_set_min_value(objects.shift_top_spinbox, display_board_st.shift_mid_threshold + 1);
        }
        lvgl_port_unlock();
    }
}

/// @brief Save RPM threshold value after setting it
/// @param e
extern "C" void action_save_rpm_spinbox(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);

    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (target == objects.save_rpm_alarm_btn)
        {
            display_board_st.rpm_alarm_threshold = lv_spinbox_get_value(objects.rpm_alarm_spinbox);

            if (nvs_set_u32(h, "rpm_al_thr", display_board_st.rpm_alarm_threshold) != ESP_OK)
                ESP_LOGE(__func__, "Cannot save rpm alarm override threshold");
        }
        if (target == objects.save_shift_ind_btn)
        {
            display_board_st.shift_mid_threshold = lv_spinbox_get_value(objects.shift_mid_spinbox);
            display_board_st.shift_top_threshold = lv_spinbox_get_value(objects.shift_top_spinbox);

            if (nvs_set_u32(h, "shft_mid", display_board_st.shift_mid_threshold) != ESP_OK)
                ESP_LOGE(__func__, "Cannot save shift light mid threshold");
            if (nvs_set_u32(h, "shft_top", display_board_st.shift_top_threshold) != ESP_OK)
                ESP_LOGE(__func__, "Cannot save shift light top threshold");
        }
        nvs_commit(h);
        nvs_close(h);
    }
}

/// @brief The "blinkfest" shift indicator has been checked or unchecked
/// @param e LV event VALUE_CHANGED
extern "C" void action_shift_indicator_toggled(lv_event_t *e)
{
    display_board_st.use_shift_indicator = lv_obj_has_state(objects.shift_ind_sw, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "shft_ind", (uint8_t)display_board_st.use_shift_indicator) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save shift indicator status");
        // Following two gets are not really needed
        if (nvs_get_u32(h, "shft_mid", &(display_board_st.shift_mid_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve shift mid threshold from NVS");
        if (nvs_get_u32(h, "shft_top", &(display_board_st.shift_top_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve shift top threshold from NVS");
        nvs_commit(h);
        nvs_close(h);
    }
}

/// @brief Updates the 10-100 RPM decimation indicator
/// @param e LV event VALUE CHANGED
extern "C" void action_decimation_update(lv_event_t *e)
{
    if (lv_obj_has_state(objects.decimation_sw, LV_STATE_CHECKED))
        display_board_st.rpm_decimation = 100;
    else
        display_board_st.rpm_decimation = 10;
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u32(h, "rpm_dec", display_board_st.rpm_decimation) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save RPM decimator status");

        nvs_commit(h);
        nvs_close(h);
    }
    p_rpm = 1000; // Force refresh indirectly
    rpm = 0;
}

/// @brief Updates the overtemperature buzzer enabled status
/// @param e LV even VALUE CHANGED
extern "C" void action_buzz_overtemp_toggled(lv_event_t *e)
{
    display_board_st.overTemp_buzz = lv_obj_has_state(objects.buzz_overtemp_sw, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "overt_bzz", (uint8_t)display_board_st.overTemp_buzz) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save overtemp buzzer status");
        nvs_commit(h);
        nvs_close(h);
    }
    if (!display_board_st.overTemp_buzz)
    {
        // Ensure buzzer is off
        if (display_board_st.ioExpander)
        {
            display_board_st.ioExpander->getBase()->digitalWrite(7, LOW);
        }
    }
    else if (overTemperatureOn)
    {
        // Enable buzzer if needed
        if (display_board_st.ioExpander)
        {
            display_board_st.ioExpander->getBase()->digitalWrite(7, HIGH);
        }
    }
}

#endif
#pragma endregion

#pragma region MAIN

/// @brief Wrapper function to attempt a rollback
/// @return ESP_OK if rollback scheduled, some NOT_OK error otherwise
esp_err_t attemptRollBack()
{
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

/// @brief Main app
extern "C" void app_main()
{
    // Declare board state at initialisation
    display_board_st.internal_ST = XDB_SM_ST_INIT;

#pragma region App metadata parse
    // Allocate memory for app_metadata and fill it by reference
    display_board_st.app_metadata = (parsed_app_meta_t *)malloc(sizeof(parsed_app_meta_t));
    if (display_board_st.app_metadata) // Successful allocation
    {
        esp_err_t meta_err = parse_app_metadata(display_board_st.app_metadata);
        if (meta_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to parse app metadata");
            attemptRollBack();
        }
    }
    else // Unsuccessful memory allocation
    {
        ESP_LOGE(TAG, "Failed to allocate memory for app_metadata");
        attemptRollBack();
    }
#pragma endregion

#pragma region OTA NVS ROLLBACK
    // Get Rollback status and first boot state
    rollBackPossible = esp_ota_check_rollback_is_possible();
    ESP_LOGI(__func__, "Rollback Possible ? %u", rollBackPossible);
    const esp_partition_t *runningPart = esp_ota_get_running_partition();
    esp_ota_img_states_t imageState;
    esp_ota_get_state_partition(runningPart, &imageState);
    firstBoot = (imageState == ESP_OTA_IMG_PENDING_VERIFY);
    ESP_LOGI(__func__, "First boot ? %u", firstBoot);

    // Init NVS and write last partition info (for factory app)
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err != ESP_OK)
        ESP_LOGE(__func__, "Cannot init default NVS");
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGE(__func__, "Could not init default NVS, erasing and retrying.");
        nvs_flash_erase();
        nvs_flash_init();
    }
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
    {
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
    else
    {
        if (strcmp("ota_0", runningPart->label) == 0) // If running partition is ota_0
        {
            ESP_LOGI(__func__, "Running partition is ota_0");
            nvs_set_i8(h, "lastPart", 0);
        }
        else if (strcmp("ota_1", runningPart->label) == 0) // If running partition is ota_0
        {
            ESP_LOGI(__func__, "Running partition is ota_1");
            nvs_set_i8(h, "lastPart", 1);
        }
        else
        {
            ESP_LOGW(__func__, "Current running partition could not be identified, defaulting to factory.");
            nvs_set_i8(h, "lastPart", -1);
        }

        if (nvs_get_u8(h, "mph_on", (uint8_t *)&(display_board_st.mph_selected)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve MPH status from NVS");

        if (nvs_get_u8(h, "rpm_al_overr", (uint8_t *)&(display_board_st.rpm_alarm_override)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve RPM alarm override.");
        if (nvs_get_u32(h, "rpm_al_thr", &(display_board_st.rpm_alarm_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve the RPM alarm threshold value.");
        if (nvs_get_u8(h, "rpm_al_blnk", (uint8_t *)&(display_board_st.rpm_alarm_blink)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve the RPM alarm blinker status from NVS");
        if (nvs_get_u8(h, "shft_ind", (uint8_t *)&(display_board_st.use_shift_indicator)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve the shift indicator status from NVS");
        if (nvs_get_u32(h, "shft_mid", &(display_board_st.shift_mid_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve shift mid threshold from NVS");
        if (nvs_get_u32(h, "shft_top", &(display_board_st.shift_top_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve shift top threshold from NVS");
        if (nvs_get_u32(h, "rpm_dec", &(display_board_st.rpm_decimation)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve RPM decimation factor from NVS");

        if (nvs_get_u8(h, "overt_bzz", (uint8_t *)&(display_board_st.overTemp_buzz)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve overtemp buzzer status from NVS");

        if (nvs_get_u8(h, "dark_bg", &(display_board_st.darkBrightness)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve the dark mode brightness value.");
        if (nvs_get_u8(h, "light_bg", &(display_board_st.lightBrightness)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve the light mode brightness value.");
        if (nvs_get_u8(h, "th_locked", (uint8_t *)&(display_board_st.modeLocked)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve the mode lock indicator.");
        if (display_board_st.modeLocked)
            if (nvs_get_u8(h, "light_th", (uint8_t *)&(display_board_st.lightMode)) != ESP_OK)
                ESP_LOGW(__func__, "Could not retrieve light mode status.");
        nvs_commit(h);
        nvs_close(h);
    }

    // Retrieve app metadata to display on the debug screen
    const esp_app_desc_t *app_metadata;
    app_metadata = esp_app_get_description();
    if (!app_metadata)
    {
        ESP_LOGE(TAG, "No app metadata available");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
#pragma endregion

#pragma region CAN START
    // Pre-empt the CAN TO timers
    ESP_LOGI(__func__, "Starting CAN message timeout watchdogs");
    if (TO_timers_init() != ESP_OK)
    {
        ESP_LOGW(__func__, "Impossible to init CAN timeout watchdog timers");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
    // CAN communications
    ESP_LOGI(__func__, "Starting TWAI port and daemon");
    if (initCAN(&dispatchFrame) != ESP_OK)
    {
        ESP_LOGW(__func__, "Issue starting TWAI port and daemon.");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
    // Set up and start CAN packagers
    if (xTaskCreatePinnedToCore(display_board_st_PKG, "XDB_ST", 4096, NULL, 3, &display_board_st_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create display state package task");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
    if (xTaskCreatePinnedToCore(display_board_version_PKG, "XDB_VER", 4096, NULL, 3, &display_board_version_PKG_hdl, CONFIG_CAN_CORE_AFFINITY) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create display version package task");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
#pragma endregion

#pragma region BOARD INIT
    // Board initialization
    ESP_LOGI(__func__, "Initializing board");

    Board *board = new Board();
    if (!(board->init()))
    {
        ESP_LOGW(__func__, "Could not initialize board.");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
    else
    {
        auto lcd = board->getLCD();
        if (!(lcd->configFrameBufferNumber(LVGL_PORT_BUFFER_NUM)))
        {
            ESP_LOGW(__func__, "Could not set up framebuffers.");
            display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
            attemptRollBack();
        }
        else
        {
            // Setting up the Bounce Buffer size (might not be necessary)
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
            auto lcd_bus = lcd->getBus();
            if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB)
            {
                if (!(static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 20)))
                {
                    ESP_LOGW(__func__, "Could not set up bounce buffer.");
                    display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
                    attemptRollBack();
                }
            }
#endif
        }
        // Board start
        if (!(board->begin()))
        {
            ESP_LOGW(__func__, "Could not begin() board.");
            display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
            attemptRollBack();
        }
        else
        {
            // Get pointer to the Backlight class
            display_board_st.backLight = board->getBacklight();
            if (display_board_st.lightMode)
            {
                display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
            }
            else
            {
                display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
            }
            // Get pointer to the IO Expander class
            display_board_st.ioExpander = board->getIO_Expander();
            display_board_st.ioExpander->getBase()->pinMode(7, OUTPUT);
            if (!display_board_st.overTemp_buzz && overTemperatureOn)
            {
                display_board_st.ioExpander->getBase()->digitalWrite(7, HIGH);
            }
            else
            {
                display_board_st.ioExpander->getBase()->digitalWrite(7, LOW);
            }
        }
    }
#pragma endregion

    // LVGL Port init and link
    if (!(lvgl_port_init(board->getLCD(), board->getTouch())))
    {
        ESP_LOGW(__func__, "Could not start LVGL port.");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }

#pragma region Starting animation
    if (lvgl_port_lock(-1))
    {
        ui_init(); // Load the UI library and draw it
        if (display_board_st.modeLocked)
            switch_theme(!(display_board_st.lightMode));
        // Set up the debug screen
        lv_label_set_text_fmt(objects.version_info, "%s - %s - %s", app_metadata->version, app_metadata->date, app_metadata->time);
        lv_label_set_text_fmt(objects.project_info, "%s", app_metadata->project_name);
        lv_label_set_text_fmt(objects.current_partition, "%s", runningPart->label);

        startup_anim();
    }
    if (initBlinkTimer() != ESP_OK)
        ESP_LOGW(__func__, "Blinking timer could not be started.");

#pragma endregion

    ESP_LOGI(__func__, "Setup done");
    if (display_board_st.internal_ST != XDB_SM_ST_DEGRADED)
    {
        esp_ota_mark_app_valid_cancel_rollback();
        display_board_st.internal_ST = XDB_SM_ST_OK;
        ESP_LOGI(__func__, "App image is valid.");

        if (firstBoot)
        {
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
            if (lvgl_port_lock(-1))
            {
                lv_obj_add_state(objects.speed, LV_STATE_CHECKED);
                lv_label_set_text(objects.speed, "OTA OK");
                lvgl_port_unlock();
                vTaskDelay(pdMS_TO_TICKS(2000));
                if (lvgl_port_lock(-1))
                {
                    lv_obj_remove_state(objects.speed, LV_STATE_CHECKED);
                    lv_label_set_text(objects.speed, "0");
                    updateLVGLObjects(true);
                    lvgl_port_unlock();
                }
            }
#elifdef CONFIG_LEFT_SIDE_DISPLAY
            if (lvgl_port_lock(-1))
            {
                lv_obj_add_state(objects.rpm, LV_STATE_CHECKED);
                lv_label_set_text(objects.rpm, "OTA OK");
                lvgl_port_unlock();
                vTaskDelay(pdMS_TO_TICKS(2000));
                if (lvgl_port_lock(-1))
                {
                    lv_obj_remove_state(objects.rpm, LV_STATE_CHECKED);
                    lv_label_set_text(objects.rpm, "0");
                    updateLVGLObjects(true);
                    lvgl_port_unlock();
                }
            }
#endif
        }
    }
    vTaskResume(CAN_RX_tsk_hdl);
    TO_timers_start();

#ifdef CONFIG_ENABLE_RUNTIME_STATS_OUTPUT
    xTaskCreate(print_system_stats, "RUNSTATS", 4096, NULL, 1, &print_runtime_stats_Hdl);
#endif

#pragma region Main Loop
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_DATA_REFRESH_INTERVAL));
        // Attempt locking LVGL elements prior to updating them (issue with jumping frames ?)
        if (lvgl_port_lock(-1))
        {
            updateLVGLObjects();
            lvgl_port_unlock();
        }
    }
#pragma endregion
}