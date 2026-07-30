# ITF Factory App Firmware - Theory of Operation (`TOO.MD`)

The **ITF Factory App** is a diagnostic and manufacturing firmware for the ESP32-S3 Interface Board. It provides a local Wi-Fi Access Point/Station web interface to inspect board hardware status, execute self-tests, and perform Over-The-Air (OTA) firmware updates of target display clusters and interface boards via **UDS (Unified Diagnostic Services) over ISO-TP CAN bus**.

---

## 1. Subsystem Architecture

```
 +-----------------------------------------------------------------------------------+
 |                                   Client Browser                                  |
 +-----------------------------------------------------------------------------------+
                                          |
                              [HTTP / REST API / OTA POST]
                                          v
 +-----------------------------------------------------------------------------------+
 |                           ESP32-S3 HTTP Web Server                                |
 |                                                                                   |
 |  +--------------------+   +-----------------------+   +------------------------+  |
 |  |  SPIFFS Asset Host |   |   REST Diagnostic API |   |  UDS ISO-TP OTA Engine |  |
 |  +--------------------+   +-----------------------+   +------------------------+  |
 +-----------------------------------------------------------------------------------+
                                                                     |
                                                           [CAN Messages / ISO-TP]
                                                                     v
                                                         +-----------------------+
                                                         |  TWAI Daemon Router   |
                                                         +-----------------------+
                                                                     |
                                                         +-----------------------+
                                                         | Target ECU Display    |
                                                         +-----------------------+
```

### Components
1. **Wi-Fi & Network Provisioning**: Configures Wi-Fi SoftAP / Station modes and initializes mDNS (`http://itf-factory.local`).
2. **SPIFFS Static Web Hosting**: Mounts `/spiffs` storage containing HTML/JS frontend assets, serving web application requests on HTTP port 80.
3. **Diagnostic REST APIs**: Provides HTTP GET/POST endpoints for status inspection, sensor raw value reading, self-test execution, and NVS configuration.
4. **UDS ISO-TP CAN Flashing Engine**: Bridges HTTP chunked file uploads (`POST /flash`) directly to ISO 15765-2 (ISO-TP) framing over CAN bus.

---

## 2. UDS ISO-TP OTA Flashing Engine

The OTA engine (`flash_post_handler`) receives firmware binaries chunk-by-chunk via HTTP POST request and transmits them over the CAN bus to a target node.

### Route Registration Lifecycle
During OTA flashing, the handler registers a dynamic route with `twai_daemon` to receive UDS responses:
1. **`ota_twai_start_listening()`**: Creates/resets `ota_rx_queue` and calls `twai_daemon_register_route(UDS_REQ_FRAME_ID, 0xFFFFFFFF, ota_rx_queue)`.
2. **Synchronous Frame Reception**: Responses from the target node are pushed by `twai_daemon`'s ISR directly to `ota_rx_queue`. `ota_twai_receive(...)` pulls frames from this queue with a configurable timeout.
3. **`ota_twai_stop_listening()`**: Unregisters the route from `twai_daemon` upon completion or error abort.

---

## 3. Sequence Diagrams

### 3.1 Successful UDS ISO-TP Flashing Sequence

```mermaid
sequenceDiagram
    autonumber
    actor User as Web Client
    participant HTTP as ESP32 HTTP Server
    participant OTA as OTA Flashing Engine
    participant Router as TWAI Daemon
    participant Target as Target ECU Node

    User->>HTTP: POST /flash (Chunked Firmware Payload)
    HTTP->>OTA: Read 1st Chunk (Header)
    Note over OTA: Scan for ESP Magic Word (0xABCD5432)
    OTA->>Router: ota_twai_start_listening()<br/>(Register UDS_REQ_FRAME_ID route)
    
    Note over OTA,Target: Phase 1: First Frame (FF)
    OTA->>Router: twai_daemon_transmit(First Frame 0x10)
    Router->>Target: CAN Frame (0x10, Total File Size)
    
    Note over OTA,Target: Phase 2: Flow Control (FC) Wait
    Target-->>Router: CAN Frame (0x30 CTS, BlockSize, STmin)
    Router-->>OTA: ota_rx_queue (Flow Control CTS)
    
    Note over OTA,Target: Phase 3: Consecutive Frames (CF) Loop
    loop Send Firmware Payload Chunks
        OTA->>Router: twai_daemon_transmit(CF 0x20 + SN)
        Router->>Target: CAN Frame (CF Data Payload)
        Note over OTA: Delay for STmin_MS
    end
    
    OTA->>Router: ota_twai_stop_listening()<br/>(Unregister UDS Route)
    OTA-->>HTTP: Flash Complete OK
    HTTP-->>User: 200 OK (Firmware Flashed Successfully)
```

### 3.2 Error and Timeout Handling Sequence

```mermaid
sequenceDiagram
    autonumber
    actor User as Web Client
    participant HTTP as ESP32 HTTP Server
    participant OTA as OTA Flashing Engine
    participant Router as TWAI Daemon
    participant Target as Target ECU Node

    alt Scenario A: Invalid Firmware Magic Word
        User->>HTTP: POST /flash (Corrupt File)
        HTTP->>OTA: Read 1st Chunk
        Note over OTA: Magic Word Check Fails!
        OTA-->>HTTP: Invalid Image Header Error
        HTTP-->>User: 400 Bad Request
    else Scenario B: Flow Control (FC) Timeout
        User->>HTTP: POST /flash (Valid Firmware)
        HTTP->>OTA: Read 1st Chunk & Validate
        OTA->>Router: ota_twai_start_listening()
        OTA->>Router: twai_daemon_transmit(First Frame 0x10)
        Router->>Target: CAN Frame (FF)
        Note over OTA: Waiting for FC (2000ms Timeout)
        Target--xRouter: [No Response / Disconnected]
        Note over OTA: Timeout Expires!
        OTA->>Router: ota_twai_stop_listening()
        OTA-->>HTTP: Target Timeout Error
        HTTP-->>User: 500 Internal Server Error
    else Scenario C: Status / NACK Error
        User->>HTTP: POST /flash (Valid Firmware)
        HTTP->>OTA: Read 1st Chunk & Validate
        OTA->>Router: ota_twai_start_listening()
        OTA->>Router: twai_daemon_transmit(First Frame 0x10)
        Router->>Target: CAN Frame (FF)
        Target-->>Router: CAN Frame (0x40 Status Error / NACK)
        Router-->>OTA: ota_rx_queue (NACK Frame)
        Note over OTA: NACK Detected!
        OTA->>Router: ota_twai_stop_listening()
        OTA-->>HTTP: Flashing Aborted Error
        HTTP-->>User: 500 Internal Server Error
    end
```
