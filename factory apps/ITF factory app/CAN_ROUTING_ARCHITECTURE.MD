# ISR-Based CAN Routing Architecture

This document details an alternative, modern architecture for handling CAN frame dispatch operations on the ESP32 using the ESP-IDF v6.0 TWAI driver. 

## The Core Concept

In the previous architecture, an intermediate FreeRTOS task (`CAN_RX_Task`) acted as a monolithic broker. It popped every message from the driver's queue and passed it to a single `dispatchCANFrame` function. While this kept `main.cpp` lightweight, it forced all logic through a single bottleneck and required complex task suspensions during OTA updates.

**The proposed architecture moves the routing logic directly into the ISR (`on_rx_done`).** Instead of a single dispatcher function, the `twai_daemon` maintains a lightweight **Route Registry**. Independent modules (like Telemetry, OTA, or specific ECU controllers) register a FreeRTOS Queue and an ID filter with the daemon. When a frame arrives, the ISR peeks at the ID, checks the registry, and pushes the frame directly to the subscribed queue.

## Key Advantages

1. **Decentralized and Lightweight:** You preserve the advantage of keeping `main.cpp` lightweight. In fact, it becomes *more* modular. There is no longer a massive `switch(id)` statement in a single dispatcher file. Different files/modules just register their own queues for the IDs they care about.
2. **Eliminates `CAN_RX_Task`:** Saves 4KB of task memory and reduces context-switching overhead. The hardware interrupt directly wakes up the final target task.
3. **No Task Suspension:** OTA processes no longer need to suspend background tasks. They simply register a temporary route for UDS response IDs.

---

## 1. Modifying `twai_daemon.cpp` (The Router)

The `twai_daemon` is rewritten to drop the `CAN_RX_Task` and implement a routing table.

### The Route Registry
```cpp
#define MAX_CAN_ROUTES 10

struct can_route_t {
    uint32_t id;
    uint32_t mask;
    QueueHandle_t target_queue;
    bool active;
};

static can_route_t route_table[MAX_CAN_ROUTES];
```

### The Registration Functions
These functions allow external modules to subscribe and unsubscribe dynamically.
```cpp
esp_err_t twai_daemon_register_route(uint32_t id, uint32_t mask, QueueHandle_t queue) {
    // (Add thread-safety/mutex if registering outside of init phase)
    for (int i = 0; i < MAX_CAN_ROUTES; i++) {
        if (!route_table[i].active) {
            route_table[i].id = id;
            route_table[i].mask = mask;
            route_table[i].target_queue = queue;
            route_table[i].active = true;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

void twai_daemon_unregister_route(QueueHandle_t queue) {
    for (int i = 0; i < MAX_CAN_ROUTES; i++) {
        if (route_table[i].target_queue == queue) {
            route_table[i].active = false;
        }
    }
}
```

### The ISR Router
The `on_rx_done` callback is where the magic happens. It executes in microseconds.
```cpp
static bool on_rx_done(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx) {
    twai_frame_t rx_frame;
    bool need_yield = false;
    
    // Drain the hardware FIFO
    while (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
        BaseType_t woken = pdFALSE;
        
        // Find the matching route
        for (int i = 0; i < MAX_CAN_ROUTES; i++) {
            if (route_table[i].active && 
               (rx_frame.header.id & route_table[i].mask) == route_table[i].id) {
                
                xQueueSendFromISR(route_table[i].target_queue, &rx_frame, &woken);
                break; // Stop at first match
            }
        }
        
        if (woken == pdTRUE) {
            need_yield = true;
        }
    }
    return need_yield;
}
```

---

## 2. Modifying Modules (e.g., `binocan.cpp`)

Previously, `binocan.cpp` probably contained the monolithic `dispatchCANFrame` implementation. Now, it acts as an independent task.

```cpp
// Inside binocan.cpp or telemetry_handler.cpp

void Telemetry_Task(void *pvParams) {
    QueueHandle_t telemetry_queue = xQueueCreate(50, sizeof(twai_frame_t));
    
    // Subscribe to all standard IDs (Mask 0x000 means "ignore ID differences, capture all")
    // Note: In reality, you'd set a specific mask to avoid stealing OTA frames.
    twai_daemon_register_route(0x000, 0x000, telemetry_queue); 
    
    twai_frame_t rx_frame;
    while (true) {
        // Block indefinitely until the ISR delivers a relevant frame
        if (xQueueReceive(telemetry_queue, &rx_frame, portMAX_DELAY) == pdPASS) {
            // Process normal telemetry frame
            process_telemetry_frame(&rx_frame);
        }
    }
}
```

---

## 3. Modifying OTA Update Logic (`main.cpp`)

The OTA update logic becomes incredibly clean. It completely drops the macro hacks, `vTaskSuspend` calls, and dispatcher hijacking. 

During an OTA update, `main.cpp` simply creates a temporary queue, registers it for the exact UDS response ID it expects, and waits.

```cpp
// Inside flash_post_handler in main.cpp

// 1. Create a temporary queue for OTA frames
QueueHandle_t ota_rx_queue = xQueueCreate(20, sizeof(twai_frame_t));

// 2. Register route for EXACTLY the UDS Response ID.
// Mask 0xFFFFFFFF means "match the ID exactly"
twai_daemon_register_route(UDSRespID, 0xFFFFFFFF, ota_rx_queue);

// ... Send First Frame ...

// 3. Wait for Flow Control (FC) frame directly from the queue
twai_frame_t rx_frame;
if (xQueueReceive(ota_rx_queue, &rx_frame, pdMS_TO_TICKS(2000)) == pdPASS) {
    if (rx_frame.data[0] == 0x30) {
        // Handle FC CTS
    }
} else {
    // Handle Timeout
}

// ... Finish OTA ...

// 4. Cleanup
twai_daemon_unregister_route(ota_rx_queue);
vQueueDelete(ota_rx_queue);
```

## Summary
By using the hardware ISR to route messages based on ID masks, your codebase becomes highly decentralized. New features or protocols can be added in completely isolated `.cpp` files without ever touching `main.cpp` or a central dispatcher header again.
