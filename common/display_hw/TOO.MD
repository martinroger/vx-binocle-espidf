# display_hw — Theory of Operation

## 1. Responsibilities

`display_hw` owns every peripheral of the Waveshare 2.8C display board that is not the CAN transceiver: the shared I2C bus, the TCA9554 IO expander, the LEDC backlight, the GT911 touch controller and the ST7701 RGB panel. `display_hw_lvgl` sits on top and hands the panel and touch handles to `esp_lvgl_adapter`, which owns the LVGL task, tick, lock and flush pipeline.

```mermaid
graph TD
    APP[left_screen / right_screen app_main] -->|display_hw_init| HW[display_hw]
    APP -->|display_hw_lvgl_start| GLUE[display_hw_lvgl]
    HW --> BL[LEDC backlight GPIO6]
    HW --> I2C[I2C0 master bus]
    I2C --> EXP[TCA9554 0x20]
    I2C --> TP[GT911 0x5D]
    EXP -->|P2 CS| SPI[3-wire SPI GPIO1/2]
    SPI --> LCD[ST7701]
    HW -->|esp_lcd_new_rgb_panel| RGB[RGB bus + 2 PSRAM framebuffers]
    RGB --> LCD
    GLUE --> ADP[esp_lvgl_adapter task, core 1]
    ADP -->|DOUBLE_DIRECT flush| RGB
    ADP -->|interrupt-driven read| TP
```

## 2. Bring-up sequence

```mermaid
sequenceDiagram
    participant APP as app_main
    participant HW as display_hw
    participant EXP as TCA9554
    participant TP as GT911
    participant LCD as ST7701 / RGB
    participant ADP as esp_lvgl_adapter

    APP->>HW: display_hw_init(num_fbs)
    HW->>HW: GPIO6 low, LEDC timer/channel, duty 0
    HW->>HW: i2c_new_master_bus(I2C0)
    alt I2C or LEDC error
        HW-->>APP: error (resources released)
        APP->>APP: XDB_SM_ST_DEGRADED, attemptRollBack()
    end
    HW->>EXP: new_i2c_tca9554 (reset: all inputs, out=0xFF)
    HW->>EXP: clear P7 in output reg, then P1/P7 to output
    HW->>TP: INT16 low, P1 low 10 ms, P1 high, wait 200 ms, INT16 released
    HW->>TP: esp_lcd_touch_new_i2c_gt911(0x5D, INT16)
    alt GT911 not answering
        TP-->>HW: error, continue without touch
    end
    HW->>LCD: 3-wire SPI IO (CS via expander P2)
    HW->>LCD: esp_lcd_new_panel_st7701 (RGB config, num_fbs, bounce 480x20)
    HW->>LCD: reset, init (vendor table, sleep-out 120 ms, display on)
    alt LCD error
        HW-->>APP: error (touch, expander, I2C released)
        APP->>APP: XDB_SM_ST_DEGRADED, attemptRollBack()
    end
    HW-->>APP: ESP_OK, or ESP_ERR_NOT_FOUND if touch missing
    APP->>ADP: display_hw_lvgl_start(task cfg)
    ADP->>ADP: init, register display (DOUBLE_DIRECT), register touch, start task
    alt adapter error
        ADP-->>APP: error
        APP->>APP: XDB_SM_ST_DEGRADED, attemptRollBack()
    end
    APP->>APP: ui_init, mark app valid, backlight fade-in
```

## 3. Design notes

- **One I2C driver.** The expander and touch use the `i2c_master` driver on one bus handle. The legacy `driver/i2c.h` driver must not be used anywhere in an app that links this component: ESP-IDF aborts at boot when both are active.
- **Buzzer pin at boot.** The TCA9554 driver resets the chip to all-inputs with output register 0xFF, and `esp_io_expander_set_level()` refuses input pins. The output register is therefore written directly to clear P7 before the pin becomes an output.
- **GT911 address.** RST is on the expander, so the GT911 driver's own GPIO reset is disabled (`rst_gpio_num = NC`). Holding INT low through reset selects address 0x5D.
- **ST7701 init table.** Copied byte for byte from the previous board definition, including COLMOD `0x66` while the RGB bus carries RGB565. The array is `static const` because the driver keeps the pointer until `esp_lcd_panel_init()`. The control IO is kept alive (`enable_io_multiplex = 0`, `mirror_by_cmd = 1`).
- **Bounce buffer.** The project runs a 64 B data cache line with 80 MHz octal PSRAM. Without RGB bounce buffers this combination can make the image drift, so `DISPLAY_HW_BOUNCE_BUFFER_LINES` defaults to 20 and a `#warning` fires if it is set to 0. `SPIRAM_XIP_FROM_PSRAM` keeps the RGB ISR running during flash writes (CAN OTA).
- **Tear mode.** `DOUBLE_DIRECT`: two full framebuffers in PSRAM (2 x 460.8 KB) and LVGL DIRECT render mode, the same layout as the previous port. The adapter's RGB default (`TRIPLE_PARTIAL`) would need a third framebuffer.
- **Thread safety.** `display_hw_backlight_set()` is a LEDC register write and safe from any task. `display_hw_buzzer_set()` goes through the I2C bus lock and can wait for an in-flight touch read.
