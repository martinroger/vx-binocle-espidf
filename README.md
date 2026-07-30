# vx-binocle-espidf

[![Build and attach to release](https://github.com/martinroger/vx-binocle-espidf/actions/workflows/release.yml/badge.svg?event=release)](https://github.com/martinroger/vx-binocle-espidf/actions/workflows/release.yml)

## Project Purpose & Architecture Overview

`vx-binocle-espidf` is a multi-target ESP32-S3 firmware repository driving the digital gauge display and interface system for automotive instrumentation. The system consists of two display units (left screen and right screen) running LVGL v9, an interface board managing CAN/TWAI communication, analog sensors, and power management, alongside factory diagnostic applications.

### Repository Structure

```
vx-binocle-espidf/
├── common/                  # Shared components & drivers across firmware applications
│   ├── binocan/             # CAN/TWAI DBC message codec engine
│   ├── coefficients.h       # Universal sensor calibration formulas & conversion scale macros
│   ├── lvgl_v9_port/        # LVGL v9 display & input driver abstraction
│   └── twai_daemon/         # Real-time FreeRTOS TWAI/CAN bus router daemon
├── interface_board/         # Main IO interface board firmware (ESP32-S3)
├── left_screen/             # Left cluster screen application (ESP32-S3, LVGL v9)
├── right_screen/            # Right cluster screen application (ESP32-S3, LVGL v9)
├── factory apps/            # Production diagnostic & test firmware
│   ├── ITF factory app/     # Interface board diagnostic firmware
│   ├── left display factory app/  # Left display factory diagnostic app
│   └── right display factory app/ # Right display factory diagnostic app
├── emulator-console/        # Console debugging emulator tool
├── bennu/                   # Bootloader & storage app
└── analysis/                # Data logs & capture analysis
```

---

## Agent Scope & Operational Boundaries

- **Target Architecture**: ESP32-S3 (`CONFIG_IDF_TARGET="esp32s3"`).
- **Target Framework Environments**:
  - **ESP-IDF v6.2.0 (`master`)**: Enforced for `interface_board`, `ITF factory app`, `left display factory app`, and `right display factory app` via `eim run "<cmd>" master`.
  - **ESP-IDF v5.5.5 (`v5.5.5`)**: Enforced exclusively for `left_screen` and `right_screen` via `eim run "<cmd>" v5.5.5`.
- **Ignored / Excluded Sub-Projects**: `emulator-console` and `bennu` are excluded from firmware migration and build evaluation per repository rules.
- **Screen Code Parity Rule**: ALL files within the `main/` subfolder across `left_screen` and `right_screen` must maintain 100% strict file parity, differentiated strictly via Kconfig macros (e.g., `CONFIG_LEFT_SIDE_DISPLAY` vs `CONFIG_RIGHT_SIDE_DISPLAY`).
- **Documentation & Requirements Tracking**: Operational requirements and constraints are explicitly tracked in [`REQUIREMENTS.md`](REQUIREMENTS.md) and [`CONSTRAINTS.md`](CONSTRAINTS.md).

---

## Sub-Project Migration & Build Status Matrix

The table below tracks target framework environments and build status across all sub-projects.

| Sub-Project | Target Chip | Manifest IDF Version | Target ESP-IDF | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`interface_board`** | ESP32-S3 | `>=6.2.0` | **v6.2.0 (`master`)** | **PASSED** | Binary compiled cleanly under ESP-IDF v6.2.0. |
| **`factory apps / ITF factory app`** | ESP32-S3 | `>=6.2.0` | **v6.2.0 (`master`)** | **PASSED** | Binary `ITF_factory.bin` compiled cleanly under ESP-IDF v6.2.0. |
| **`factory apps / left display factory app`** | ESP32-S3 | `>=6.2.0` | **v6.2.0 (`master`)** | **PASSED** | Binary `LDB_factory.bin` compiled cleanly under ESP-IDF v6.2.0. |
| **`factory apps / right display factory app`** | ESP32-S3 | `>=6.2.0` | **v6.2.0 (`master`)** | **PASSED** | Binary `RDB_factory.bin` compiled cleanly under ESP-IDF v6.2.0. |
| **`left_screen`** | ESP32-S3 | `>=5.5.5` | **v5.5.5 (`v5.5.5`)** | **PASSED** | Compiled cleanly under ESP-IDF v5.5.5; uses `twai_daemon` API. |
| **`right_screen`** | ESP32-S3 | `>=5.5.5` | **v5.5.5 (`v5.5.5`)** | **PASSED** | Compiled cleanly under ESP-IDF v5.5.5; uses `twai_daemon` API. |
| **`emulator-console`** | ESP32-S3 | N/A | N/A | **IGNORED** | Ignored per repository scope rules. |
| **`bennu`** | ESP32-S3 | N/A | N/A | **IGNORED** | Ignored per repository scope rules. |

---

## External Dependency Summary Matrix

The table below summarizes external component dependencies managed via `idf_component.yml` and `dependencies.lock` across all sub-projects.

| Sub-Project | External Dependency | Locally Installed Version | Version Requirement |
| :--- | :--- | :--- | :--- |
| **`interface_board`** | `esp-idf-lib/ads111x` | `1.1.14` | `^1.1.14` |
| | `esp-idf-lib/tca95x5` | `1.0.7` | `^1.0.7` |
| | `esp-idf-lib/esp_idf_lib_helpers` *(transient)* | `1.4.0` | `*` |
| | `esp-idf-lib/i2cdev` *(transient)* | `2.1.2` | `*` |
| | `twai_daemon` *(local)* | `local` | `../../common/twai_daemon` |
| | `binocan` *(local)* | `local` | `../../common/binocan` |
| **`left_screen`** | `espressif/esp32_display_panel` | `1.0.4` | `^1.0.2` |
| | `lvgl/lvgl` | `9.4.0` | `^9.4.0` |
| | `espressif/esp-lib-utils` *(transient)* | `0.2.3` | `0.2.*` |
| | `espressif/esp32_io_expander` *(transient)* | `1.1.1` | `1.*` |
| | `twai_daemon` *(local)* | `local` | `../../common/twai_daemon` |
| | `binocan` *(local)* | `local` | `../../common/binocan` |
| | `lvgl_v9_port` *(local)* | `local` | `../../common/lvgl_v9_port` |
| **`right_screen`** | `espressif/esp32_display_panel` | `1.0.4` | `^1.0.2` |
| | `lvgl/lvgl` | `9.4.0` | `^9.4.0` |
| | `espressif/esp-lib-utils` *(transient)* | `0.2.3` | `0.2.*` |
| | `espressif/esp32_io_expander` *(transient)* | `1.1.1` | `1.*` |
| | `twai_daemon` *(local)* | `local` | `../../common/twai_daemon` |
| | `binocan` *(local)* | `local` | `../../common/binocan` |
| | `lvgl_v9_port` *(local)* | `local` | `../../common/lvgl_v9_port` |
| **`factory apps / ITF factory app`** | `espressif/mdns` | `1.9.1` | `*` |
| | `twai_daemon` *(local)* | `local` | `../../../common/twai_daemon` |
| | `binocan` *(local)* | `local` | `../../../common/binocan` |
| **`factory apps / left display factory app`** | `espressif/mdns` | `1.9.1` | `*` |
| **`factory apps / right display factory app`** | `espressif/mdns` | `1.9.1` | `*` |
| **`emulator-console`** | `espressif/ESP32_IO_Expander` | `1.1.1` | `*` |
| | `espressif/esp-lib-utils` *(transient)* | `0.2.3` | `0.2.*` |
| **`bennu`** | *None* | N/A | N/A |
