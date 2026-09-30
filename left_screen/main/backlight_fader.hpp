#pragma once
#include <stdint.h>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "global_vars.hpp"

#ifdef TAG
#undef TAG
#endif
#define TAG "BacklightFader"

static TimerHandle_t backlight_fader_timer = NULL;
static int current_fader_brightness = 0;
static int target_fader_brightness = 0;
static int fader_step_delta = 0;
static int fader_steps_remaining = 0;

/// @brief Timer callback running in prvTimerTask (prio 1) to incrementally step backlight duty
static void backlight_fader_timer_cb(TimerHandle_t xTimer)
{
    if (fader_steps_remaining <= 0 || current_fader_brightness == target_fader_brightness)
    {
        current_fader_brightness = target_fader_brightness;
        if (display_board_st.backLight != nullptr)
        {
            display_board_st.backLight->setBrightness(current_fader_brightness);
        }
        xTimerStop(xTimer, 0);
        return;
    }

    fader_steps_remaining--;
    if (fader_steps_remaining == 0)
    {
        current_fader_brightness = target_fader_brightness;
    }
    else
    {
        current_fader_brightness += fader_step_delta;
    }

    current_fader_brightness = std::clamp(current_fader_brightness, 0, 100);

    if (display_board_st.backLight != nullptr)
    {
        display_board_st.backLight->setBrightness(current_fader_brightness);
    }

    if (current_fader_brightness == target_fader_brightness)
    {
        xTimerStop(xTimer, 0);
    }
}

/// @brief Initialize the FreeRTOS software timer for backlight fading
inline void backlight_fader_init()
{
    if (backlight_fader_timer == NULL)
    {
        backlight_fader_timer = xTimerCreate(
            "BL_FADE",
            pdMS_TO_TICKS(15),
            pdTRUE,
            NULL,
            backlight_fader_timer_cb
        );
        if (backlight_fader_timer == NULL)
        {
            ESP_LOGE(TAG, "Failed to create backlight fader FreeRTOS timer");
        }
    }
}

/// @brief Get the current in-flight or stable backlight brightness
inline uint8_t get_current_backlight_brightness()
{
    return (uint8_t)std::clamp(current_fader_brightness, 0, 100);
}

/// @brief Set the initial tracking brightness without fading (e.g. at boot 0)
inline void set_backlight_brightness_instant(uint8_t brightness)
{
    brightness = (uint8_t)std::clamp((int)brightness, 0, 100);
    if (backlight_fader_timer != NULL)
    {
        xTimerStop(backlight_fader_timer, 0);
    }
    current_fader_brightness = brightness;
    target_fader_brightness = brightness;
    fader_steps_remaining = 0;
    fader_step_delta = 0;
    if (display_board_st.backLight != nullptr)
    {
        display_board_st.backLight->setBrightness(brightness);
    }
}

/// @brief Smoothly fade display backlight to target brightness using FreeRTOS software timer
/// @param target_brightness Target brightness percentage (0 - 100)
/// @param duration_ms Total fade transition duration in milliseconds (default: 250)
/// @param step_interval_ms Time between interpolation steps in milliseconds (default: 15)
inline void set_backlight_brightness_smooth(uint8_t target_brightness, uint16_t duration_ms = 250, uint16_t step_interval_ms = 15)
{
    target_brightness = (uint8_t)std::clamp((int)target_brightness, 0, 100);
    target_fader_brightness = target_brightness;

    if (display_board_st.backLight == nullptr)
    {
        current_fader_brightness = target_brightness;
        return;
    }

    if (backlight_fader_timer == NULL)
    {
        backlight_fader_init();
    }

    if (duration_ms == 0 || step_interval_ms == 0 || current_fader_brightness == target_fader_brightness)
    {
        set_backlight_brightness_instant(target_brightness);
        return;
    }

    uint16_t total_steps = duration_ms / step_interval_ms;
    if (total_steps == 0)
    {
        total_steps = 1;
    }

    fader_steps_remaining = total_steps;
    int diff = target_fader_brightness - current_fader_brightness;
    fader_step_delta = diff / (int)total_steps;
    if (fader_step_delta == 0)
    {
        fader_step_delta = (diff > 0) ? 1 : -1;
    }

    // Adjust timer period if requested interval changed
    TickType_t period_ticks = pdMS_TO_TICKS(step_interval_ms);
    if (period_ticks == 0)
        period_ticks = 1;

    xTimerChangePeriod(backlight_fader_timer, period_ticks, 0);
    xTimerStart(backlight_fader_timer, 0);
}
