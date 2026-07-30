# Interface Board Firmware (`interface_board`)

`interface_board` is the central IO controller firmware driving sensor data acquisition, power rail management, and real-time CAN bus telemetry broadcasting for the digital gauge cluster system.

---

## Technical Overview

- **Target SoC**: ESP32-S3 (16MB Flash, Octal PSRAM)
- **Target ESP-IDF Version**: ESP-IDF v6.2.0 (`eim run "<idf.py command>" master`)
- **CAN Bus Driver**: `common/twai_daemon` (500 kbps bitrate, ISR-driven router)
- **Message Codec**: `common/binocan` (DBC-derived C++ CAN frame encoder)

---

## Key Hardware Functions

- **Power Management**: Controls 5V main and 5V AUX supply channels (`init_5V_ctrl`).
- **Analog Sensor Acquisition**: Reads coolant temperature, primary/auxiliary fuel levels, and battery voltage via ADC1/ADC2 with Simple Moving Average (`sma_filter`) noise filtering.
- **Pulse Capture**: Measures speed sensor frequency and tachometer RPM via ESP32-S3 MCPWM capture timers.
- **I2C Expansion & Discrete Inputs**: Reads vehicle discrete signals and light switches via TCA9555 IO expander.
- **Persistent Mileage Tracking**: Maintains total odometer and trip metrics in dedicated NVS flash (`nvs_odo`) with wear leveling and rollback protection.
- **Periodic CAN Publishing**: Runs FreeRTOS tasks to broadcast telemetry, odometer, board status, and diagnostic messages.

---

## Documentation

For a detailed architectural breakdown of hardware interfaces, sensor processing pipelines, and FreeRTOS packaging tasks, see [TOO.MD](TOO.MD).
