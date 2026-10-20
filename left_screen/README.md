# Left Display Board (LDB)

## Overview
The **Left Display Board (LDB)** drives the primary digital instrument cluster screen located in the left binocle pod. It renders real-time engine RPM (with optional gear estimator readout), vehicle speed, coolant temperature, odometer/trip information, and critical warning icons using LVGL v9.

## Hardware Architecture
- **Microcontroller**: ESP32-S3 (240 MHz dual-core, 8 MB Flash, 8 MB Octal PSRAM).
- **Display Controller**: 480x480 ST7701 RGB panel with GT911 touch, driven by plain ESP-IDF `esp_lcd` drivers in [`common/display_hw`](../common/display_hw) and rendered through `espressif/esp_lvgl_adapter`.
- **Audio Buzzer**: Buzzer on TCA9554 IO expander pin 7, switched on and off for over-temperature alarms.
- **CAN Interface**: On-board TWAI transceiver listening to `ITF_values`, `ITF_status`, and `ITF_debug`.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For complete UI rendering pipeline and CAN dispatch details, see [TOO.MD](TOO.MD).
