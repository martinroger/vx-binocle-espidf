# CAN OTA Update Logic & Handshake Sequence (UDS ISO-TP)

This document details the over-the-air (OTA) logic implemented in [`main/main.cpp`](main/main.cpp) (specifically within `flash_post_handler`), which bridges HTTP firmware chunk uploads to a target CAN bus node using the UDS (Unified Diagnostic Services) protocol over ISO 15765-2 (ISO-TP).

---

## 1. Overview

The ESP32-S3 acts as an HTTP server receiving a firmware file via chunked `POST /flash` request. Instead of updating itself, it streams the firmware chunks over the CAN bus to a target node (such as a left or right cluster display).

---

## 2. TWAI Daemon Integration (`ota_twai` Helper API)

During the OTA transfer, the HTTP handler dynamically registers a route with `twai_daemon`:

1. **`ota_twai_start_listening()`**: Creates/resets `ota_rx_queue` and registers a route filter for `UDS_REQ_FRAME_ID` (`0x7E0`) via `twai_daemon_register_route`.
2. **ISR Ingestion**: The `twai_daemon` ISR (`on_rx_done`) pushes matching response frames directly to `ota_rx_queue`.
3. **Synchronous Polling**: The OTA handler polls responses via `ota_twai_receive(msg, ticks)`.
4. **`ota_twai_stop_listening()`**: Unregisters the route from `twai_daemon` upon completion or error abort, restoring standard background CAN routing.

---

## 3. ISO-TP Handshake & Transfer Protocol

```
 +------------------------------------------------------------------------------------+
 | 1. First Frame (FF) Transmitted (0x10 + Total File Size + 2 Bytes Data)            |
 +------------------------------------------------------------------------------------+
                                          |
                              [Wait for Flow Control]
                                          v
 +------------------------------------------------------------------------------------+
 | 2. Flow Control (FC) Received (0x30 CTS, BlockSize, STmin_MS)                      |
 +------------------------------------------------------------------------------------+
                                          |
                          [Ship Consecutive Frames Loop]
                                          v
 +------------------------------------------------------------------------------------+
 | 3. Consecutive Frames (CF) (0x20 + SN 1..F + Payload)                             |
 |    - Delay STmin_MS between frames                                                |
 |    - Pause & wait for FC if BlockSize limit is reached                            |
 +------------------------------------------------------------------------------------+
                                          |
                              [All Chunks Transmitted]
                                          v
 +------------------------------------------------------------------------------------+
 | 4. Final Status Frame (0x40 + OTA Status Code)                                     |
 +------------------------------------------------------------------------------------+
```

---

## 4. Sequence Diagrams & Escape Points

### 4.1 Successful UDS Handshake & Transfer Sequence

```mermaid
sequenceDiagram
    autonumber
    actor Client as HTTP Web Client
    participant HTTP as HTTP Server (flash_post_handler)
    participant Queue as ota_rx_queue
    participant TWAI as TWAI Daemon Router
    participant ECU as Target ECU Node

    Client->>HTTP: POST /flash (Chunked Firmware Payload)
    HTTP->>HTTP: Validate ESP Image Magic Word (0xABCD5432)
    HTTP->>TWAI: ota_twai_start_listening()<br/>(Register UDS route)
    
    Note over HTTP,ECU: Handshake Step 1: First Frame (FF)
    HTTP->>TWAI: twai_daemon_transmit(FF 0x10, File Size)
    TWAI->>ECU: CAN Frame (0x10, Size, Data[0..1])
    
    Note over HTTP,ECU: Handshake Step 2: Flow Control (FC) Wait
    ECU-->>TWAI: CAN Frame (0x30 CTS, BlockSize, STmin)
    TWAI-->>Queue: Post to ota_rx_queue from ISR
    HTTP->>Queue: ota_twai_receive(2000ms Timeout)
    Queue-->>HTTP: FC CTS Frame (BlockSize, STmin_MS)
    
    Note over HTTP,ECU: Handshake Step 3: Consecutive Frames (CF) Shipping Loop
    loop Every 7 Bytes of Payload
        HTTP->>HTTP: Delay for STmin_MS
        HTTP->>TWAI: twai_daemon_transmit(CF 0x20 + SN)
        TWAI->>ECU: CAN Frame (CF Data Payload)
        opt BlockSize Limit Reached
            ECU-->>TWAI: CAN Frame (0x30 CTS)
            TWAI-->>Queue: Post to ota_rx_queue
            HTTP->>Queue: ota_twai_receive()
        end
    end

    Note over HTTP,ECU: Handshake Step 4: Final Status Verification
    ECU-->>TWAI: CAN Frame (0x40 Status Code 0x00)
    TWAI-->>Queue: Post to ota_rx_queue
    HTTP->>Queue: ota_twai_receive(5000ms Timeout)
    Queue-->>HTTP: Status Frame (Status 0x00 OK)
    
    HTTP->>TWAI: ota_twai_stop_listening()
    HTTP-->>Client: 200 OK JSON {"status":"0", "version":...}
```

---

### 4.2 Escape Points & Error / Timeout Sequences

The OTA handler enforces explicit escape points at every step to prevent hanging or memory leaks. In all failure cases, `ota_twai_stop_listening()` is invoked to restore the `twai_daemon`.

```mermaid
sequenceDiagram
    autonumber
    actor Client as HTTP Web Client
    participant HTTP as HTTP Server (flash_post_handler)
    participant TWAI as TWAI Daemon Router
    participant ECU as Target ECU Node

    alt Escape Point 1: Invalid ESP Image Header
        Client->>HTTP: POST /flash (Corrupt Payload)
        Note over HTTP: Scan for 0xABCD5432 Fails
        HTTP-->>Client: 400 Bad Request ("No valid image header in file")
        
    else Escape Point 2: First Frame (FF) Transmission Failure
        HTTP->>TWAI: twai_daemon_transmit(FF)
        TWAI--xECU: [Bus Error / TX Hardware Full]
        Note over HTTP: TX Return != ESP_OK
        HTTP->>TWAI: ota_twai_stop_listening()
        HTTP-->>Client: 500 Internal Server Error ("FF could not be transmitted")
        
    else Escape Point 3: Flow Control (FC) Timeout / No Response
        HTTP->>TWAI: twai_daemon_transmit(FF)
        TWAI->>ECU: CAN Frame (FF)
        Note over HTTP: Waiting for FC Response...
        ECU--xTWAI: [Unresponsive / Disconnected Node]
        Note over HTTP: ota_twai_receive() Timeout / OTA_TO_timer Expired
        HTTP->>TWAI: ota_twai_stop_listening()
        HTTP-->>Client: 500 Internal Server Error ("FC was not received within timeout")
        
    else Escape Point 4: Target Status Error / NACK Frame
        HTTP->>TWAI: twai_daemon_transmit(FF)
        TWAI->>ECU: CAN Frame (FF)
        ECU-->>TWAI: CAN Frame (0x40 Status Error Code 0x82)
        TWAI-->>HTTP: ota_rx_queue (Status 0x82 Error)
        Note over HTTP: Non-Zero Status Frame Detected
        HTTP->>TWAI: ota_twai_stop_listening()
        HTTP-->>Client: 500 Internal Server Error ("Status frame code: 0x82")
        
    else Escape Point 5: Unmatched Message Overflow (500 Frames)
        HTTP->>TWAI: ota_twai_receive()
        TWAI-->>HTTP: >500 Unmatched Bus Frames Received
        Note over HTTP: otherFrames >= 500 Counter Limit
        HTTP->>TWAI: ota_twai_stop_listening()
        HTTP-->>Client: 500 Internal Server Error ("FC was not received in 500 frames")

    else Escape Point 6: Consecutive Frame (CF) Transmission Failure
        HTTP->>TWAI: twai_daemon_transmit(CF)
        TWAI--xECU: [Bus Disconnected Mid-Transfer]
        Note over HTTP: TX Return != ESP_OK
        HTTP->>TWAI: ota_twai_stop_listening()
        HTTP-->>Client: 500 Internal Server Error ("CF Frame TX error")
    end
```
