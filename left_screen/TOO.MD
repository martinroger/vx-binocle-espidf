# Left Screen Cluster Firmware - Theory of Operation (`TOO.MD`)

The **Left Screen Cluster Firmware** drives the primary instrument gauge display. It initializes the ST7701 480x480 RGB display panel, mounts LVGL v9, receives CAN telemetry messages from the central interface board, updates UI elements in real time, and persists user preferences (brightness, theme) to NVS flash.

---

## 1. Subsystem Architecture

```
 +-----------------------------------------------------------------------------------+
 |                             Interface Board (CAN Bus)                             |
 +-----------------------------------------------------------------------------------+
                                          |
                              [CAN Messages (500 kbps)]
                                          v
 +-----------------------------------------------------------------------------------+
 |                        Left Screen Firmware (ESP32-S3)                            |
 |                                                                                   |
 |  +-----------------------------------------------------------------------------+  |
 |  |                          TWAI Daemon ISR Router                             |  |
 |  |    Pushes matched CAN frames to CAN_Dispatch_Task_hdl queue                 |  |
 |  +-----------------------------------------------------------------------------+  |
 |                                        |                                          |
 |                                        v                                          |
 |  +-----------------------------------------------------------------------------+  |
 |  |                       CAN Dispatch Task (twai_ops.hpp)                      |  |
 |  |    Decodes Binocan signals & updates global display_board_st variables      |  |
 |  +-----------------------------------------------------------------------------+  |
 |                                        |                                          |
 |                                        v                                          |
 |  +-----------------------------------------------------------------------------+  |
 |  |                        LVGL v9 Task / UI Update Loop                        |  |
 |  |    Locks LVGL, updates gauge arcs, labels, needles, and indicators          |  |
 |  +-----------------------------------------------------------------------------+  |
 +-----------------------------------------------------------------------------------+
                                          |
                                          v
 +-----------------------------------------------------------------------------------+
 |                    ST7701 4.0" RGB Display (esp32_display_panel)                  |
 +-----------------------------------------------------------------------------------+
```

---

## 2. Display Hardware & LVGL v9 Porting

1. **Board & Panel Driver**: Uses `esp32_display_panel` (`Board` & `LCD` classes) to configure the ST7701 3-wire SPI + RGB interface (480x480 resolution, 16-bit 565 color depth, PSRAM frame buffers).
2. **LVGL v9 Integration**: Initialized via `common/lvgl_v9_port`. The porting task manages display flush callbacks, double-buffered frame updates, and thread synchronization using `lvgl_port_lock()` / `lvgl_port_unlock()`.
3. **Brightness Control**: Backlight PWM brightness is controlled via `display_board_st.backLight->setBrightness(level)` and persisted to default NVS storage (`dark_bg`, `light_bg` keys).

---

## 3. CAN Telemetry Ingestion & Dispatch

1. **TWAI Driver Initialization**: Initialized during startup via `initCAN()`.
2. **Route Registration**: Registers filter rules with `twai_daemon_register_route()` to route relevant CAN IDs into a dedicated queue.
3. **Dispatch Task (`CAN_Dispatch_Task`)**:
   - Blocks on queue reception.
   - Decodes incoming frames using `binocan` decoder functions (`binocan_itf_fast_metrics_unpack`, `binocan_itf_slow_metrics_unpack`, `binocan_itf_odometer_unpack`).
   - Updates global metrics (`display_board_st.speed`, `.rpm`, `.coolant_temp`, `.fuel_level`, `.odometer_m`, `.trip_m`).

---

## 4. UI Rendering & Theme Management

- **EEZ Studio UI Layout**: Generated UI components reside under `components/ui`.
- **UI Update Loop**: `updateUI.hpp` periodically checks metric updates and applies smooth value interpolation to speedometers, tachometers, temperature gauges, warning icons, and digital readouts.
- **Theme Switching**: Supports Light Mode and Dark Mode themes (`theme.hpp`), toggled dynamically based on ambient sensor thresholds or user settings.

---

## 5. Compile-Time Macro Differentiation (`CONFIG_LEFT_SIDE_DISPLAY`)

Per Rule 6, code parity is maintained between `left_screen` and `right_screen`. When compiled with `CONFIG_LEFT_SIDE_DISPLAY=y`:
- Primary gauge cluster screens (Speedometer, Tachometer, Primary Fuel, Coolant) are activated as main views.
- Right-screen specific controls (e.g. secondary setting sliders) are conditionally excluded via `#ifdef CONFIG_RIGHT_SIDE_DISPLAY`.
