# display_hw

Hardware bring-up for the Waveshare ESP32-S3-Touch-LCD-2.8C display board, on plain ESP-IDF `esp_lcd` drivers, plus the LVGL glue through [`espressif/esp_lvgl_adapter`](https://components.espressif.com/components/espressif/esp_lvgl_adapter).

Used by [`left_screen`](../../left_screen) and [`right_screen`](../../right_screen).

## Using it in an app

Add the directory to `EXTRA_COMPONENT_DIRS` in the app's top-level `CMakeLists.txt`, before `include($ENV{IDF_PATH}/tools/cmake/project.cmake)`, and list `display_hw` in `REQUIRES` of the component that uses it:

```cmake
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../common/display_hw)
```

Do not add it as a `path:` dependency in `idf_component.yml`: the component manager writes local path dependencies to `dependencies.lock` as absolute paths. The registry dependencies in this component's own [`idf_component.yml`](idf_component.yml) are still resolved and locked.

## Hardware

| Function | Part | Connection |
| :--- | :--- | :--- |
| LCD | ST7701, 480 x 480, RGB565 over a 16-bit RGB bus | HSYNC 38, VSYNC 39, DE 40, PCLK 41 (18 MHz), D0..D15 = 5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17 |
| LCD control | 3-wire SPI (9-bit, DC bit) | SCK GPIO2, SDA GPIO1, CS on expander P2 |
| Touch | GT911 at 0x5D | I2C0 (SCL 7, SDA 15, 400 kHz), INT GPIO16, RST on expander P1 |
| IO expander | TCA9554 at 0x20 | I2C0, shared with touch. P1 touch reset, P2 LCD CS, P7 buzzer |
| Backlight | LEDC PWM | GPIO6, 5 kHz, 10-bit, timer 0 / channel 0, low-speed mode |

All pins and bus speeds are macros in [`include/display_hw.h`](include/display_hw.h).

## API

- [`display_hw_init()`](include/display_hw.h): backlight (off), I2C, expander, touch, LCD. Returns the panel, control IO, touch and expander handles.
- [`display_hw_backlight_set()`](include/display_hw.h): brightness in percent.
- [`display_hw_buzzer_set()`](include/display_hw.h): buzzer on expander P7.
- [`display_hw_lvgl_required_fbs()`](include/display_hw_lvgl.h), [`display_hw_lvgl_start()`](include/display_hw_lvgl.h): adapter init, display and touch registration, LVGL task start (tear mode `DOUBLE_DIRECT`, rotation 0).
- [`display_hw_lvgl_lock()` / `display_hw_lvgl_unlock()`](include/display_hw_lvgl.h): recursive LVGL lock.

## Configuration (`menuconfig` → Display hardware)

| Option | Default | Meaning |
| :--- | :--- | :--- |
| `DISPLAY_HW_BOUNCE_BUFFER_LINES` | 20 | RGB bounce buffer height. Keep it above 0 (see [TOO.MD](TOO.MD)). |
| `DISPLAY_HW_TOUCH_USE_INTERRUPT` | y | Read the GT911 on INT edges instead of polling. |
| `DISPLAY_HW_TOUCH_RESET_WAIT_MS` | 200 | Wait after releasing GT911 reset. |

Requires ESP-IDF >= 5.5.5 (adapter requirement). See [TOO.MD](TOO.MD) for the bring-up sequence and design notes, and [HOWTO.md](HOWTO.md) for a copy-paste integration guide.
