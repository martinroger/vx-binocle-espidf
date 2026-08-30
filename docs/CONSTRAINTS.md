# VX Binocle Operational & Architecture Constraints

## 1. Platform & Tooling Constraints

- **Target SoC**: Espressif **ESP32-S3** (dual-core Xtensa LX7, 240 MHz).
- **Target Framework Version**: **ESP-IDF v5.5.x** (v5.5.0 to v5.5.5). Any backward compatibility anterior to v5.5 or future v6.x migrations must be guarded with appropriate preprocessor version checks (`ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)`).
- **Build Clean Invariant**: Whenever `Kconfig`, `Kconfig.projbuild`, or `sdkconfig.defaults` are modified, `idf.py fullclean build` must be executed to ensure clean CMake cache re-evaluation.
- **Managed Components Boundary**: The `managed_components/` directory across all subprojects is strictly READ-ONLY. No local patches or modifications are permitted within managed components.

---

## 2. Hardware & Electrical Safety Constraints

- **Strapping Pins (ESP32-S3)**: Avoid using strapping GPIOs (GPIO 0, 3, 45, 46) for critical output functions that could force unwanted boot modes during power transitions.
- **Power Rail Sequencing**: The 5V peripheral step-up/regulator stage must be actively disabled before triggering software reboots to prevent brownout conditions and reverse current flow.
- **MCPWM Low-Side Drive Inversion**: Emulator MCPWM output generators must be inverted at the GPIO matrix to properly sink/switch vehicle pull-up circuits without float states.
- **ADC Voltage Limits**: Internal ESP32-S3 ADCs and ADS1115 external ADCs must be strictly protected via voltage dividers/clamping to avoid exceeding 3.3V reference limits.
- **Hardware v0.3 Pin Alignment**: In Interface Board HW v0.3, PWM Coolant and PWM RPM pins are swapped relative to legacy prototypes. Firmware defaults must reflect the updated board revision.

---

## 3. Software & Real-Time Constraints

- **FreeRTOS Task Core Pinning**:
  - **Core 0**: Protocol daemons, TWAI/CAN bus transmission/reception, ADC sampling loops, and Wi-Fi/HTTP network stacks.
  - **Core 1**: LVGL rendering pipeline, UI animation loops, and display buffer flushing.
- **Task Stack Allocations**: Minimum 4096 bytes allocated to FreeRTOS tasks interacting with C standard library, floating-point math, or logging.
- **TWAI Modern Driver Architecture & Bus-Off Recovery**:
  - The modernized `esp_driver_twai` operates on a zero-copy pointer model; all asynchronous transmissions must utilize persistent descriptor slots (`s_tx_slots` pool managed by `twai_daemon`) with ISR recycling via `on_tx_done` to prevent stack dangling pointer corruptions.
  - The CAN driver handles `TWAI_ERROR_BUS_OFF` and disconnected hardware gracefully via `on_state_change` ISR callbacks triggering non-blocking `twai_node_recover()` without watchdog resets or task hangs.
  - All nodes (including display factory apps) must initialize the TWAI peripheral to hold transceiver TX lines recessive (logic HIGH) preventing floating pin bus disruption.
- **HTTP Server Handler Capacity**: Any project registering REST endpoints, static files, and WebSockets must set `httpd_cfg.max_uri_handlers` to at least 16 (exceeding the default of 8).

---

## 4. OTA & Flash Partition Constraints

- **Dual-Bank Partition Alignment**: Factory partitions and dual OTA slots (`ota_0`, `ota_1`) must be 64KB-aligned with identical sizing (minimum 2 MB each for display firmware, 1.5 MB for interface board).
- **OTA Flash Write Watchdog Safety**: During in-flight SPI flash sector erasure and write operations (via Web or CAN), the OTA task must yield periodically (`vTaskDelay(1)`) to avoid triggering the Task Watchdog Timer (TWDT).
- **CAN Bus Bandwidth Throttling**: OTA-over-CAN block streaming must observe inter-frame pacing or flow-control ACKs to prevent bus saturation and preserve minimum timing margins for arbitration.
