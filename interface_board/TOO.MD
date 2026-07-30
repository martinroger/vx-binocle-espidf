# Interface Board Firmware - Theory of Operation (`TOO.MD`)

The **Interface Board Firmware** runs on the central ESP32-S3 microcontroller within the gauge cluster system. It manages power rails, acquires high-rate digital and analog vehicle sensor inputs, processes measurements using digital filters, persists mileage metrics to NVS flash, and broadcasts structured sensor frames onto the automotive CAN bus via `twai_daemon`.

---

## 1. System Architecture

```
 +-----------------------------------------------------------------------------------+
 |                              Hardware Sensors & Inputs                            |
 |  +-------------------+   +--------------------+   +----------------------------+  |
 |  | Analog Resistance |   | Speed / RPM Pulse  |   | Switches & Discrete Inputs |  |
 |  | (Coolant, Fuel)   |   | (MCPWM Capture)    |   | (TCA9555 / Active HI/LO)   |  |
 |  +-------------------+   +--------------------+   +----------------------------+  |
 +-----------------------------------------------------------------------------------+
           |                          |                            |
           v                          v                            v
 +-----------------------------------------------------------------------------------+
 |                          ESP32-S3 Interface Firmware                              |
 |                                                                                   |
 |  +-------------------+   +--------------------+   +----------------------------+  |
 |  | ADC + SMA Filter  |   | Speed/RPM Calculator|  | Input State Aggregator     |  |
 |  +-------------------+   +--------------------+   +----------------------------+  |
 |                            \        |        /                                    |
 |                             v       v       v                                     |
 |  +-----------------------------------------------------------------------------+  |
 |  |                FreeRTOS CAN Packaging Tasks (twai_ops.hpp)                  |  |
 |  |  - Slow Metrics PKG (100ms)               - Odometer PKG (1000ms)           |  |
 |  |  - Fast Metrics PKG (20ms)                - Board Status / Version PKG       |  |
 |  +-----------------------------------------------------------------------------+  |
 +-----------------------------------------------------------------------------------+
                                          |
                        [twai_daemon_transmit(&tx_msg, 1ms)]
                                          v
 +-----------------------------------------------------------------------------------+
 |                             TWAI Daemon CAN Router                                |
 +-----------------------------------------------------------------------------------+
                                          |
                                    [CAN Bus 500kbps]
                                          v
 +-----------------------------------------------------------------------------------+
 |                        Left & Right Cluster Display Nodes                         |
 +-----------------------------------------------------------------------------------+
```

---

## 2. Hardware Subsystems & Signal Processing

### 2.1 Power Control
- **5V Main & 5V AUX Rails**: Initialized during startup via `init_5V_ctrl()`, `enable_5V()`, and `enable_5V_AUX()`.
- **Fault Recovery / Rollback**: If power rail initialization or NVS mounting fails, the system executes `attemptRollBack()`, triggering an OTA rollback to the previous valid app partition and rebooting.

### 2.2 Analog Sensor Acquisition (ADC + SMA Filtering)
- **Inputs**: Coolant temperature sensor, primary fuel level, auxiliary fuel level, system battery voltage.
- **Processing**: Sampled via ESP32 ADC1/ADC2 and processed through a Simple Moving Average (`sma_filter`) to remove electrical noise before encoding into CAN messages.

### 2.3 Frequency Pulse Capture (Speed & RPM)
- **Hardware Peripherals**: ESP32-S3 MCPWM capture timers (`mcpwm_capture_helpers.h`).
- **Processing**: Measures pulse period and frequency from speed sensor and tachometer inputs, converting raw frequency data into vehicle speed ($km/h$) and engine speed ($RPM$).

### 2.4 Discrete Input Processing & I2C Expander
- **TCA9555 Expander**: Interfaced over I2C to expand discrete input channels (`tca9555_helpers.h`).
- **Active HI/LOW Processors**: Classifies discrete input states (indicator signals, lights, fault switches) with debouncing (`active_hi_low_processor.h`).

---

## 3. Persistent Odometer & Flash Wear Leveling

- **NVS Partition (`nvs_odo`)**: Dedicated NVS partition initialized via `nvs_init_odo_flash()`.
- **Metrics**: Persists total odometer distance (`odometer_m`) and trip meter (`trip_m`).
- **Wear Leveling & Validation**: Written periodically or on shutdown; corrupted sectors trigger fallback rollback sequence to preserve mileage integrity.

---

## 4. Real-Time CAN Broadcasting (`twai_ops.hpp`)

The firmware delegates all CAN transmission to `twai_daemon`. Dedicated FreeRTOS packaging tasks format sensor values into standard `binocan` protocol frames:

| Task Name | Cycle Time | Data Published |
| :--- | :--- | :--- |
| `itf_fast_metrics_PKG` | 20 ms | Speed, RPM, gear/discrete active flags |
| `itf_slow_metrics_PKG` | 100 ms | Coolant temperature, fuel levels, battery voltage, ambient temp |
| `itf_odometer_PKG` | 1000 ms | Accumulated odometer distance and trip meter |
| `itf_board_st_PKG` | 1000 ms | Hardware error status, supply rail metrics, error counters |
| `itf_board_version_PKG`| 5000 ms | Firmware version tag, git commit hash, build timestamp |

Each task executes in a loop, taking FreeRTOS task notifications (`ulTaskNotifyTake`), encoding metrics into `twai_message_t` buffers using `binocan_*_pack()`, and calling:
```cpp
twai_daemon_transmit(&tx_msg, pdMS_TO_TICKS(1));
```
