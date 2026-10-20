# display_hw: how to reuse it

## Capabilities & scope

- Brings up the Waveshare ESP32-S3-Touch-LCD-2.8C: ST7701 480 x 480 RGB panel (3-wire SPI init, CS on the TCA9554 expander), GT911 touch (reset through the expander), TCA9554 expander and LEDC backlight.
- Starts LVGL through `espressif/esp_lvgl_adapter` in `DOUBLE_DIRECT` tear-avoid mode with two PSRAM framebuffers, and registers touch as an LVGL pointer.
- Gives a recursive LVGL lock for tasks other than the LVGL task.
- Does not handle rotation, a UI, or other boards. Pins and bus speeds are fixed macros in [`include/display_hw.h`](include/display_hw.h).

## Dependencies & CMake `REQUIRES`

ESP-IDF >= 5.5.5 with PSRAM enabled (octal, 80 MHz on this board).

Registry components, declared in this component's [`idf_component.yml`](idf_component.yml) and fetched automatically:

| Component | Version |
| :--- | :--- |
| `lvgl/lvgl` | `~9.5.0` (public) |
| `espressif/esp_lvgl_adapter` | `~0.7.3` |
| `espressif/esp_lcd_st7701` | `~2.0.3` |
| `espressif/esp_lcd_panel_io_additions` | `~1.0.1` |
| `espressif/esp_io_expander_tca9554` | `~2.0.3` (public) |
| `espressif/esp_lcd_touch_gt911` | `~1.2.1` (public) |

ESP-IDF components: `esp_lcd`, `esp_driver_gpio` (public), `esp_driver_i2c`, `esp_driver_ledc` (private).

### Adding it to a project

Load it as a plain component through `EXTRA_COMPONENT_DIRS` in the project `CMakeLists.txt`, before `project.cmake`:

```cmake
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../common/display_hw)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
```

Do not add it as a `path:` dependency in `idf_component.yml`: the component manager writes local path dependencies to `dependencies.lock` as absolute paths. Loaded through `EXTRA_COMPONENT_DIRS`, its registry dependencies are still resolved and locked.

Then require it from `main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES display_hw)
```

## Copy-paste integration

```c
#include "display_hw.h"
#include "display_hw_lvgl.h"

void app_main(void)
{
    display_hw_handles_t hw = {0};
    esp_err_t err = display_hw_init(display_hw_lvgl_required_fbs(), &hw);
    if (err == ESP_ERR_NOT_FOUND) {
        // Panel is up, touch did not answer: hw.touch is NULL, keep going without input
    } else if (err != ESP_OK) {
        return;
    }

    const display_hw_lvgl_config_t lvgl_cfg = {
        .task_stack_size = 12288,
        .task_priority = 2,
        .task_core_id = 1,
        .tick_period_ms = 2,
        .task_max_delay_ms = 500,
    };
    ESP_ERROR_CHECK(display_hw_lvgl_start(&lvgl_cfg, &hw, NULL));

    // Build the UI under the LVGL lock
    if (display_hw_lvgl_lock(-1)) {
        lv_obj_t *label = lv_label_create(lv_screen_active());
        lv_label_set_text(label, "Hello");
        display_hw_lvgl_unlock();
    }

    // Backlight starts off; turn it on once the first frame is drawn
    display_hw_backlight_set(80);
    display_hw_buzzer_set(false);
}
```

Rules:

- Call `display_hw_init()` then `display_hw_lvgl_start()` once, from `app_main`.
- Wrap every LVGL call made outside the LVGL task in `display_hw_lvgl_lock()` / `display_hw_lvgl_unlock()`. LVGL event callbacks already run under the lock.
- Pass `display_hw_lvgl_required_fbs()` to `display_hw_init()` so the RGB panel allocates the framebuffers the adapter expects.

## Kconfig configuration matrix (`menuconfig` → Display hardware)

| Option | Default | Effect |
| :--- | :--- | :--- |
| `DISPLAY_HW_BOUNCE_BUFFER_LINES` | `20` | Height of the internal-RAM bounce buffer for the RGB panel (480 x N pixels). Keep it above 0: without it PSRAM bandwidth causes drift (see [TOO.MD](TOO.MD)). |
| `DISPLAY_HW_TOUCH_USE_INTERRUPT` | `y` | Read the GT911 on INT edges (GPIO16) instead of polling. |
| `DISPLAY_HW_TOUCH_RESET_WAIT_MS` | `200` | Delay after releasing the GT911 reset before talking to it. |

See [README.md](README.md) for the pin map and [TOO.MD](TOO.MD) for the bring-up sequence.
