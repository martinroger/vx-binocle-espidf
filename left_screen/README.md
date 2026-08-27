# Left Display Board (LDB)

## Overview
The **Left Display Board (LDB)** drives the primary digital instrument cluster screen located in the left binocle pod. It renders real-time engine RPM, vehicle speed, coolant temperature, odometer/trip information, and critical warning icons using LVGL v9.

## Hardware Architecture
- **Microcontroller**: ESP32-S3 (240 MHz dual-core, 8 MB Flash, 8 MB Octal PSRAM).
- **Display Controller**: High-resolution LCD panel driven via ESP32 Display Panel (`esp32_display_panel`).
- **Audio Buzzer**: Hardware PWM buzzer on GPIO 4 for over-temperature and critical fault audio alarms.
- **CAN Interface**: On-board TWAI transceiver listening to `ITF_values`, `ITF_status`, and `ITF_debug`.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For complete UI rendering pipeline and CAN dispatch details, see [TOO.MD](TOO.MD).
