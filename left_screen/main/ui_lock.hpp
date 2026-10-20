#pragma once
#include "display_hw_lvgl.h"

/// @brief Take the LVGL lock. Keeps the name used by the previous lvgl_v9_port so call sites stay unchanged.
/// @param timeout_ms Timeout in milliseconds, negative waits forever
/// @return true if the lock was taken
/// @note Thread-safe, recursive. Wraps esp_lv_adapter_lock().
inline bool lvgl_port_lock(int timeout_ms)
{
    return display_hw_lvgl_lock(timeout_ms);
}

/// @brief Release the LVGL lock taken with lvgl_port_lock()
/// @return Always true, kept for signature compatibility with the previous port
/// @note Must be called from the task holding the lock.
inline bool lvgl_port_unlock(void)
{
    display_hw_lvgl_unlock();
    return true;
}
