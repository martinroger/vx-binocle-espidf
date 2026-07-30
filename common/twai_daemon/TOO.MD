# TWAI Daemon Component - Theory of Operation (`TOO.MD`)

The `twai_daemon` component provides a lightweight, ISR-driven CAN (Two-Wire Automotive Interface) driver and frame routing engine for ESP-IDF v6+ applications.

---

## 1. Overview & Architecture

Unlike traditional CAN implementations that poll for incoming frames in a background task, `twai_daemon` operates entirely via an **Interrupt Service Routine (ISR) Callback Architecture**. 

```
 +-----------------------------------------------------------------------+
 |                         ESP-IDF TWAI Peripheral                       |
 +-----------------------------------------------------------------------+
                                     |
                          [Frame Received Interrupt]
                                     v
 +-----------------------------------------------------------------------+
 |                     ISR Callback: on_rx_done()                        |
 |                                                                       |
 |  Iterates through route_table[32]:                                   |
 |    IF (rx_id & route.mask) == (route.id & route.mask) THEN            |
 |        xQueueSendFromISR(route.target_queue, &rx_message, &woken)     |
 +-----------------------------------------------------------------------+
             |                                       |
             v                                       v
    +------------------+                    +------------------+
    | FreeRTOS Queue A |                    | FreeRTOS Queue B |
    | (e.g. Telemetry) |                    | (e.g. UDS OTA)   |
    +------------------+                    +------------------+
             |                                       |
             v                                       v
    +------------------+                    +------------------+
    | Task / Handler A |                    | Task / Handler B |
    +------------------+                    +------------------+
```

### Key Architectural Advantages
- **Zero Polling Overhead**: CPU cycles are consumed only when valid CAN frames arrive.
- **Selective Subscriptions**: Tasks subscribe only to the message IDs they care about. Unmatched frames are discarded immediately in the ISR with zero queue allocations.
- **Multi-Subscriber Support**: A single incoming frame can be routed to multiple independent queues if their filters match.

---

## 2. Component Initialization (`initCAN`)

The `initCAN()` function configures and enables the ESP-IDF TWAI hardware node (`esp_driver_twai`):

1. **Pin Assignment**: Configures TX (`CONFIG_CAN_TX`, default GPIO 2) and RX (`CONFIG_CAN_RX`, default GPIO 3) pins using `twai_onchip_node_config_t`.
2. **Timing Configuration**: Configures standard 500 kbps bitrate (`TWAI_CLK_SRC_DEFAULT`).
3. **Queue Sizing**: Allocates a 256-message TX hardware queue.
4. **ISR Callback Registration**: Registers `on_rx_done` as the RX event callback via `twai_node_register_event_callbacks()`.
5. **Node Activation**: Calls `twai_node_enable()` to place the peripheral into operational mode.

---

## 3. Dynamic Route Registry

The route registry maintains up to 32 active subscriptions (`MAX_CAN_ROUTES`):

```cpp
struct can_route_t {
    uint32_t id;           // Base CAN ID to match
    uint32_t mask;         // Bitmask (1 = care, 0 = don't care)
    QueueHandle_t target_queue; // Receiver FreeRTOS queue
    bool active;           // Slot active status
};
```

### API Functions
- **`twai_daemon_register_route(id, mask, queue)`**: Searches for an inactive slot in `route_table`, populates the filter criteria and target queue handle, and marks `.active = true`.
- **`twai_daemon_unregister_route(queue)`**: Deactivates all entries matching the given `queue` handle, preventing subsequent ISR pushes.

---

## 4. Transmission Pipeline (`twai_daemon_transmit`)

Sending CAN messages is non-blocking or blocking based on `ticks_to_wait`:

1. Translates `twai_message_t` into native `twai_frame_t`.
2. Passes payload pointer and header attributes (ID, DLC, IDE, RTR) directly to `twai_node_transmit(can_node_hdl, &tx_frame, ticks_to_wait)`.
3. Handles driver hardware queue saturation safely by returning `ESP_ERR_TIMEOUT` if the queue remains full after `ticks_to_wait`.

---

## 5. Thread & ISR Safety

- **Read-Only Inspection in ISR**: The `on_rx_done` callback reads the `route_table` array. Since registration typically occurs during initialization or controlled setup phases, non-blocking array inspection inside the ISR ensures minimal context switch latency.
- **Yield Signaling**: If `xQueueSendFromISR` sets `woken = pdTRUE`, `on_rx_done` returns `true`, triggering immediate FreeRTOS context switching to higher-priority receiving tasks.
