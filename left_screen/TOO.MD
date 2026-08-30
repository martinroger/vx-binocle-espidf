# Left Display Board (LDB) — Theory of Operation

## 1. Architecture Overview

The Left Display Board executes a multi-task FreeRTOS architecture:

```mermaid
graph TD
    A[CAN Bus] -->|Hardware ISR| B(TWAI Daemon on_rx_done)
    B -->|Queue Handoff| C(TWAI RX Worker Task - Core 0)
    C -->|dispatchFrame| D[Declarative Route Table]
    D -->|Atomic Data Model Update| E[Global State Model]
    E -->|Render Loop| F(LVGL Task - Core 1)
    F -->|SPI / RGB Buffer Flush| G[LCD Display Panel]
    E -->|Threshold Monitor| H(Alarm & Buzzer Task)
    H -->|LEDC / PWM| I[Buzzer Transducer]
```

---

## 2. UI Pipeline & Font Rendering

- **LVGL v9 Port**: Custom double-buffered rendering pipeline with DMA transfers (`lvgl_v9_port.cpp`).
- **Typography**: Custom-generated White Rabbit pixel fonts (20px, 28px, 32px) for crisp contrast under automotive lighting.
- **Startup Animation**: Ignition-on sequence sweeps the RPM arc to max scale and back, illuminates all warning telltales, and transitions into live mode (`start_animation.hpp`).

---

## 3. CAN Telemetry & Declarative Route Dispatch

- **Declarative Routing Table**: Frame ingestion is orchestrated by `rx_routes[]` in `twai_ops.hpp`, associating Frame IDs (`BINOCAN_ITF_ACTIVE_HI_LO_FRAME_ID`, `BINOCAN_ITF_SLOW_METRICS_FRAME_ID`, etc.) with unpack handlers and watchdog timers.
- **Watchdog Lifecycles**: Incoming frames automatically reset per-signal FreeRTOS software timers. If signal loss occurs (> 5 cycles), callbacks revert cluster variables to safe default failure values.
- **Overtemperature Alarm**: When coolant temperature exceeds the calibrated threshold (default 105°C), the alarm monitor triggers visual flashing on the UI and pulsates the audible buzzer until temperature drops below hysteresis limit (98°C).
