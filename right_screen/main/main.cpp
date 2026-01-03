#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_display_panel.hpp"
#include <lvgl.h>
#include "lvgl_v9_port.h"
#include <ui.h>
#include <styles.h>

#include <math.h>
#include "twai_daemon.h"
#include "binocan.h"
#include "coefficients.h"
#include "version_parser.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_err.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

#include "global_vars.hpp"
#include "theme.hpp"
#include "updateUI.hpp"
#include "twai_ops.hpp"
#include "start_animation.hpp"

// Refresh interval to the LVGL objects
#ifndef DISP_VALUES_REFRESH_INTERVAL
#define DISP_VALUES_REFRESH_INTERVAL 25
#endif

using namespace esp_panel::drivers;
using namespace esp_panel::board;

#ifdef TAG
#undef TAG
#endif
#define TAG "Main"

#pragma region ACTIONS
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

#ifdef CONFIG_RIGHT_SIDE_DISPLAY

extern "C" void action_test_brightness(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);
    uint8_t testBrightness = lv_slider_get_value(target);
    display_board_st.backLight->setBrightness(testBrightness);
}

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

extern "C" void action_mph_switch_toggled(lv_event_t *e)
{

    display_board_st.mph_selected = lv_obj_has_state(objects.mph_on, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "mph_on", display_board_st.mph_selected) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save mph selector status");
        nvs_commit(h);
        nvs_close(h);
    }
    lvgl_port_lock(-1);
    if (display_board_st.mph_selected == 0)
    {
        lv_scale_set_range(objects.speed_scale, 0, 2400);
        lv_scale_set_total_tick_count(objects.speed_scale, 49);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_kph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 2400);
        lv_label_set_text(objects.speed_unit, "KPH");
    }
    else if (display_board_st.mph_selected == 1)
    {
        lv_scale_set_range(objects.speed_scale, 0, 1600);
        lv_scale_set_total_tick_count(objects.speed_scale, 33);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_mph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 1600);
        lv_label_set_text(objects.speed_unit, "MPH");
    }
    lvgl_port_unlock();
    // Force refresh
    p_odometer_km = 0;
    p_trip_km = 0;
    p_speed_kph = 0;
}

#elifdef CONFIG_LEFT_SIDE_DISPLAY

extern "C" void action_set_rpm_alarm_override(lv_event_t *e)
{
    display_board_st.rpm_alarm_override = lv_obj_has_state(objects.override_alarm_sw, LV_STATE_CHECKED);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u8(h, "rpm_al_overr", display_board_st.rpm_alarm_override) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save rpm alarm override switch status");
    }
    lvgl_port_lock(-1);
    switch (display_board_st.rpm_alarm_override)
    {
    case 1:
        lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, false);
        if (nvs_get_u32(h, "rpm_al_thr", &(display_board_st.rpm_alarm_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve RPM alarm threshold from NVS");
        lv_spinbox_set_value(objects.rpm_alarm_spinbox, (int32_t)display_board_st.rpm_alarm_threshold);
        break;
    case 0:
        lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_spinbox_set_value(objects.rpm_alarm_spinbox, 0);
        break;
    default:
        ESP_LOGE(__func__, "Unauthorized RPM alarm override state");
        break;
    }
    nvs_commit(h);
    nvs_close(h);
    lvgl_port_unlock();
}

extern "C" void action_inc_rpm_spinbox(lv_event_t *e)
{
    lvgl_port_lock(-1);
    lv_spinbox_increment(objects.rpm_alarm_spinbox);
    lvgl_port_unlock();
}

extern "C" void action_dec_rpm_spinbox(lv_event_t *e)
{
    lvgl_port_lock(-1);
    lv_spinbox_decrement(objects.rpm_alarm_spinbox);
    lvgl_port_unlock();
}

extern "C" void action_save_rpm_spinbox(lv_event_t *e)
{
    display_board_st.rpm_alarm_threshold = lv_spinbox_get_value(objects.rpm_alarm_spinbox);
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
        ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
    else
    {
        if (nvs_set_u32(h, "rpm_al_thr", display_board_st.rpm_alarm_threshold) != ESP_OK)
            ESP_LOGE(__func__, "Cannot save rpm alarm override threshold");
        nvs_commit(h);
        nvs_close(h);
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
        if (nvs_get_u8(h, "mph_on", &(display_board_st.mph_selected)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve MPH status from NVS");
        if (nvs_get_u8(h, "rpm_al_overr", &(display_board_st.rpm_alarm_override)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve RPM alarm override.");
        if (nvs_get_u32(h, "rpm_al_thr", &(display_board_st.rpm_alarm_threshold)) != ESP_OK)
            ESP_LOGW(__func__, "Could not retrieve the RPM alarm threshold value.");
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
    // CAN communications
    ESP_LOGI(__func__, "Starting TWAI port and daemon");
    if (initCAN(&dispatchFrame) != ESP_OK)
    {
        ESP_LOGW(__func__, "Issue starting TWAI port and daemon.");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
    // Set up and start CAN packagers
    if (xTaskCreate(display_board_st_PKG, "XDB_ST", 4096, NULL, 3, &display_board_st_PKG_hdl) != pdPASS)
    {
        ESP_LOGE(__func__, "Could not create display state package task");
        display_board_st.internal_ST = XDB_SM_ST_DEGRADED;
        attemptRollBack();
    }
    if (xTaskCreate(display_board_version_PKG, "XDB_VER", 4096, NULL, 3, &display_board_version_PKG_hdl) != pdPASS)
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
            display_board_st.backLight = board->getBacklight();
            if (display_board_st.lightMode)
            {
                display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
            }
            else
            {
                display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
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
    // UI loading and mofidifiers
    ESP_LOGI(__func__, "Loading UI");
    ESP_UTILS_CHECK_FALSE_EXIT(lvgl_port_lock(-1), "Failed to perform initial LVGL Mutex lock");
    ui_init(); // Load the UI library and draw it
    if (display_board_st.modeLocked)
        switch_theme(!(display_board_st.lightMode));
    // Set up the debug screen
    lv_label_set_text_fmt(objects.version_info, "%s - %s - %s", app_metadata->version, app_metadata->date, app_metadata->time);
    lv_label_set_text_fmt(objects.project_info, "%s", app_metadata->project_name);
    lv_label_set_text_fmt(objects.current_partition, "%s", runningPart->label);

    startup_anim();
/* 
#ifdef CONFIG_LEFT_SIDE_DISPLAY
    lv_obj_set_style_pad_radial(objects.rpm_scale, 20, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    lv_scale_set_text_src(objects.rpm_scale, rpm_scale_labels);

    switch (display_board_st.rpm_alarm_override)
    {
    case 1:
        lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, true);
        lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, false);
        lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, false);
        lv_spinbox_set_value(objects.rpm_alarm_spinbox, (int32_t)display_board_st.rpm_alarm_threshold);
        break;
    case 0:
        lv_obj_set_state(objects.override_alarm_sw, LV_STATE_CHECKED, false);
        lv_obj_set_state(objects.dec_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.inc_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.rpm_alarm_spinbox, LV_STATE_DISABLED, true);
        lv_obj_set_state(objects.save_rpm_alarm_btn, LV_STATE_DISABLED, true);
        lv_spinbox_set_value(objects.rpm_alarm_spinbox, 0);
        break;
    default:
        ESP_LOGE(__func__, "Unauthorized RPM alarm override state");
        break;
    }

#elifdef CONFIG_RIGHT_SIDE_DISPLAY
    lv_slider_set_value(objects.dark_slider, display_board_st.darkBrightness, LV_ANIM_OFF);
    lv_slider_set_value(objects.light_slider, display_board_st.lightBrightness, LV_ANIM_OFF);
    lv_label_set_text_fmt(objects.l_bright, "%u", display_board_st.lightBrightness);
    lv_label_set_text_fmt(objects.d_bright, "%u", display_board_st.darkBrightness);
    lv_obj_set_state(objects.mode_lock_switch, LV_STATE_CHECKED, display_board_st.modeLocked);
    lv_obj_set_state(objects.theme_switch, LV_STATE_DISABLED, !(display_board_st.modeLocked));
    lv_obj_set_state(objects.theme_switch, LV_STATE_CHECKED, !(display_board_st.lightMode));

    lv_obj_set_style_pad_radial(objects.speed_scale, 15, LV_PART_INDICATOR); // Pad the scale labels away from the tick marks
    if (display_board_st.mph_selected == 0)
    {
        lv_scale_set_range(objects.speed_scale, 0, 2400);
        lv_scale_set_total_tick_count(objects.speed_scale, 49);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_kph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 2400);
        lv_label_set_text(objects.speed_unit, "KPH");
        lv_obj_set_state(objects.mph_on, LV_STATE_CHECKED, false);
    }
    else if (display_board_st.mph_selected == 1)
    {
        lv_scale_set_range(objects.speed_scale, 0, 1600);
        lv_scale_set_total_tick_count(objects.speed_scale, 33);
        lv_scale_set_major_tick_every(objects.speed_scale, 4);
        lv_scale_set_text_src(objects.speed_scale, speed_mph_scale_labels);
        lv_arc_set_range(objects.speed_arc, 0, 1600);
        lv_label_set_text(objects.speed_unit, "MPH");
        lv_obj_set_state(objects.mph_on, LV_STATE_CHECKED, true);
    }
#endif

    vTaskSuspend(CAN_RX_tsk_hdl);
    // Prepare values for the starting/check sequence
    p_screen_interlock_OK = !screen_interlock_OK;
    lv_obj_set_style_opa(objects.interlock_state, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_internal_ST = 0xFF;
    lv_obj_set_style_opa(objects.internal_state, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_itf_board_st = 0xFF;
    lv_obj_set_style_opa(objects.itf_state, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_CAN_RX_TimedOut = !CAN_RX_TimedOut;
    lv_obj_set_style_opa(objects.can_state, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_indicatorsOn = !indicatorsOn;
    lv_obj_set_style_opa(objects.indicators_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_rightTurnOn = !rightTurnOn;
    p_leftTurnOn = !leftTurnOn;
    p_highBeamOn = !highBeamOn;
    lv_obj_set_style_opa(objects.hi_beam_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_lowFuelOn = !lowFuelOn;
    lv_obj_set_style_opa(objects.low_fuel_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_overTemperatureOn = !overTemperatureOn;
    lv_obj_set_style_opa(objects.over_temperature_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_brakesOn = !brakesOn;
    lv_obj_set_style_opa(objects.brakes_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_absOn = !absOn;
    lv_obj_set_style_opa(objects.abs_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_parkingBrakeOn = !parkingBrakeOn;
    lv_obj_set_style_opa(objects.parkingbrake_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_lowCoolantOn = !lowCoolantOn;
    lv_obj_set_style_opa(objects.low_coolant_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_batteryOn = !batteryOn;
    lv_obj_set_style_opa(objects.battery_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_lowOilOn = !lowOilOn;
    lv_obj_set_style_opa(objects.low_oil_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_milOn = !milOn;
    lv_obj_set_style_opa(objects.mil_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    p_airbagOn = !airbagOn;
    lv_obj_set_style_opa(objects.airbag_tt, LV_OPA_COVER, LV_STATE_DEFAULT);
    lvgl_port_unlock();
    vTaskDelay(100);
    // ESP_LOGI(__func__, "Backlight : %d", board->getBacklight()->on());
    if (display_board_st.lightMode)
        display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
    else
        display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
    // This probably needs to be called in a second point
    lvgl_port_lock(-1);
#ifdef CONFIG_LEFT_SIDE_DISPLAY
    animateTargetArcWithDuration(objects.rpm_arc, 8000, 1000);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(1600));
    lvgl_port_lock(-1);
    animateTargetArcWithDuration(objects.rpm_arc, 0, 500);
#elifdef CONFIG_RIGHT_SIDE_DISPLAY
    animateTargetArcWithDuration(objects.speed_arc, 2400, 1000);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(1600));
    lvgl_port_lock(-1);
    animateTargetArcWithDuration(objects.speed_arc, 0, 500);
#endif
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(600));
    lvgl_port_lock(-1);
    updateLVGLObjects(true);
    lvgl_port_unlock(); */
#pragma endregion
    ESP_LOGI(__func__, "Setup done");
    if (display_board_st.internal_ST != XDB_SM_ST_DEGRADED)
    {
        esp_ota_mark_app_valid_cancel_rollback();
        display_board_st.internal_ST = XDB_SM_ST_OK;
        ESP_LOGI(__func__, "App image is valid.");
    }
    vTaskResume(CAN_RX_tsk_hdl);
#pragma region Main Loop
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(DISP_VALUES_REFRESH_INTERVAL));
        // Attempt locking LVGL elements prior to updating them (issue with jumping frames ?)
        if (lvgl_port_lock(-1))
        {
            updateLVGLObjects();
            lvgl_port_unlock();
        }
    }
#pragma endregion
}