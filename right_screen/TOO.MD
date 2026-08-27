# Right Display Board (RDB) — Theory of Operation

## 1. Architecture Overview

The Right Display Board architecture coordinates telemetry ingestion, state aggregation, and LVGL graphics rendering:

```mermaid
graph TD
    A[CAN Bus] -->|TWAI Transceiver| B(TWAI Daemon Receiver Task - Core 0)
    B -->|Atomic State Update| C[Global Auxiliary State Model]
    C -->|Render Loop| D(LVGL Task - Core 1)
    D -->|DMA Flush| E[LCD Display Panel]
```

---

## 2. Ingestion & Multi-Screen Views

- **Auxiliary Gauges**: Renders fuel quantity (liters / bar graph), low-voltage battery status, and oil pressure.
- **Trip Statistics**: Displays trip distance, average speed, runtime elapsed, and maximum velocity.
- **Diagnostics**: Presents active diagnostic codes and CAN bus communication health metrics.
