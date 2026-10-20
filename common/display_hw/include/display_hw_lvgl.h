/**
 * @file display_hw_lvgl.h
 * @brief LVGL bring-up for display_hw through espressif/esp_lvgl_adapter.
 *
 * Kept in C because the adapter's configuration macros use C compound literals and designated
 * initialisers that are not valid C++.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"
#include "display_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LVGL task settings passed to the adapter.
 */
typedef struct {
    uint32_t task_stack_size;   /*!< LVGL task stack in bytes */
    uint32_t task_priority;     /*!< LVGL task FreeRTOS priority */
    int task_core_id;           /*!< Core the LVGL task is pinned to, -1 for no affinity */
    uint32_t tick_period_ms;    /*!< LVGL tick period in ms */
    uint32_t task_max_delay_ms; /*!< Upper bound between two lv_timer_handler() calls, in ms */
} display_hw_lvgl_config_t;

/**
 * @brief Framebuffer count the RGB panel must allocate for the tear-avoid mode used here.
 *
 * Pass the result to display_hw_init() before calling display_hw_lvgl_start().
 *
 * @return Number of framebuffers (2 for DOUBLE_DIRECT without rotation).
 */
uint8_t display_hw_lvgl_required_fbs(void);

/**
 * @brief Initialise the adapter, register the display and touch, and start the LVGL task.
 *
 * Uses tear-avoid mode DOUBLE_DIRECT (two PSRAM framebuffers, LVGL DIRECT render mode),
 * rotation 0.
 *
 * @param[in]  cfg      LVGL task settings.
 * @param[in]  hw       Handles from display_hw_init(); @c hw->touch may be NULL (no input device).
 * @param[out] ret_disp Registered LVGL display, may be NULL if not needed.
 *
 * @return
 *      - ESP_OK              LVGL task running
 *      - ESP_ERR_INVALID_ARG cfg or hw is NULL, or hw->panel is NULL
 *      - ESP_FAIL            display or touch registration failed
 *      - other               adapter init/start error
 *
 * @note Call once, from app_main, after display_hw_init().
 */
esp_err_t display_hw_lvgl_start(const display_hw_lvgl_config_t *cfg, const display_hw_handles_t *hw,
                                lv_display_t **ret_disp);

/**
 * @brief Take the LVGL lock (recursive).
 *
 * @param[in] timeout_ms Timeout in ms, negative waits forever.
 *
 * @return true if the lock was taken.
 *
 * @note Thread-safe. The mutex is recursive, so nested lock/unlock pairs in the same task are
 *       allowed. LVGL event callbacks already run under the lock.
 */
bool display_hw_lvgl_lock(int timeout_ms);

/**
 * @brief Release the LVGL lock taken with display_hw_lvgl_lock().
 *
 * @note Thread-safe; must be called from the task that holds the lock.
 */
void display_hw_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif
