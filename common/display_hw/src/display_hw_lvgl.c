/**
 * @file display_hw_lvgl.c
 * @brief esp_lvgl_adapter glue for display_hw.
 */
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "display_hw_lvgl.h"

static const char *TAG = "display_hw_lvgl";

#define DISPLAY_HW_TEAR_MODE  ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT
#define DISPLAY_HW_ROTATION   ESP_LV_ADAPTER_ROTATE_0

/* Frame wait state. Written by display_hw_lvgl_wait_frame() and by the display event callback,
 * both under the LVGL lock (the callback runs inside lv_timer_handler()). */
static lv_display_t *s_disp = NULL;
static SemaphoreHandle_t s_frame_sem = NULL;
static bool s_frame_wait_armed = false;
static bool s_frame_flushed = false;

/* LV_EVENT_FLUSH_FINISH: a flush of the armed redraw has returned. In DOUBLE_DIRECT the adapter's
 * flush of the last area only returns once the panel has switched to the new framebuffer.
 * LV_EVENT_REFR_READY: the refresh cycle is over, so the whole frame is on the panel. */
static void frame_event_cb(lv_event_t *e)
{
    if (!s_frame_wait_armed) {
        return;
    }
    if (lv_event_get_code(e) == LV_EVENT_FLUSH_FINISH) {
        s_frame_flushed = true;
    } else if (s_frame_flushed) {
        s_frame_wait_armed = false;
        xSemaphoreGive(s_frame_sem);
    }
}

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

    /* The LVGL task is not running yet, so the display can be touched without the lock. */
    s_frame_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_frame_sem, ESP_ERR_NO_MEM, TAG, "frame semaphore");
    lv_display_add_event_cb(disp, frame_event_cb, LV_EVENT_FLUSH_FINISH, NULL);
    lv_display_add_event_cb(disp, frame_event_cb, LV_EVENT_REFR_READY, NULL);
    s_disp = disp;

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

bool display_hw_lvgl_wait_frame(int timeout_ms)
{
    if (s_disp == NULL || s_frame_sem == NULL || !display_hw_lvgl_lock(-1)) {
        return false;
    }
    xSemaphoreTake(s_frame_sem, 0);
    s_frame_flushed = false;
    s_frame_wait_armed = true;
    lv_obj_invalidate(lv_display_get_screen_active(s_disp));
    display_hw_lvgl_unlock();

    const TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(s_frame_sem, ticks) == pdTRUE) {
        return true;
    }

    /* Timed out: disarm, and drop a give that may have raced with the timeout. */
    display_hw_lvgl_lock(-1);
    s_frame_wait_armed = false;
    display_hw_lvgl_unlock();
    return xSemaphoreTake(s_frame_sem, 0) == pdTRUE;
}
