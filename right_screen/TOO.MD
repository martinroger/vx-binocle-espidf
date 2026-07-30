# Right Screen Cluster Firmware - Theory of Operation (`TOO.MD`)

The **Right Screen Cluster Firmware** drives the secondary instrument display. It initializes the ST7701 480x480 RGB display panel, mounts LVGL v9, receives CAN status and auxiliary telemetry from the interface board, provides interactive setting sliders (Light/Dark mode brightness controls), and persists preferences to NVS flash.

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
 |                        Right Screen Firmware (ESP32-S3)                           |
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
 |  |    Renders settings sliders, auxiliary telemetry, and diagnostic views      |  |
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
3. **Interactive Brightness Tuning**:
   - `action_test_brightness()` adjusts backlight duty cycle dynamically while dragging sliders (`dark_slider`, `light_slider`).
   - `action_save_brightness()` writes validated duty cycle values to default NVS storage (`dark_bg`, `light_bg` keys).

---

## 3. CAN Telemetry Ingestion & Dispatch

1. **TWAI Driver Initialization**: Initialized during startup via `initCAN()`.
2. **Route Registration**: Registers filter rules with `twai_daemon_register_route()` to route relevant CAN IDs into a dedicated queue.
3. **Dispatch Task (`CAN_Dispatch_Task`)**:
   - Blocks on queue reception.
   - Decodes incoming frames using `binocan` decoder functions (`binocan_itf_fast_metrics_unpack`, `binocan_itf_slow_metrics_unpack`, `binocan_itf_board_st_unpack`).
   - Updates global metrics (`display_board_st.aux_fuel`, `.system_voltage`, `.ambient_temp`, `.board_st`).

---

## 4. UI Rendering & Theme Management

- **EEZ Studio UI Layout**: Generated UI components reside under `components/ui`.
- **UI Update Loop**: `updateUI.hpp` periodically refreshes auxiliary metrics, diagnostic screens, and interactive settings sliders.
- **Factory Boot Action**: Includes `action_reboot_factory()` to switch boot partitions and restart into the factory diagnostic application.

---

## 5. Compile-Time Macro Differentiation (`CONFIG_RIGHT_SIDE_DISPLAY`)

Per Rule 6, code parity is maintained between `left_screen` and `right_screen`. When compiled with `CONFIG_RIGHT_SIDE_DISPLAY=y`:
- Brightness adjustment actions (`action_test_brightness`, `action_save_brightness`) are compiled into the binary.
- Right-screen specific layout widgets and sliders are rendered.
