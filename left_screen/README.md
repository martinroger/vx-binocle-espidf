# Left Screen Cluster Firmware (`left_screen`)

`left_screen` is the primary instrument cluster display application for the **Left Screen** of the digital gauge cluster system.

---

## Technical Overview

- **Target SoC**: ESP32-S3 (16MB Flash, Octal PSRAM)
- **Display Driver**: `esp32_display_panel` (ST7701 4.0" 480x480 RGB interface)
- **UI Engine**: LVGL v9 (`lvgl/lvgl` 9.4.0 via `common/lvgl_v9_port`)
- **CAN Bus Integration**: Consumes vehicle telemetry (speed, RPM, coolant temperature, fuel level, battery voltage, odometer) from `interface_board` via `common/twai_daemon` and `common/binocan`.
- **Target ESP-IDF Version**: ESP-IDF v5.5.5 (`eim run "<idf.py command>" v5.5.5`)
- **Configuration Switch**: `CONFIG_LEFT_SIDE_DISPLAY=y`

---

## Source Parity Notice

Per repository design rules (Rule 6), source files within `main/` maintain **100% byte-for-byte content parity** with `right_screen/main/`. Functional differences are differentiated strictly at compile-time via `CONFIG_LEFT_SIDE_DISPLAY`.

---

## Documentation

For a detailed architectural breakdown of display drivers, LVGL porting, CAN ingestion, and UI update loops, see [TOO.MD](TOO.MD).
