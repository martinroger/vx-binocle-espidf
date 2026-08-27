# VX Binocle System Requirements & Traceability

## 1. System Overview & Scope

The **VX Binocle** project is an automotive dual-display digital instrument cluster and signal acquisition system designed for retrofitting classic vehicles (specifically Opel Speedster / Vauxhall VX220). The ecosystem comprises multiple specialized ESP32-S3 boards communicating via a unified CAN bus (TWAI):

1. **Interface Board (ITF)**: Real-time acquisition of vehicle sensors (vehicle speed, engine RPM, coolant temperature, fuel level, battery voltage, analog inputs, and digital switch states) and transmission over CAN.
2. **Left Display Board (LDB)**: Circular/oval LCD cluster display showing primary instrumentation (RPM arc, vehicle speed, warning indicators, odometer/trip, and coolant temperature).
3. **Right Display Board (RDB)**: Secondary LCD cluster display showing secondary telemetry, auxiliary gauges, trip computer, and status diagnostics.
4. **Factory Apps (ITF / LDB / RDB)**: Dedicated recovery, calibration, and OTA web provisioning firmware for NVS configuration and sensor zeroing.
5. **Vehicle Emulator (Console & ESPHome)**: Bench testing suite simulating vehicle signals (pulse trains, resistor ladders, switch states, and drive cycles) via UART console, Node-RED, or ESPHome.

---

## 2. Functional Requirements

### 2.1 Interface Board (ITF)
- **REQ-ITF-001 (Speed Pulse Capture)**: Accurately measure vehicle wheel speed pulse trains via MCPWM capture / PCNT and convert to km/h using configured wheel circumference and pulses-per-km.
- **REQ-ITF-002 (Engine RPM Pulse Capture)**: Measure ignition/tacho pulse frequency via MCPWM capture and calculate engine RPM based on cylinder count.
- **REQ-ITF-003 (Analog Signal Ingestion)**: Sample fuel sender voltage, coolant temperature sensor, oil pressure, and board voltage via external 16-bit ADC (ADS1115) and internal ADC.
- **REQ-ITF-004 (Signal Filtering)**: Apply Simple Moving Average (SMA) filtering with bounded raw signals to eliminate jitter.
- **REQ-ITF-005 (CAN Telemetry Broadcast)**: Broadcast standard CAN messages (`ITF_status`, `ITF_values`, `ITF_debug`) over TWAI at periodic rates (20ms, 50ms, 100ms).
- **REQ-ITF-006 (Persistent Odometer)**: Maintain high-resolution vehicle odometer and trip distance in non-volatile storage (NVS) with wear-leveling.

### 2.2 Left & Right Displays (LDB & RDB)
- **REQ-DSP-001 (High-Speed UI Rendering)**: Render fluid gauge animations (60 FPS target) using LVGL v9 and custom White Rabbit typography.
- **REQ-DSP-002 (CAN Telemetry Ingestion)**: Ingest incoming CAN frames via background TWAI daemon and update global UI data models atomically.
- **REQ-DSP-003 (Audible & Visual Warnings)**: Trigger high-priority warning indicators and audible buzzer alarms on coolant overtemperature, low oil pressure, or critical battery voltage.
- **REQ-DSP-004 (Startup Animation)**: Execute synchronized gauge sweep and indicator self-test animation upon ignition power-on.

### 2.3 Factory Calibration & Web Provisioning
- **REQ-FAC-001 (WiFi AP & Web Portal)**: Launch captive Wi-Fi AP and HTTP server with mDNS service discovery (`itf-factory.local`, `ldb-factory.local`, `rdb-factory.local`).
- **REQ-FAC-002 (NVS Calibration Management)**: Read, validate, and persist calibration constants (pulses/km, fuel resistance curve, temperature lookup, gear ratios) into NVS.
- **REQ-FAC-003 (Safe Power Sequencing)**: Gracefully de-energize 5V peripheral stages prior to software reboot or OTA partition activation.

### 2.4 Vehicle Emulator Suite
- **REQ-EMU-001 (High-Precision Signal Generation)**: Generate pulse frequencies for Speed and RPM using hardware MCPWM generators with inverted low-side driving polarity.
- **REQ-EMU-002 (Fuel Sender Resistance Simulation)**: Replicate 19-step variable resistance sender via discrete switched resistor network.
- **REQ-EMU-003 (Digital Indicator Expander)**: Control discrete vehicle warning telltales via TCA9555 I2C GPIO expander.
- **REQ-EMU-004 (ESPHome & Node-RED Control)**: Provide native Home Assistant / ESPHome web dashboard and Node-RED drivecycle replayer integration.

### 2.5 Firmware Over-The-Air (OTA) & Web Provisioning
- **REQ-OTA-001 (Web Multipart Firmware Upload)**: Enable browser-based multipart binary upload in Factory Apps to flash non-active OTA partition (`ota_0` / `ota_1`) via `esp_ota_ops`.
- **REQ-OTA-002 (Image Header & Target Validation)**: Verify ESP32-S3 app descriptor magic bytes, target chip ID, project version, and SHA256 integrity before writing final partition state.
- **REQ-OTA-003 (Dual-Bank Rollback & Failsafe Boot)**: Support dual-slot A/B partition rollback (`esp_ota_mark_app_invalid_rollback`) with automatic fallback to the factory or previous valid slot if the new image fails startup self-test.

### 2.6 OTA-Over-CAN Capabilities & Bus Flashing
- **REQ-OTA-CAN-001 (Diagnostic Flashing Session State)**: Support switching individual or all cluster nodes into dedicated OTA state (`ITF_SM_ST`, `LDB_SM_ST`, `RDB_SM_ST` = `5 "OTA"`) over CAN, suppressing standard periodic telemetry.
- **REQ-OTA-CAN-002 (Segmented Block Transfer Protocol)**: Stream firmware image blocks across the 500 kbps TWAI bus using segmented transport addressing, block sequence numbering, and CRC checksum validation.
- **REQ-OTA-CAN-003 (Non-Blocking Flash Ingestion)**: Buffer incoming CAN chunks into RAM rings and flash asynchronously in page blocks without triggering task watchdog timers (TWDT) or dropping bus frames.
- **REQ-OTA-CAN-004 (End-of-Transfer Activation & Node Reboot)**: Provide end-of-transfer validation handshake, setting active boot partition (`esp_ota_set_boot_partition`), and synchronized bus reset sequence.

---

## 3. Implementation Traceability Matrix

| Requirement ID | Subsystem | Source Code / Configuration Reference | Verification Method |
| :--- | :--- | :--- | :--- |
| **REQ-ITF-001** | Interface Board | [`interface_board/main/mcpwm_capture_helpers.h`](../interface_board/main/mcpwm_capture_helpers.h) | Bench pulse sweep (0–250 km/h) |
| **REQ-ITF-002** | Interface Board | [`interface_board/main/mcpwm_capture_helpers.h`](../interface_board/main/mcpwm_capture_helpers.h) | Bench frequency generator (0–8000 RPM) |
| **REQ-ITF-003** | Interface Board | [`interface_board/main/adc_processor.h`](../interface_board/main/adc_processor.h) | Voltage sweep across ADS1115 channels |
| **REQ-ITF-004** | Interface Board | [`interface_board/components/sma_filter`](../interface_board/components/sma_filter) | Step response noise rejection test |
| **REQ-ITF-005** | Interface Board | [`interface_board/main/twai_ops.hpp`](../interface_board/main/twai_ops.hpp), [`common/binocan`](../common/binocan) | CAN bus analyzer frame rate audit |
| **REQ-ITF-006** | Interface Board | [`interface_board/components/odometer`](../interface_board/components/odometer), [`interface_board/components/nvs_storage`](../interface_board/components/nvs_storage) | Power cycle endurance & pulse accumulation |
| **REQ-DSP-001** | Displays | [`left_screen/main/main.cpp`](../left_screen/main/main.cpp), [`common/lvgl_v9_port`](../common/lvgl_v9_port) | LVGL frame rate counter / screen refresh |
| **REQ-DSP-002** | Displays | [`common/twai_daemon`](../common/twai_daemon), [`left_screen/main/updateUI.hpp`](../left_screen/main/updateUI.hpp) | Frame reception & decoding benchmark |
| **REQ-DSP-003** | Displays | [`left_screen/main/main.cpp`](../left_screen/main/main.cpp), [`right_screen/main/main.cpp`](../right_screen/main/main.cpp) | Simulated over-temperature signal test |
| **REQ-DSP-004** | Displays | [`left_screen/main/start_animation.hpp`](../left_screen/main/start_animation.hpp) | Visual ignition boot inspection |
| **REQ-FAC-001** | Factory Apps | [`factory apps/ITF factory app/main/main.cpp`](../factory%20apps/ITF%20factory%20app/main/main.cpp) | Browser access & mDNS resolution |
| **REQ-FAC-002** | Factory Apps | [`factory apps/ITF factory app/main/main.cpp`](../factory%20apps/ITF%20factory%20app/main/main.cpp), [`index.html`](../factory%20apps/ITF%20factory%20app/main/web/index.html) | Form validation and NVS readback |
| **REQ-FAC-003** | Factory Apps | [`factory apps/ITF factory app/main/main.cpp`](../factory%20apps/ITF%20factory%20app/main/main.cpp) | Oscilloscope power rail decay check |
| **REQ-EMU-001** | Emulator | [`emulator-console/main/pwm_gen_helpers.h`](../emulator-console/main/pwm_gen_helpers.h), [`esphome/custom_mcpwm.h`](../emulator-console/esphome/custom_mcpwm.h) | Frequency counter & duty cycle verification |
| **REQ-EMU-002** | Emulator | [`emulator-console/esphome/resistor_ladder.h`](../emulator-console/esphome/resistor_ladder.h) | Digital multimeter resistance audit |
| **REQ-EMU-003** | Emulator | [`emulator-console/main/gpio_exp_helper.h`](../emulator-console/main/gpio_exp_helper.h) | LED indicator state inspection |
| **REQ-EMU-004** | Emulator | [`emulator-console/esphome/binocle-emulator.yaml`](../emulator-console/esphome/binocle-emulator.yaml), [`flows.json`](../emulator-console/node-red%20flow/flows.json) | Home Assistant & Node-RED live replayer run |
| **REQ-OTA-001** | Factory Apps | [`factory apps/left display factory app/main/main.cpp`](../factory%20apps/left%20display%20factory%20app/main/main.cpp), [`esp_ota_ops.h`](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/ota.html) | Web browser `.bin` upload & flash validation |
| **REQ-OTA-002** | Factory Apps / Core | [`bennu/main/main.cpp`](../bennu/main/main.cpp), [`esp_ota_ops.h`](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/ota.html) | Bad header & wrong chip image rejection |
| **REQ-OTA-003** | Core Firmware | [`bennu/partitionTable.csv`](../bennu/partitionTable.csv), [`esp_ota_ops.h`](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/ota.html) | Boot failure rollback test to previous slot |
| **REQ-OTA-CAN-001** | Protocol / CAN | [`common/binocan/binocan.dbc`](../common/binocan/binocan.dbc), [`common/binocan/src/binocan.h`](../common/binocan/src/binocan.h) | Diagnostic state request and mode transition |
| **REQ-OTA-CAN-002** | CAN Transceiver / Ops | [`common/twai_daemon`](../common/twai_daemon), [`interface_board/main/twai_ops.hpp`](../interface_board/main/twai_ops.hpp) | Segmented chunk streaming & CRC verification |
| **REQ-OTA-CAN-003** | Core / Flash | [`esp_ota_ops.h`](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/ota.html) | RAM ring buffer to flash throughput benchmark |
| **REQ-OTA-CAN-004** | Core / Reboot | [`esp_ota_ops.h`](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/ota.html) | Post-flash partition switch & reboot test |
