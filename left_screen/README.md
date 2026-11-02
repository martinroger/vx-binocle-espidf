# Left Display Board (LDB)

## Overview
The **Left Display Board (LDB)** drives the primary digital instrument cluster screen located in the left binocle pod. It renders real-time engine RPM (or the estimated gear), the shift light and RPM alarm, fuel level, coolant temperature, battery voltage, odometer/trip information, and warning telltales using LVGL v9. Vehicle speed is shown on the right display.

## Hardware Architecture
- **Microcontroller**: ESP32-S3 (240 MHz dual-core, 16 MB flash, 8 MB octal PSRAM).
- **Display Controller**: 480x480 ST7701 RGB panel with GT911 touch, driven by plain ESP-IDF `esp_lcd` drivers in [`common/display_hw`](../common/display_hw) and rendered through `espressif/esp_lvgl_adapter`.
- **Audio Buzzer**: Buzzer on TCA9554 IO expander pin 7, switched on while the over-temperature telltale is on (when enabled in settings).
- **CAN Interface**: On-board TWAI transceiver (TX GPIO 19, RX GPIO 20) receiving `ITF_fast_metrics`, `ITF_slow_metrics`, `ITF_active_hi_lo`, `ITF_odometer`, `ITF_board_st`, `ITF_board_version`, the external oil and charge-cooling frames, the other display's status frame, and OTA requests.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For complete UI rendering pipeline and CAN dispatch details, see [TOO.MD](TOO.MD).
