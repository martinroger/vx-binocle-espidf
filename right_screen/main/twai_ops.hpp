#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_ota_ops.h"

#include "global_vars.hpp"
#include "twai_daemon.h"
#include "binocan.h"

TimerHandle_t alt_display_st_TO_hdl; // Alternative display timeout timer handle
TimerHandle_t OTA_TO_hdl;            // OTA message timeout timer handle

/// @brief Callback for alternate display status timeout
/// @param xTimer
inline void alt_display_st_TO_cb(TimerHandle_t xTimer)
{
    // Only set the screen interlock as NOK
    screen_interlock_OK = false;
    // Assumes this is a one-shot timer so it's stopped.
}

#pragma region SPECIFIC
#ifdef CONFIG_RIGHT_SIDE_DISPLAY
#define UDS_RESP_FRAME_ID BINOCAN_RDB_UDS_RESP_FRAME_ID
#define UDS_REQ_FRAME_ID BINOCAN_RDB_UDS_REQ_FRAME_ID
#define UDS_FRAME_OBJ static binocan_rdb_uds_req_t binocan_rdb_uds_req_msg;
#define UDS_FRAME_UNPACK binocan_rdb_uds_req_unpack(&binocan_rdb_uds_req_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL
#define ALT_DISPLAY_ST_FRAME_ID BINOCAN_LDB_ST_FRAME_ID

inline esp_err_t alt_display_st_Handler(const twai_frame_t *rxMsg)
{
    static binocan_ldb_st_t binocan_ldb_st_msg;
    if (binocan_ldb_st_unpack(&binocan_ldb_st_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }
    // General state message check
    if (binocan_ldb_st_ldb_sm_st_is_in_range(binocan_ldb_st_msg.ldb_sm_st))
    {
        screen_interlock_OK = ((uint8_t)binocan_ldb_st_ldb_sm_st_decode(binocan_ldb_st_msg.ldb_sm_st) == XDB_SM_ST_OK);
    }
    else
    {
        ESP_LOGW(__func__, "LDB SM Status signal out of range: %d", binocan_ldb_st_msg.ldb_sm_st);
        screen_interlock_OK = false; // Default value
    }
    return ESP_OK;
}

/// @brief Packaging handler that send the board state over CAN
/// @param pvParameters
inline void display_board_st_PKG(void *pvParameters)
{
    binocan_rdb_st_t binocan_rdb_st;
    binocan_rdb_st_init(&binocan_rdb_st);
    uint8_t payload[BINOCAN_RDB_ST_LENGTH] = {0};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_RDB_ST_CYCLE_TIME_MS));
        binocan_rdb_st.rdb_sm_st = binocan_rdb_st_rdb_sm_st_encode(display_board_st.internal_ST);
        binocan_rdb_st.xdb_dark_brightness = binocan_rdb_st_xdb_dark_brightness_encode(display_board_st.darkBrightness);
        binocan_rdb_st.xdb_light_brightness = binocan_rdb_st_xdb_light_brightness_encode(display_board_st.lightBrightness);
        binocan_rdb_st.xdb_light_mode = binocan_rdb_st_xdb_light_mode_encode(display_board_st.lightMode);
        binocan_rdb_st.xdb_mode_lock = binocan_rdb_st_xdb_mode_lock_encode(display_board_st.modeLocked);
        binocan_rdb_st_pack(payload, &binocan_rdb_st, BINOCAN_RDB_ST_LENGTH);
        if (twai_transmit_msg(BINOCAN_RDB_ST_FRAME_ID, payload, BINOCAN_RDB_ST_LENGTH, false, 5) != ESP_OK)
        {
            ESP_LOGD(TAG, "Could not transmit display state message");
        }
    }
}

inline void display_board_version_PKG(void *pvParameters)
{
    uint8_t message_mux = 0;
    binocan_rdb_board_version_t binocan_rdb_board_version;
    binocan_rdb_board_version_init(&binocan_rdb_board_version);
    uint8_t payload[BINOCAN_RDB_BOARD_VERSION_LENGTH] = {0};
    uint32_t cycle_time_ms = BINOCAN_RDB_BOARD_VERSION_CYCLE_TIME_MS;

    binocan_rdb_board_version.rdb_version_major = binocan_rdb_board_version_rdb_version_major_encode((uint8_t)(display_board_st.app_metadata->base_version[0]) - 48);
    binocan_rdb_board_version.rdb_version_minor = binocan_rdb_board_version_rdb_version_minor_encode((uint8_t)(display_board_st.app_metadata->base_version[2]) - 48);
    binocan_rdb_board_version.rdb_version_patch = binocan_rdb_board_version_rdb_version_patch_encode((uint8_t)(display_board_st.app_metadata->base_version[4]) - 48);
    binocan_rdb_board_version.rdb_version_dirty = binocan_rdb_board_version_rdb_version_dirty_encode((uint8_t)display_board_st.app_metadata->is_dirty);
    // Build a 64-bit value from up to 8 bytes of commitID and pass to the encode helper.
    if (display_board_st.app_metadata && display_board_st.app_metadata->commitID)
    {
        uint64_t commit_val = 0;
        size_t src_len = display_board_st.app_metadata->commit_len;
        if (src_len > 8)
            src_len = 8;
        for (size_t i = 0; i < src_len; ++i)
        {
            commit_val = (commit_val << 8) | (uint8_t)display_board_st.app_metadata->commitID[src_len - i - 1];
        }
        binocan_rdb_board_version.rdb_version_commit = binocan_rdb_board_version_rdb_version_commit_encode(commit_val);
    }
    else
    {
        binocan_rdb_board_version.rdb_version_commit = binocan_rdb_board_version_rdb_version_commit_encode(0ULL);
    }
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(cycle_time_ms));
        // Switch the mux indicator
        message_mux = (message_mux + 1) % 2; // Currently only two values to the MUX
        binocan_rdb_board_version.rdb_version_mux = binocan_rdb_board_version_rdb_version_mux_encode(message_mux);
        binocan_rdb_board_version_pack(payload, &binocan_rdb_board_version, BINOCAN_RDB_BOARD_VERSION_LENGTH);
        if (twai_transmit_msg(BINOCAN_RDB_BOARD_VERSION_FRAME_ID, payload, BINOCAN_RDB_BOARD_VERSION_LENGTH, false, 5) != ESP_OK)
        {
            ESP_LOGD(TAG, "Could not transmit display version message");
        }
    }
}

#elifdef CONFIG_LEFT_SIDE_DISPLAY
#define UDS_RESP_FRAME_ID BINOCAN_LDB_UDS_RESP_FRAME_ID
#define UDS_REQ_FRAME_ID BINOCAN_LDB_UDS_REQ_FRAME_ID
#define UDS_FRAME_OBJ static binocan_ldb_uds_req_t binocan_ldb_uds_req_msg;
#define UDS_FRAME_UNPACK binocan_ldb_uds_req_unpack(&binocan_ldb_uds_req_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL
#define ALT_DISPLAY_ST_FRAME_ID BINOCAN_RDB_ST_FRAME_ID

inline esp_err_t alt_display_st_Handler(const twai_frame_t *rxMsg)
{
    static binocan_rdb_st_t binocan_rdb_st_msg;
    if (binocan_rdb_st_unpack(&binocan_rdb_st_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }
    if (binocan_rdb_st_rdb_sm_st_is_in_range(binocan_rdb_st_msg.rdb_sm_st))
    {
        screen_interlock_OK = ((uint8_t)binocan_rdb_st_rdb_sm_st_decode(binocan_rdb_st_msg.rdb_sm_st) == XDB_SM_ST_OK);
    }
    else
    {
        ESP_LOGW(__func__, "RDB SM Status signal out of range: %d", binocan_rdb_st_msg.rdb_sm_st);
        screen_interlock_OK = false; // Default value
    }
    // Light mode indicator
    if (binocan_rdb_st_xdb_light_mode_is_in_range(binocan_rdb_st_msg.xdb_light_mode))
    {
        if ((display_board_st.lightMode != (bool)binocan_rdb_st_xdb_light_mode_decode(binocan_rdb_st_msg.xdb_light_mode))) // somehow they are different, after init phase
        {

            display_board_st.lightMode = (bool)binocan_rdb_st_xdb_light_mode_decode(binocan_rdb_st_msg.xdb_light_mode);
            switch_theme(!(display_board_st.lightMode), true);

            if (display_board_st.modeLocked) // Write light mode if mode lock is engaged
            {
                nvs_handle_t h;
                if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
                    ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
                else
                {
                    if (nvs_set_u8(h, "light_th", (uint8_t)(display_board_st.lightMode)) != ESP_OK)
                        ESP_LOGW(__func__, "Could not set light mode in NVS");
                    nvs_commit(h);
                    nvs_close(h);
                }
            }
        }
    }
    else
    {
        ESP_LOGW(__func__, "RDB light mode out of range: %d", binocan_rdb_st_msg.xdb_light_mode);
    }
    // Mode lock
    if (binocan_rdb_st_xdb_mode_lock_is_in_range(binocan_rdb_st_msg.xdb_mode_lock))
    {
        if (display_board_st.modeLocked != (bool)binocan_rdb_st_xdb_light_mode_decode(binocan_rdb_st_msg.xdb_mode_lock)) // Mode lock is different
        {
            nvs_handle_t h;
            if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
                ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
            else
            {
                if (nvs_set_u8(h, "th_locked", (uint8_t)(binocan_rdb_st_xdb_light_mode_decode(binocan_rdb_st_msg.xdb_mode_lock))) != ESP_OK) // Write mode lock to NVS
                    ESP_LOGW(__func__, "Could not set theme lock in NVS");
                else
                {
                    display_board_st.modeLocked = (bool)binocan_rdb_st_xdb_light_mode_decode(binocan_rdb_st_msg.xdb_mode_lock); // Update mode lock system variable
                    if (display_board_st.modeLocked)                                                                            // Write light mode if mode lock is engaged
                    {
                        if (nvs_set_u8(h, "light_th", (uint8_t)(display_board_st.lightMode)) != ESP_OK)
                            ESP_LOGW(__func__, "Could not set light mode in NVS");
                    }
                }
                nvs_commit(h);
                nvs_close(h);
            }
        }
    }
    else
    {
        ESP_LOGW(__func__, "RDB mode lock out of range: %d", binocan_rdb_st_msg.xdb_mode_lock);
    }
    // Dark brightness
    if (binocan_rdb_st_xdb_dark_brightness_is_in_range(binocan_rdb_st_msg.xdb_dark_brightness))
    {
        uint8_t newBrightness = binocan_rdb_st_xdb_dark_brightness_decode(binocan_rdb_st_msg.xdb_dark_brightness);
        if (newBrightness != display_board_st.darkBrightness)
        {
            nvs_handle_t h;
            if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
                ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
            else
            {
                if (nvs_set_u8(h, "dark_bg", newBrightness) != ESP_OK)
                    ESP_LOGW(__func__, "Could not set dark brightness value in NVS");
                else
                {
                    display_board_st.darkBrightness = newBrightness;
                    if (!(display_board_st.lightMode))
                        display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
                }
            }
        }
    }
    else
    {
        ESP_LOGW(__func__, "RDB dark brightness out of range: %d", binocan_rdb_st_msg.xdb_dark_brightness);
        display_board_st.darkBrightness = 100;
        if (!(display_board_st.lightMode))
            display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
    }
    // Light brightness
    if (binocan_rdb_st_xdb_light_brightness_is_in_range(binocan_rdb_st_msg.xdb_light_brightness))
    {
        uint8_t newBrightness = binocan_rdb_st_xdb_light_brightness_decode(binocan_rdb_st_msg.xdb_light_brightness);
        if (newBrightness != display_board_st.lightBrightness)
        {
            nvs_handle_t h;
            if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK)
                ESP_LOGE(__func__, "Cannot get into storage namespace of default NVS");
            else
            {
                if (nvs_set_u8(h, "light_bg", newBrightness) != ESP_OK)
                    ESP_LOGW(__func__, "Could not set light brightness value in NVS");
                else
                {
                    display_board_st.lightBrightness = newBrightness;
                    if ((display_board_st.lightMode))
                        display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
                }
            }
        }
    }
    else
    {
        ESP_LOGW(__func__, "RDB light brightness out of range: %d", binocan_rdb_st_msg.xdb_light_brightness);
        display_board_st.lightBrightness = 100;
        if ((display_board_st.lightMode))
            display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
    }
    return ESP_OK;
}

/// @brief Packaging handler that send the board state over CAN
/// @param pvParameters
inline void display_board_st_PKG(void *pvParameters)
{
    binocan_ldb_st_t binocan_ldb_st;
    binocan_ldb_st_init(&binocan_ldb_st);
    uint8_t payload[BINOCAN_LDB_ST_LENGTH] = {0};

    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BINOCAN_LDB_ST_CYCLE_TIME_MS));
        binocan_ldb_st.ldb_sm_st = binocan_ldb_st_ldb_sm_st_encode(display_board_st.internal_ST);
        binocan_ldb_st.xdb_dark_brightness = binocan_ldb_st_xdb_dark_brightness_encode(display_board_st.darkBrightness);
        binocan_ldb_st.xdb_light_brightness = binocan_ldb_st_xdb_light_brightness_encode(display_board_st.lightBrightness);
        binocan_ldb_st.xdb_light_mode = binocan_ldb_st_xdb_light_mode_encode(display_board_st.lightMode);
        binocan_ldb_st.xdb_mode_lock = binocan_ldb_st_xdb_mode_lock_encode(display_board_st.modeLocked);
        binocan_ldb_st_pack(payload, &binocan_ldb_st, BINOCAN_LDB_ST_LENGTH);
        if (twai_transmit_msg(BINOCAN_LDB_ST_FRAME_ID, payload, BINOCAN_LDB_ST_LENGTH, false, 5) != ESP_OK)
        {
            ESP_LOGD(TAG, "Could not transmit display state message");
        }
    }
}

inline void display_board_version_PKG(void *pvParameters)
{
    uint8_t message_mux = 0;
    binocan_ldb_board_version_t binocan_ldb_board_version;
    binocan_ldb_board_version_init(&binocan_ldb_board_version);
    uint8_t payload[BINOCAN_LDB_BOARD_VERSION_LENGTH] = {0};
    uint32_t cycle_time_ms = BINOCAN_LDB_BOARD_VERSION_CYCLE_TIME_MS;

    binocan_ldb_board_version.ldb_version_major = binocan_ldb_board_version_ldb_version_major_encode((uint8_t)(display_board_st.app_metadata->base_version[0]) - 48);
    binocan_ldb_board_version.ldb_version_minor = binocan_ldb_board_version_ldb_version_minor_encode((uint8_t)(display_board_st.app_metadata->base_version[2]) - 48);
    binocan_ldb_board_version.ldb_version_patch = binocan_ldb_board_version_ldb_version_patch_encode((uint8_t)(display_board_st.app_metadata->base_version[4]) - 48);
    binocan_ldb_board_version.ldb_version_dirty = binocan_ldb_board_version_ldb_version_dirty_encode((uint8_t)display_board_st.app_metadata->is_dirty);
    // Build a 64-bit value from up to 8 bytes of commitID and pass to the encode helper.
    if (display_board_st.app_metadata && display_board_st.app_metadata->commitID)
    {
        uint64_t commit_val = 0;
        size_t src_len = display_board_st.app_metadata->commit_len;
        if (src_len > 8)
            src_len = 8;
        for (size_t i = 0; i < src_len; ++i)
        {
            commit_val = (commit_val << 8) | (uint8_t)display_board_st.app_metadata->commitID[src_len - i - 1];
        }
        binocan_ldb_board_version.ldb_version_commit = binocan_ldb_board_version_ldb_version_commit_encode(commit_val);
    }
    else
    {
        binocan_ldb_board_version.ldb_version_commit = binocan_ldb_board_version_ldb_version_commit_encode(0ULL);
    }
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(cycle_time_ms));
        // Switch the mux indicator
        message_mux = (message_mux + 1) % 2; // Currently only two values to the MUX
        binocan_ldb_board_version.ldb_version_mux = binocan_ldb_board_version_ldb_version_mux_encode(message_mux);
        binocan_ldb_board_version_pack(payload, &binocan_ldb_board_version, BINOCAN_LDB_BOARD_VERSION_LENGTH);
        if (twai_transmit_msg(BINOCAN_LDB_BOARD_VERSION_FRAME_ID, payload, BINOCAN_LDB_BOARD_VERSION_LENGTH, false, 5) != ESP_OK)
        {
            ESP_LOGD(TAG, "Could not transmit display version message");
        }
    }
}

#endif
#pragma endregion

#pragma region COMMON

TaskHandle_t display_board_st_PKG_hdl;
TaskHandle_t display_board_version_PKG_hdl;


// Timeout callbacks
// Bit ugly ...
esp_err_t OTAHandler(const twai_frame_t *rxMsg);

inline void OTA_TO_cb(TimerHandle_t xTimer)
{
    ESP_LOGW(__func__, "OTA Timeout");
    OTAHandler(NULL);
}

inline void itf_active_hi_lo_TO_cb(TimerHandle_t xTimer)
{
    absOn = true;
    airbagOn = true;
    milOn = true;
    highBeamOn = true;
    brakesOn = true;
    lowCoolantOn = true;
    lowFuelOn = true;
    lowOilOn = true;
    batteryOn = true;
    overTemperatureOn = true;
    parkingBrakeOn = true;
    indicatorsOn = true;
    leftTurnOn = true;
    rightTurnOn = true;
    alarmOn = true;
    headlightsOn = true;
    itf_board_st = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_FAULT_CHOICE;
}

inline void itf_slow_metrics_TO_cb(TimerHandle_t xTimer)
{
    coolant_degC = UINT8_MAX;
    fuelLevel_pc = 0;
    lvVoltage_v = 0;
    batteryOn = true;
    overTemperatureOn = true;
    lowFuelOn = true;
    itf_board_st = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
}

inline void itf_fast_metrics_TO_cb(TimerHandle_t xTimer)
{
    rpm = 0;
    speed_kph = 0;
    itf_board_st = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_FAULT_CHOICE;
}

inline void itf_odometer_TO_cb(TimerHandle_t xTimer)
{
    itf_board_st = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
}

inline void ext_oil_metrics_TO_cb(TimerHandle_t xTimer)
{
    // Do nothing
}

inline void ext_chargecooling_metrics_TO_cb(TimerHandle_t xTimer)
{
    // Do nothing
}

inline void itf_board_st_TO_cb(TimerHandle_t xTimer)
{
    itf_board_st = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_FAULT_CHOICE;
}

inline void itf_board_version_TO_cb(TimerHandle_t xTimer)
{
    itf_board_st = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_DEGRADED_CHOICE;
}

// Actual TWAI message handlers

// Actual TWAI message handlers

inline esp_err_t OTAHandler(const twai_frame_t *rxMsg)
{
    // Variables reserved for the UDS side of business
    static uint8_t ST_min = CONFIG_OTA_SEPARATION_TIME_MS;
    static uint8_t BSize = CONFIG_OTA_BLOCK_SIZE;
    static uint8_t blockCounter = 0x00;
    static uint8_t sequenceNumber = 0x01;
    static const esp_partition_t *update_partition = NULL;
    static bool FC_sent = false;
    static bool FF_received = false;
    static uint32_t receivedBytes;
    static bool transferComplete = false;
    static bool OTA_started = false;
    static uint32_t image_size;
    static esp_ota_handle_t ota_handle;
    uint8_t uds_resp_payload[8] = {0};
    esp_err_t ota_err;

    auto send_uds_response = [&](uint8_t d0, uint8_t d1, uint8_t d2 = 0xAA) -> esp_err_t {
        uds_resp_payload[0] = d0;
        uds_resp_payload[1] = d1;
        uds_resp_payload[2] = d2;
        for (size_t i = 3; i < 8; i++)
        {
            uds_resp_payload[i] = 0xAA;
        }
        if (twai_transmit_msg(UDS_RESP_FRAME_ID, uds_resp_payload, 8, false, 5) != ESP_OK)
        {
            ESP_LOGE(__func__, "Could not transmit UDS response message");
            return ESP_FAIL;
        }
        return ESP_OK;
    };

    // External reset of the OTA, such as on timeout
    if (rxMsg == NULL)
    {
        ESP_LOGW(__func__, "NULL message passed to OTA, resetting status");
        if (OTA_started)
        {
            esp_ota_abort(ota_handle);
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            blockCounter = 0x00;
            sequenceNumber = 0x01;
            ESP_LOGW(__func__, "OTA was in progress, aborted.");
        }
        send_uds_response(0x40, 0xFF);
        return ESP_ERR_TIMEOUT; // Error break
    }

    // Get target partition
    if (update_partition == NULL) // Only first time
        update_partition = esp_ota_get_next_update_partition(NULL);
    UDS_FRAME_OBJ
    // Attempt unpack, filter malformed frames
    if (UDS_FRAME_UNPACK)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        if (OTA_started)
        {
            esp_ota_abort(ota_handle);
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            blockCounter = 0x00;
            sequenceNumber = 0x01;
            ESP_LOGW(__func__, "OTA was in progress, aborted.");
        }
        send_uds_response(0x40, 0xFF);
        return ESP_ERR_INVALID_SIZE; // Error break
    }
    // Check if this is a correct first frame
    if ((rxMsg->buffer[0] & 0xF0) == 0x10)
    {
        if (OTA_started || FF_received) // Early exit error case
        {
            ESP_LOGE(__func__, "New FF received while OTA in progress, aborting.");
            esp_ota_abort(ota_handle);
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            send_uds_response(0x40, 0xFF);
            return ESP_ERR_NOT_FINISHED;
        }

        // TODO : check for escape sequence, shorter size contents
        image_size = swap_endian<uint32_t>(*(uint32_t *)(rxMsg->buffer + 2));
        receivedBytes = 0;
        sequenceNumber = 0x01;
        blockCounter = 0x00;
        transferComplete = false;
        ESP_LOGI(__func__, "Received FF, starting OTA for size : %lu bytes on partition %s", image_size, update_partition->label);
        ota_err = esp_ota_begin(update_partition, (size_t)image_size, &ota_handle);
        if (ota_err != ESP_OK) // Break if somehow the OTA doesn't start, print error code
        {
            ESP_LOGE(__func__, "Could not start OTA : %s", esp_err_to_name(ota_err));
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            blockCounter = 0x00;
            sequenceNumber = 0x01;
            send_uds_response(0x40, 0x04);
            return ota_err;
        }
        ota_err = esp_ota_write(ota_handle, (rxMsg->buffer + 6), 2);
        if (ota_err != ESP_OK) // Break if somehow the OTA doesn't write
        {
            ESP_LOGE(__func__, "Could not write OTA segment : %s", esp_err_to_name(ota_err));
            esp_ota_abort(ota_handle);
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            blockCounter = 0x00;
            sequenceNumber = 0x01;
            send_uds_response(0x40, 0x05);
            return ota_err;
        }
        receivedBytes = 2;
        FF_received = true;
        OTA_started = true;
        // Send the FC frame for continuation
        uds_resp_payload[0] = 0x30;
        uds_resp_payload[1] = BSize;
        uds_resp_payload[2] = ST_min;
        for (size_t i = 3; i < 8; i++)
        {
            uds_resp_payload[i] = 0xAA;
        }
        if (twai_transmit_msg(UDS_RESP_FRAME_ID, uds_resp_payload, 8, false, 5) != ESP_OK)
        {
            ESP_LOGE(__func__, "Could not transmit UDS response message");
            esp_ota_abort(ota_handle);
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            blockCounter = 0x00;
            sequenceNumber = 0x01;
            return ESP_ERR_NO_MEM;
        }
        else
        {
            ESP_LOGD(__func__, "Flow Control Frame sent.");
        }
        FC_sent = true;
        return ESP_OK; // Successful break
    }
    else if ((rxMsg->buffer[0] & 0xF0) == 0x20) // CF is received
    {
        if (!OTA_started || !FF_received || !FC_sent) // Weird break case
        {
            ESP_LOGE(__func__, "Unexpected CF received");
            esp_ota_abort(ota_handle);
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            blockCounter = 0x00;
            sequenceNumber = 0x01;
            send_uds_response(0x40, 0xFF);
            return ESP_ERR_NOT_ALLOWED;
        }

        // Check sequence number
        if ((rxMsg->buffer[0] & 0x0F) != sequenceNumber)
        {
            ESP_LOGE(__func__, "Bad sequence number : %u instead of %u", rxMsg->buffer[0] & 0x0F, sequenceNumber);
            esp_ota_abort(ota_handle);
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            blockCounter = 0x00;
            sequenceNumber = 0x01;
            send_uds_response(0x40, 0x03, sequenceNumber);
            return ESP_ERR_INVALID_ARG;
        }
        sequenceNumber = ((sequenceNumber + 1) & 0x0F) == 0x00 ? 0x01 : (sequenceNumber + 1) & 0x0F;
        blockCounter++;

        // Check if the whole message can be written to OTA or if something needs to be ignored
        if (image_size - receivedBytes >= 7)
        {
            ota_err = esp_ota_write(ota_handle, (rxMsg->buffer + 1), 7);
            if (ota_err != ESP_OK) // Break if somehow the OTA doesn't write
            {
                ESP_LOGE(__func__, "Could not write OTA segment : %s", esp_err_to_name(ota_err));
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                send_uds_response(0x40, 0x05);
                return ota_err;
            }
            receivedBytes += 7;

            odometer_km = image_size - receivedBytes;
            trip_km = 100.0 * (float)(receivedBytes) / (float)(image_size);
            fuelLevel_pc = trip_km;
            if (image_size == receivedBytes)
                transferComplete = true;
        }
        else // Only part of the buffer needs to be taken in
        {
            ota_err = esp_ota_write(ota_handle, (rxMsg->buffer + 1), image_size - receivedBytes);
            if (ota_err != ESP_OK) // Break if somehow the OTA doesn't write
            {
                ESP_LOGE(__func__, "Could not write OTA segment : %s", esp_err_to_name(ota_err));
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                send_uds_response(0x40, 0x05);
                return ota_err;
            }
            receivedBytes = image_size;
            transferComplete = true;
        }

        if (transferComplete) // Need to reset everything
        {
            ESP_LOGI(__func__, "Transfer complete");
            FC_sent = false;
            FF_received = false;
            OTA_started = false;
            receivedBytes = 0;
            transferComplete = false;
            image_size = 0;
            blockCounter = 0x00;
            sequenceNumber = 0x01;
            if (OTA_TO_hdl && xTimerIsTimerActive(OTA_TO_hdl))
                xTimerStop(OTA_TO_hdl, pdMS_TO_TICKS(1));
            ota_err = esp_ota_end(ota_handle);
            if (ota_err != ESP_OK)
            {
                ESP_LOGE(__func__, "OTA not successful : %s", esp_err_to_name(ota_err));
                send_uds_response(0x40, 0x02);
                return ota_err;
            }
            else
            {
                ESP_LOGI(__func__, "OTA image verification OK, setting boot to %s and restarting.", update_partition->label);
                esp_ota_set_boot_partition(update_partition);
                send_uds_response(0x40, 0x00);
                vTaskDelay(pdMS_TO_TICKS(1));
                esp_restart();
                return ESP_OK;
            }
        }

        // Send a FC frame if blockCounter == BSize
        if (BSize > 0 && blockCounter == BSize)
        {
            uds_resp_payload[0] = 0x30;
            uds_resp_payload[1] = BSize;
            uds_resp_payload[2] = ST_min;
            for (size_t i = 3; i < 8; i++)
            {
                uds_resp_payload[i] = 0xAA;
            }
            if (twai_transmit_msg(UDS_RESP_FRAME_ID, uds_resp_payload, 8, false, 5) != ESP_OK)
            {
                ESP_LOGE(__func__, "Could not transmit UDS response message");
                esp_ota_abort(ota_handle);
                FC_sent = false;
                FF_received = false;
                OTA_started = false;
                receivedBytes = 0;
                transferComplete = false;
                image_size = 0;
                blockCounter = 0x00;
                sequenceNumber = 0x01;
                return ESP_ERR_NO_MEM;
            }
            else
            {
                ESP_LOGD(__func__, "Flow Control Frame sent.");
            }
            FC_sent = true;
            sequenceNumber = 0x01;
            blockCounter = 0x00;
        }
        return ESP_OK; // Successful break from top level switch
    }
    else
    {
        ESP_LOGD(__func__, "Unknown frame type received, ignoring.");
        return ESP_ERR_INVALID_RESPONSE;
    }
}

inline esp_err_t itf_active_hi_lo_Handler(const twai_frame_t *rxMsg)
{
    static binocan_itf_active_hi_lo_t binocan_itf_active_hi_lo_msg;
    if (binocan_itf_active_hi_lo_unpack(&binocan_itf_active_hi_lo_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }

    if (binocan_itf_active_hi_lo_itf_abs_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_abs_al_tt))
    {
        absOn = !(binocan_itf_active_hi_lo_itf_abs_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_abs_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_ABS_AL_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "ABS telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_abs_al_tt);
        absOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_airbag_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_airbag_al_tt))
    {
        airbagOn = !(binocan_itf_active_hi_lo_itf_airbag_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_airbag_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_AIRBAG_AL_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Airbag telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_airbag_al_tt);
        airbagOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_cel_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_cel_al_tt))
    {
        milOn = !(binocan_itf_active_hi_lo_itf_cel_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_cel_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_CEL_AL_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Check Engine Light telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_cel_al_tt);
        milOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_hi_beams_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt))
    {
        highBeamOn = (binocan_itf_active_hi_lo_itf_hi_beams_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_HI_BEAMS_AH_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "High beams telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_hi_beams_ah_tt);
        highBeamOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_brake_low_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt))
    {
        brakesOn = !(binocan_itf_active_hi_lo_itf_brake_low_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_BRAKE_LOW_AL_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Low brake fluid telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_brake_low_al_tt);
        brakesOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_coolant_low_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_coolant_low_ah_tt))
    {
        lowCoolantOn = (binocan_itf_active_hi_lo_itf_coolant_low_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_coolant_low_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_COOLANT_LOW_AH_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Low coolant level telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_coolant_low_ah_tt);
        lowCoolantOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_fuel_low_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_fuel_low_tt))
    {
        lowFuelOn = (binocan_itf_active_hi_lo_itf_fuel_low_tt_decode(binocan_itf_active_hi_lo_msg.itf_fuel_low_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_FUEL_LOW_TT_ON_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Low fuel level telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_fuel_low_tt);
        lowFuelOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_oil_pressure_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt))
    {
        lowOilOn = !(binocan_itf_active_hi_lo_itf_oil_pressure_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_OIL_PRESSURE_AL_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Low oil pressure telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_oil_pressure_al_tt);
        lowOilOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_alternator_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_alternator_al_tt))
    {
        batteryOn = !(binocan_itf_active_hi_lo_itf_alternator_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_alternator_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_ALTERNATOR_AL_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Battery or alternator telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_alternator_al_tt);
        batteryOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_over_temperature_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_over_temperature_tt))
    {
        overTemperatureOn = (binocan_itf_active_hi_lo_itf_over_temperature_tt_decode(binocan_itf_active_hi_lo_msg.itf_over_temperature_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_OVER_TEMPERATURE_TT_ON_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Coolant overtemperature telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_over_temperature_tt);
        overTemperatureOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_parking_brake_al_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt))
    {
        parkingBrakeOn = !(binocan_itf_active_hi_lo_itf_parking_brake_al_tt_decode(binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_PARKING_BRAKE_AL_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Parking brake telltale signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_parking_brake_al_tt);
        parkingBrakeOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_left_turn_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt) && binocan_itf_active_hi_lo_itf_right_turn_ah_tt_is_in_range(binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt))
    {
        indicatorsOn = (binocan_itf_active_hi_lo_itf_left_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_LEFT_TURN_AH_TT_HI_CHOICE) ||
                       (binocan_itf_active_hi_lo_itf_right_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_RIGHT_TURN_AH_TT_HI_CHOICE);
        leftTurnOn = (binocan_itf_active_hi_lo_itf_left_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_LEFT_TURN_AH_TT_HI_CHOICE);
        rightTurnOn = (binocan_itf_active_hi_lo_itf_right_turn_ah_tt_decode(binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_RIGHT_TURN_AH_TT_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Turn indicators telltale signal out of range: L %d - R %d", binocan_itf_active_hi_lo_msg.itf_left_turn_ah_tt, binocan_itf_active_hi_lo_msg.itf_right_turn_ah_tt);
        indicatorsOn = true;
        rightTurnOn = true;
        leftTurnOn = true;
    }

    if (binocan_itf_active_hi_lo_itf_ignition_ah_st_is_in_range(binocan_itf_active_hi_lo_msg.itf_ignition_ah_st))
    {
        ignitionST = (binocan_itf_active_hi_lo_itf_ignition_ah_st_decode(binocan_itf_active_hi_lo_msg.itf_ignition_ah_st) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_IGNITION_AH_ST_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Ignition Status signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_ignition_ah_st);
        ignitionST = false; // Default value
    }
    if (binocan_itf_active_hi_lo_itf_alarm_ah_is_in_range(binocan_itf_active_hi_lo_msg.itf_alarm_ah))
    {
        alarmOn = (binocan_itf_active_hi_lo_itf_alarm_ah_decode(binocan_itf_active_hi_lo_msg.itf_alarm_ah) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_ALARM_AH_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Alarm signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_alarm_ah);
        alarmOn = true; // Default value
    }
    if (binocan_itf_active_hi_lo_itf_backlight_ah_is_in_range(binocan_itf_active_hi_lo_msg.itf_backlight_ah))
    {
        headlightsOn = (binocan_itf_active_hi_lo_itf_backlight_ah_decode(binocan_itf_active_hi_lo_msg.itf_backlight_ah) == BINOCAN_ITF_ACTIVE_HI_LO_ITF_BACKLIGHT_AH_HI_CHOICE);
    }
    else
    {
        ESP_LOGW(__func__, "Backlight signal out of range: %d", binocan_itf_active_hi_lo_msg.itf_backlight_ah);
        headlightsOn = true; // Default value
    }

    ESP_LOGD(__func__, "Telltales: ABS %d, Airbag %d, CEL %d, High Beams %d, Low Brake Fluid %d, Low Coolant %d, Low Fuel %d, Low Oil Pressure %d, Battery/Alternator %d, Over Temperature %d, Parking Brake %d, Indicators %d, Ignition %d, Alarm %d, Headlights %d",
             absOn, airbagOn, milOn, highBeamOn, brakesOn, lowCoolantOn, lowFuelOn, lowOilOn, batteryOn,
             overTemperatureOn, parkingBrakeOn, indicatorsOn, ignitionST, alarmOn, headlightsOn);
    return ESP_OK;
}

inline esp_err_t itf_slow_metrics_Handler(const twai_frame_t *rxMsg)
{
    static binocan_itf_slow_metrics_t binocan_itf_slow_metrics_msg;
    if (binocan_itf_slow_metrics_unpack(&binocan_itf_slow_metrics_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }
    if (binocan_itf_slow_metrics_itf_coolant_temp_is_in_range(binocan_itf_slow_metrics_msg.itf_coolant_temp))
    {
        coolant_degC = (uint8_t)round(binocan_itf_slow_metrics_itf_coolant_temp_decode(binocan_itf_slow_metrics_msg.itf_coolant_temp));
    }
    else
    {
        ESP_LOGW(__func__, "Coolant temperature signal out of range: %d", binocan_itf_slow_metrics_msg.itf_coolant_temp);
        coolant_degC = 255;       // Default value
        overTemperatureOn = true; // Set over temperature on if coolant is out of range
    }

    if (binocan_itf_slow_metrics_itf_fuel_level_pc_is_in_range(binocan_itf_slow_metrics_msg.itf_fuel_level_pc))
    {
        fuelLevel_pc = (uint8_t)round(binocan_itf_slow_metrics_itf_fuel_level_pc_decode(binocan_itf_slow_metrics_msg.itf_fuel_level_pc));
    }
    else
    {
        ESP_LOGW(__func__, "Fuel level signal out of range: %d", binocan_itf_slow_metrics_msg.itf_fuel_level_pc);
        fuelLevel_pc = 0; // Default value
        lowFuelOn = true; // Set low fuel on if fuel level is out of range
    }

    if (binocan_itf_slow_metrics_itf_lv_voltage_v_is_in_range(binocan_itf_slow_metrics_msg.itf_lv_voltage_v))
    {
        lvVoltage_v = (float)binocan_itf_slow_metrics_itf_lv_voltage_v_decode(binocan_itf_slow_metrics_msg.itf_lv_voltage_v);
    }
    else
    {
        ESP_LOGW(__func__, "LV Voltage signal out of range: %d", binocan_itf_slow_metrics_msg.itf_lv_voltage_v);
        batteryOn = true; // Set battery on if LV voltage is out of range
    }

    ESP_LOGD(__func__, "Slow Metrics: Coolant %d, Fuel Level %d, LV Voltage %.2f",
             coolant_degC, fuelLevel_pc, lvVoltage_v);
    return ESP_OK;
}

inline esp_err_t itf_fast_metrics_Handler(const twai_frame_t *rxMsg)
{
    static binocan_itf_fast_metrics_t binocan_itf_fast_metrics_msg;

    if (binocan_itf_fast_metrics_unpack(&binocan_itf_fast_metrics_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }
    if (binocan_itf_fast_metrics_itf_rpm_is_in_range(binocan_itf_fast_metrics_msg.itf_rpm))
    {
        rpm = (uint32_t)lround(binocan_itf_fast_metrics_itf_rpm_decode(binocan_itf_fast_metrics_msg.itf_rpm));
    }
    else
    {
        ESP_LOGW(__func__, "RPM signal out of range: %d", binocan_itf_fast_metrics_msg.itf_rpm);
        rpm = 0; // Default value
    }

    if (binocan_itf_fast_metrics_itf_speed_kph_is_in_range(binocan_itf_fast_metrics_msg.itf_speed_kph))
    {
        speed_kph = (float)binocan_itf_fast_metrics_itf_speed_kph_decode(binocan_itf_fast_metrics_msg.itf_speed_kph);
    }
    else
    {
        ESP_LOGW(__func__, "Speed signal out of range: %d", binocan_itf_fast_metrics_msg.itf_speed_kph);
        speed_kph = 0; // Default value
    }

    ESP_LOGD(__func__, "Vehicle Metrics: RPM %lu, Speed %.2f", rpm, speed_kph);
    return ESP_OK;
}

inline esp_err_t itf_odometer_Handler(const twai_frame_t *rxMsg)
{
    static binocan_itf_odometer_t binocan_itf_odometer_msg;

    if (binocan_itf_odometer_unpack(&binocan_itf_odometer_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }

    if (binocan_itf_odometer_itf_odometer_km_is_in_range(binocan_itf_odometer_msg.itf_odometer_km) && binocan_itf_odometer_itf_odo_rem_m_is_in_range(binocan_itf_odometer_msg.itf_odo_rem_m))
    {
        odometer_km = binocan_itf_odometer_itf_odometer_km_decode(binocan_itf_odometer_msg.itf_odometer_km) + binocan_itf_odometer_itf_odo_rem_m_decode(binocan_itf_odometer_msg.itf_odo_rem_m) / 1000.0;
    }
    else
    {
        ESP_LOGW(__func__, "Odometer could not be decoded : %d + %d /1000", binocan_itf_odometer_itf_odometer_km_decode(binocan_itf_odometer_msg.itf_odometer_km), binocan_itf_odometer_itf_odo_rem_m_decode(binocan_itf_odometer_msg.itf_odo_rem_m));
        odometer_km = 0;
    }

    if (binocan_itf_odometer_itf_trip_km_is_in_range(binocan_itf_odometer_msg.itf_trip_km) && binocan_itf_odometer_itf_trip_rem_m_is_in_range(binocan_itf_odometer_msg.itf_trip_rem_m))
    {
        trip_km = binocan_itf_odometer_itf_trip_km_decode(binocan_itf_odometer_msg.itf_trip_km) + binocan_itf_odometer_itf_trip_rem_m_decode(binocan_itf_odometer_msg.itf_trip_rem_m) / 1000.0;
    }
    else
    {
        ESP_LOGW(__func__, "Trip could not be decoded : %d + %d /1000", binocan_itf_odometer_itf_trip_km_decode(binocan_itf_odometer_msg.itf_trip_km), binocan_itf_odometer_itf_trip_rem_m_decode(binocan_itf_odometer_msg.itf_trip_rem_m));
        trip_km = 0;
    }

    ESP_LOGD(__func__, "Odometer : %0.1f km Trip: %0.1f", odometer_km, trip_km);
    return ESP_OK;
}

inline esp_err_t ext_oil_metrics_Handler(const twai_frame_t *rxMsg)
{
    static binocan_ext_oil_metrics_t binocan_ext_oil_metrics_msg;
    if (binocan_ext_oil_metrics_unpack(&binocan_ext_oil_metrics_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

inline esp_err_t ext_chargecooling_metrics_Handler(const twai_frame_t *rxMsg)
{
    static binocan_ext_chargecooling_metrics_t binocan_ext_chargecooling_metrics_msg;
    if (binocan_ext_chargecooling_metrics_unpack(&binocan_ext_chargecooling_metrics_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

inline esp_err_t itf_board_st_Handler(const twai_frame_t *rxMsg)
{
    static binocan_itf_board_st_t binocan_itf_board_st_msg;
    if (binocan_itf_board_st_unpack(&binocan_itf_board_st_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }

    if (binocan_itf_board_st_itf_sm_st_is_in_range(binocan_itf_board_st_msg.itf_sm_st))
    {
        itf_board_st = (uint8_t)binocan_itf_board_st_itf_sm_st_decode(binocan_itf_board_st_msg.itf_sm_st);
    }
    else
    {
        ESP_LOGW(__func__, "ITF SM Status signal out of range: %d", binocan_itf_board_st_msg.itf_sm_st);
        itf_board_st = BINOCAN_ITF_BOARD_ST_ITF_SM_ST_FAULT_CHOICE; // Default value
    }
    return ESP_OK;
}

inline esp_err_t itf_board_version_Handler(const twai_frame_t *rxMsg)
{
    static binocan_itf_board_version_t binocan_itf_board_version_msg;
    if (binocan_itf_board_version_unpack(&binocan_itf_board_version_msg, rxMsg->buffer, rxMsg->header.dlc) == EINVAL)
    {
        ESP_LOGE(__func__, "Malformed frame 0x%03lX, invalid DLC", rxMsg->header.id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// Declarative Routing Table & Dispatch
// -----------------------------------------------------------------------------

typedef esp_err_t (*twai_frame_handler_t)(const twai_frame_t *rxMsg);

struct twai_route_entry_t {
    uint32_t frame_id;
    twai_frame_handler_t handler;
    uint32_t timeout_ms;
    TimerCallbackFunction_t timeout_cb;
    TimerHandle_t timer_hdl;
    const char *name;
};

static twai_route_entry_t rx_routes[] = {
    { BINOCAN_ITF_ACTIVE_HI_LO_FRAME_ID,          itf_active_hi_lo_Handler,          5 * BINOCAN_ITF_ACTIVE_HI_LO_CYCLE_TIME_MS,          itf_active_hi_lo_TO_cb,          NULL, "act_hi_lo" },
    { BINOCAN_ITF_SLOW_METRICS_FRAME_ID,          itf_slow_metrics_Handler,          5 * BINOCAN_ITF_SLOW_METRICS_CYCLE_TIME_MS,          itf_slow_metrics_TO_cb,          NULL, "slow_m" },
    { BINOCAN_ITF_FAST_METRICS_FRAME_ID,          itf_fast_metrics_Handler,          5 * BINOCAN_ITF_FAST_METRICS_CYCLE_TIME_MS,          itf_fast_metrics_TO_cb,          NULL, "fast_m" },
    { BINOCAN_ITF_ODOMETER_FRAME_ID,              itf_odometer_Handler,              5 * BINOCAN_ITF_ODOMETER_CYCLE_TIME_MS,              itf_odometer_TO_cb,              NULL, "odom" },
    { BINOCAN_EXT_OIL_METRICS_FRAME_ID,          ext_oil_metrics_Handler,          5 * BINOCAN_EXT_OIL_METRICS_CYCLE_TIME_MS,          ext_oil_metrics_TO_cb,          NULL, "oil_m" },
    { BINOCAN_EXT_CHARGECOOLING_METRICS_FRAME_ID, ext_chargecooling_metrics_Handler, 5 * BINOCAN_EXT_CHARGECOOLING_METRICS_CYCLE_TIME_MS, ext_chargecooling_metrics_TO_cb, NULL, "chg_m" },
    { BINOCAN_ITF_BOARD_ST_FRAME_ID,              itf_board_st_Handler,              5 * BINOCAN_ITF_BOARD_ST_CYCLE_TIME_MS,              itf_board_st_TO_cb,              NULL, "ibs" },
    { BINOCAN_ITF_BOARD_VERSION_FRAME_ID,         itf_board_version_Handler,         5 * BINOCAN_ITF_BOARD_VERSION_CYCLE_TIME_MS,         itf_board_version_TO_cb,         NULL, "ibv" },
    { ALT_DISPLAY_ST_FRAME_ID,                    alt_display_st_Handler,            5000,                                                alt_display_st_TO_cb,            NULL, "alt_st" },
    { UDS_REQ_FRAME_ID,                           OTAHandler,                        5000,                                                OTA_TO_cb,                        NULL, "OTA" },
};

static const size_t NUM_RX_ROUTES = sizeof(rx_routes) / sizeof(rx_routes[0]);

/// @brief Initializer for all the timeout timers
/// @return ESP_OK if all timers created successfully, ESP_FAIL otherwise
inline esp_err_t TO_timers_init()
{
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < NUM_RX_ROUTES; ++i)
    {
        if (rx_routes[i].timeout_ms > 0 && rx_routes[i].timeout_cb != nullptr)
        {
            rx_routes[i].timer_hdl = xTimerCreate(rx_routes[i].name,
                                                  pdMS_TO_TICKS(rx_routes[i].timeout_ms),
                                                  pdFALSE,
                                                  NULL,
                                                  rx_routes[i].timeout_cb);
            if (rx_routes[i].timer_hdl == NULL)
            {
                ESP_LOGE(__func__, "Failed to create timer for route: %s", rx_routes[i].name);
                ret = ESP_FAIL;
            }
        }
    }
    for (size_t i = 0; i < NUM_RX_ROUTES; ++i)
    {
        if (rx_routes[i].frame_id == UDS_REQ_FRAME_ID)
        {
            OTA_TO_hdl = rx_routes[i].timer_hdl;
        }
        else if (rx_routes[i].frame_id == ALT_DISPLAY_ST_FRAME_ID)
        {
            alt_display_st_TO_hdl = rx_routes[i].timer_hdl;
        }
    }
    return ret;
}

/// @brief Starts the timeout timers
/// @return ESP_OK if all timers started successfully, ESP_FAIL otherwise
inline esp_err_t TO_timers_start()
{
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < NUM_RX_ROUTES; ++i)
    {
        if (rx_routes[i].frame_id != UDS_REQ_FRAME_ID && rx_routes[i].timer_hdl != NULL)
        {
            if (xTimerReset(rx_routes[i].timer_hdl, pdMS_TO_TICKS(1)) != pdPASS)
            {
                ret = ESP_FAIL;
            }
        }
    }
    return ret;
}

/// @brief Stops the timeout timers
/// @return ESP_OK if all timers stopped successfully, ESP_FAIL otherwise
inline esp_err_t TO_timers_stop()
{
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < NUM_RX_ROUTES; ++i)
    {
        if (rx_routes[i].timer_hdl != NULL)
        {
            if (xTimerStop(rx_routes[i].timer_hdl, pdMS_TO_TICKS(1)) != pdPASS)
            {
                ret = ESP_FAIL;
            }
        }
    }
    return ret;
}

/// @brief Dispatcher linked to the TWAI daemon. Parses received CAN frames
/// @param rxMsg Received TWAI frame
/// @return Error code, if relevant
inline esp_err_t dispatchFrame(const twai_frame_t *rxMsg)
{
    if (rxMsg == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < NUM_RX_ROUTES; ++i)
    {
        if (rx_routes[i].frame_id == rxMsg->header.id)
        {
            if (rx_routes[i].timer_hdl != NULL)
            {
                xTimerReset(rx_routes[i].timer_hdl, pdMS_TO_TICKS(1));
            }
            return rx_routes[i].handler(rxMsg);
        }
    }

    ESP_LOGD(__func__, "Unknown CAN frame received: ID = 0x%03lX", rxMsg->header.id);
    return ESP_ERR_INVALID_ARG;
}

#pragma endregion