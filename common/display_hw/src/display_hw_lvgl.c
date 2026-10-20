/**
 * @file display_hw_lvgl.c
 * @brief esp_lvgl_adapter glue for display_hw.
 */
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"

#include "display_hw_lvgl.h"

static const char *TAG = "display_hw_lvgl";

#define DISPLAY_HW_TEAR_MODE  ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT
#define DISPLAY_HW_ROTATION   ESP_LV_ADAPTER_ROTATE_0

uint8_t display_hw_lvgl_required_fbs(void)
{
    return esp_lv_adapter_get_required_frame_buffer_count(DISPLAY_HW_TEAR_MODE, DISPLAY_HW_ROTATION);
}

esp_err_t display_hw_lvgl_start(const display_hw_lvgl_config_t *cfg, const display_hw_handles_t *hw,
                                lv_display_t **ret_disp)
{
    ESP_RETURN_ON_FALSE(cfg && hw && hw->panel, ESP_ERR_INVALID_ARG, TAG, "invalid args");

    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_cfg.task_stack_size = cfg->task_stack_size;
    adapter_cfg.task_priority = cfg->task_priority;
    adapter_cfg.task_core_id = cfg->task_core_id;
    adapter_cfg.tick_period_ms = cfg->tick_period_ms;
    adapter_cfg.task_max_delay_ms = cfg->task_max_delay_ms;
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_cfg), TAG, "adapter init");

    /* The RGB path only uses the panel handle; the 3-wire control IO has no transfer callbacks. */
    esp_lv_adapter_display_config_t disp_cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                hw->panel, NULL, DISPLAY_HW_H_RES, DISPLAY_HW_V_RES, DISPLAY_HW_ROTATION);
    disp_cfg.tear_avoid_mode = DISPLAY_HW_TEAR_MODE;
    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "display registration failed");

    if (hw->touch) {
        esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, hw->touch);
        ESP_RETURN_ON_FALSE(esp_lv_adapter_register_touch(&touch_cfg), ESP_FAIL, TAG, "touch registration failed");
    } else {
        ESP_LOGW(TAG, "No touch handle, starting without input device");
    }

    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "adapter start");
    if (ret_disp) {
        *ret_disp = disp;
    }
    return ESP_OK;
}

bool display_hw_lvgl_lock(int timeout_ms)
{
    return esp_lv_adapter_lock(timeout_ms < 0 ? -1 : timeout_ms) == ESP_OK;
}

void display_hw_lvgl_unlock(void)
{
    esp_lv_adapter_unlock();
}
