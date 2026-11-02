# Right Display Board (RDB)

## Overview
The **Right Display Board (RDB)** drives the secondary digital cluster display located in the right binocle pod. It renders vehicle speed (arc and readout, km/h or mph), fuel level, coolant temperature, battery voltage, odometer/trip information, warning telltales (including low oil pressure), and diagnostic information using LVGL v9.

## Hardware Architecture
- **Microcontroller**: ESP32-S3 (240 MHz dual-core, 16 MB flash, 8 MB octal PSRAM).
- **Display Controller**: 480x480 ST7701 RGB panel with GT911 touch, driven by plain ESP-IDF `esp_lcd` drivers in [`common/display_hw`](../common/display_hw) and rendered through `espressif/esp_lvgl_adapter`.
- **CAN Interface**: On-board TWAI transceiver (TX GPIO 19, RX GPIO 20) listening to the interface board broadcast frames.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For complete UI rendering pipeline and CAN dispatch details, see [TOO.MD](TOO.MD).
