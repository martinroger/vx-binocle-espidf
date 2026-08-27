# Right Display Board (RDB)

## Overview
The **Right Display Board (RDB)** drives the secondary digital cluster display located in the right binocle pod. It renders auxiliary telemetry, fuel gauge, battery voltage, oil pressure, trip statistics, and diagnostic messages using LVGL v9.

## Hardware Architecture
- **Microcontroller**: ESP32-S3 (240 MHz dual-core, 8 MB Flash, 8 MB Octal PSRAM).
- **Display Controller**: High-resolution LCD panel driven via ESP32 Display Panel (`esp32_display_panel`).
- **CAN Interface**: On-board TWAI transceiver listening to CAN broadcast messages.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For complete UI rendering pipeline and CAN dispatch details, see [TOO.MD](TOO.MD).
