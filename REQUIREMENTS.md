# Project Requirements & Agent Operational Scope (`REQUIREMENTS.md`)

This document details the agent operational requirements, architecture guidelines, design rules, and implementation traceability matrix for the `vx-binocle-espidf` repository.

---

## 1. System Requirements & Operational Boundaries

### REQ-01: ESP-IDF Version Compatibility
- **Target Line**: ESP-IDF >= v6.0 (Active sub-projects `interface_board`, `ITF factory app`, `left display factory app`, `right display factory app` migrated to ESP-IDF v6.2.0).
- **Exclusion Scope**: `left_screen` and `right_screen` are excluded due to un-migrated legacy `esp_lcd` third-party component dependencies (`esp32_display_panel` / `esp32_io_expander`).
- **Execution Tool**: Must use `eim` (Espressif Installation Manager) for virtual environment execution (e.g., `eim run "<idf.py command>" master`).
- **Traceability**: Managed via `main/idf_component.yml` manifests and `sdkconfig.defaults`.

### REQ-02: Target Chip Hardware Verification
- **Primary SoC**: ESP32-S3 across all primary sub-projects (`interface_board`, `left_screen`, `right_screen`, `factory apps/*`).
- **Configuration**: Standardized on 16MB QIO Flash, Octal SPI RAM (PSRAM), CPU frequency 240MHz, 1000Hz FreeRTOS tick rate.

### REQ-03: Non-Factory Screen Code Parity & Macro Differentiation
- **Core Code Parity**: Core application logic in `left_screen/main/main.cpp` and `right_screen/main/main.cpp` MUST remain identical.
- **Macro Switch Differentiation**: Functional differences between screens are strictly controlled via compile-time Kconfig macro switches:
  - `CONFIG_LEFT_SIDE_DISPLAY=y` (Left screen)
  - `CONFIG_RIGHT_SIDE_DISPLAY=y` (Right screen)
- **UI Exemption**: Automatically generated UI components under `components/ui` or EEZ Studio generated code are exempt from this parity constraint.

### REQ-04: Shared Component Reusability & Compatibility
- Shared logic under `common/` (`twai_daemon`, `binocan`, `lvgl_v9_port`, `coefficients.h`) must remain compatible across all consuming sub-projects (`interface_board`, `left_screen`, `right_screen`, `factory apps/*`).
- `twai_daemon` must use ESP-IDF v6 driver headers (`esp_driver_twai`, `esp_driver_gpio`).

### REQ-05: Dependency Tracking & Matrix Maintenance
- Top-level [`README.md`](file:///home/martinroger/Documents/vx-binocle-espidf/README.md) MUST maintain an updated matrix of all external dependencies, locally installed versions (`dependencies.lock`), and requirement declarations (`idf_component.yml`).

---

## 2. Implementation Traceability Matrix

| Requirement | Description | Primary Code / Config Files | Verification Method |
| :--- | :--- | :--- | :--- |
| **REQ-01** | ESP-IDF v6.2.0 execution & manifest constraints | [`interface_board/main/idf_component.yml`](file:///home/martinroger/Documents/vx-binocle-espidf/interface_board/main/idf_component.yml)<br>[`left_screen/main/idf_component.yml`](file:///home/martinroger/Documents/vx-binocle-espidf/left_screen/main/idf_component.yml)<br>[`right_screen/main/idf_component.yml`](file:///home/martinroger/Documents/vx-binocle-espidf/right_screen/main/idf_component.yml) | `eim run "idf.py reconfigure" master` |
| **REQ-02** | Target SoC & flash/PSRAM hardware settings | [`interface_board/sdkconfig.defaults`](file:///home/martinroger/Documents/vx-binocle-espidf/interface_board/sdkconfig.defaults)<br>[`left_screen/sdkconfig.defaults`](file:///home/martinroger/Documents/vx-binocle-espidf/left_screen/sdkconfig.defaults)<br>[`right_screen/sdkconfig.defaults`](file:///home/martinroger/Documents/vx-binocle-espidf/right_screen/sdkconfig.defaults) | `CONFIG_IDF_TARGET="esp32s3"` in build config |
| **REQ-03** | Screen code parity in `main/main.cpp` | [`left_screen/main/main.cpp`](file:///home/martinroger/Documents/vx-binocle-espidf/left_screen/main/main.cpp)<br>[`right_screen/main/main.cpp`](file:///home/martinroger/Documents/vx-binocle-espidf/right_screen/main/main.cpp) | Diff check between `main.cpp` files |
| **REQ-04** | Shared CAN & display component driver compatibility | [`common/twai_daemon/CMakeLists.txt`](file:///home/martinroger/Documents/vx-binocle-espidf/common/twai_daemon/CMakeLists.txt)<br>[`common/binocan/CMakeLists.txt`](file:///home/martinroger/Documents/vx-binocle-espidf/common/binocan/CMakeLists.txt)<br>[`common/lvgl_v9_port/CMakeLists.txt`](file:///home/martinroger/Documents/vx-binocle-espidf/common/lvgl_v9_port/CMakeLists.txt) | Sub-project reconfigure & compilation |
| **REQ-05** | Global dependency summary matrix in root README | [`README.md`](file:///home/martinroger/Documents/vx-binocle-espidf/README.md) | Visual inspection against `dependencies.lock` |
