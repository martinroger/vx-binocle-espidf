# Project Requirements & Agent Operational Scope (`REQUIREMENTS.md`)

This document details the agent operational requirements, architecture guidelines, design rules, and implementation traceability matrix for the `vx-binocle-espidf` repository.

---

## 1. System Requirements & Operational Boundaries

### REQ-01: ESP-IDF Version Compatibility & Sub-project Target Enforcer
- **ESP-IDF v6.2.0 (`master`) Target Scope**: `interface_board`, `ITF factory app`, `left display factory app`, and `right display factory app` target ESP-IDF v6.2.0 (`eim run "<cmd>" master`) with manifest constraints `idf: version: '>=6.2.0'`.
- **ESP-IDF v5.5.5 (`v5.5.5`) Target Scope**: `left_screen` and `right_screen` target ESP-IDF v5.5.5 exclusively (`eim run "<cmd>" v5.5.5`) with manifest constraints `idf: version: '>=5.5.5'`.
- **Ignored / Excluded Scope**: `emulator-console` and `bennu` are ignored from firmware build evaluation and migration efforts per repository scope rules.
- **Execution Tool**: Must use `eim` (Espressif Installation Manager) for virtual environment execution (e.g., `eim run "<cmd>" master` or `eim run "<cmd>" v5.5.5`).
- **Traceability**: Managed via `main/idf_component.yml` manifests and `sdkconfig.defaults`.

### REQ-02: Target Chip Hardware Verification
- **Primary SoC**: ESP32-S3 across all primary sub-projects (`interface_board`, `left_screen`, `right_screen`, `factory apps/*`).
- **Configuration**: Standardized on 16MB QIO Flash, Octal SPI RAM (PSRAM), CPU frequency 240MHz, 1000Hz FreeRTOS tick rate.

### REQ-03: Non-Factory Screen Code Parity Across `main/` Subfolder
- **Core File Parity**: ALL files located in the `main/` subfolder across `left_screen` and `right_screen` (`main.cpp`, `twai_ops.hpp`, `start_animation.hpp`, `global_vars.hpp`, `updateUI.hpp`, `CMakeLists.txt`, `idf_component.yml`, `Kconfig.projbuild`, etc.) MUST maintain 100% byte-for-byte file content parity.
- **Macro Switch Differentiation**: Functional differences between left and right displays are strictly controlled via compile-time Kconfig macro switches:
  - `CONFIG_LEFT_SIDE_DISPLAY=y` (Left screen)
  - `CONFIG_RIGHT_SIDE_DISPLAY=y` (Right screen)
- **UI Exemption**: Automatically generated UI components under `components/ui` or EEZ Studio generated code are exempt from this parity constraint.

### REQ-04: Shared Component Reusability & Compatibility
- Shared logic under `common/` (`twai_daemon`, `binocan`, `lvgl_v9_port`, `coefficients.h`) must remain compatible across all consuming sub-projects (`interface_board`, `left_screen`, `right_screen`, `factory apps/*`).
- `twai_daemon` must use ESP-IDF driver headers (`esp_driver_twai`, `esp_driver_gpio`).

### REQ-05: Dependency Tracking & Matrix Maintenance
- Top-level [`README.md`](README.md) MUST maintain an updated matrix of all external dependencies, locally installed versions (`dependencies.lock`), and requirement declarations (`idf_component.yml`).

---

## 2. Implementation Traceability Matrix

| Requirement | Description | Primary Code / Config Files | Verification Method |
| :--- | :--- | :--- | :--- |
| **REQ-01** | ESP-IDF target execution & manifest constraints | [`interface_board/main/idf_component.yml`](interface_board/main/idf_component.yml)<br>[`left_screen/main/idf_component.yml`](left_screen/main/idf_component.yml)<br>[`right_screen/main/idf_component.yml`](right_screen/main/idf_component.yml) | `eim run "idf.py reconfigure" <master\|v5.5.5>` |
| **REQ-02** | Target SoC & flash/PSRAM hardware settings | [`interface_board/sdkconfig.defaults`](interface_board/sdkconfig.defaults)<br>[`left_screen/sdkconfig.defaults`](left_screen/sdkconfig.defaults)<br>[`right_screen/sdkconfig.defaults`](right_screen/sdkconfig.defaults) | `CONFIG_IDF_TARGET="esp32s3"` in build config |
| **REQ-03** | Screen file parity across all `main/` files | [`left_screen/main/`](left_screen/main)<br>[`right_screen/main/`](right_screen/main) | `diff -r left_screen/main right_screen/main` |
| **REQ-04** | Shared CAN & display component driver compatibility | [`common/twai_daemon/CMakeLists.txt`](common/twai_daemon/CMakeLists.txt)<br>[`common/binocan/CMakeLists.txt`](common/binocan/CMakeLists.txt)<br>[`common/lvgl_v9_port/CMakeLists.txt`](common/lvgl_v9_port/CMakeLists.txt) | Sub-project reconfigure & compilation |
| **REQ-05** | Global dependency summary matrix in root README | [`README.md`](README.md) | Visual inspection against `dependencies.lock` |
