# VX Binocle Protocol & State Machine Scenarios

This document outlines key sequence flows, communication patterns, and error escape mechanisms used across the VX Binocle system.

---

## 1. System Boot & Initialization Sequence

```mermaid
sequenceDiagram
    autonumber
    participant ITF as Interface Board (ITF)
    participant CAN as TWAI / CAN Bus
    participant LDB as Left Display (LDB)
    participant RDB as Right Display (RDB)

    Note over ITF, RDB: Ignition ON (Power Applied)
    ITF->>ITF: Initialize NVS & Hardware Peripherals
    LDB->>LDB: Initialize LVGL & Display Panel
    RDB->>RDB: Initialize LVGL & Display Panel
    
    LDB->>LDB: Start Gauge Sweep Animation
    RDB->>RDB: Start Boot Self-Test Animation

    ITF->>CAN: Start TWAI Driver & Daemon
    LDB->>CAN: Start TWAI Receiver Task
    RDB->>CAN: Start TWAI Receiver Task

    loop Every 20ms / 50ms / 100ms
        ITF->>CAN: Broadcast ITF_values / ITF_status / ITF_debug
        CAN-->>LDB: Ingest CAN telemetry
        CAN-->>RDB: Ingest CAN telemetry
        LDB->>LDB: Update Speed/RPM/Coolant UI
        RDB->>RDB: Update Aux Gauge / Trip UI
    end
```

---

## 2. CAN Stream Loss & Timeout Recovery

```mermaid
sequenceDiagram
    autonumber
    participant ITF as Interface Board (ITF)
    participant CAN as TWAI Bus
    participant DSP as Display Board (LDB/RDB)

    ITF->>CAN: Broadcast periodic telemetry (Normal)
    CAN-->>DSP: Telemetry received & timestamp updated
    
    Note over CAN: Physical Cable Disconnection / Bus Error
    DSP->>DSP: TWAI receive timeout (> 500ms without frame)
    DSP->>DSP: Transition UI to "No CAN Signal" / Fallback State
    DSP->>DSP: Dim gauges / Display warning indicator
    
    Note over CAN: Bus Connection Restored
    ITF->>CAN: Resume CAN frame transmission
    CAN-->>DSP: Valid frame received
    DSP->>DSP: Reset timeout timer & restore live UI rendering
```

---

## 3. High-Priority Alert Flow: Coolant Over-Temperature

```mermaid
sequenceDiagram
    autonumber
    participant SENS as Coolant Sensor
    participant ITF as Interface Board (ITF)
    participant CAN as TWAI Bus
    participant LDB as Left Display Board
    participant BUZZ as Audio Buzzer

    SENS->>ITF: Coolant Temperature > 105°C
    ITF->>ITF: Apply SMA filter & validate reading
    ITF->>CAN: Broadcast ITF_values (High Coolant Temp)
    CAN-->>LDB: Ingest telemetry
    
    LDB->>LDB: Detect Coolant Temp > Warning Threshold
    LDB->>LDB: Flash Coolant Warning UI Icon (Red)
    LDB->>BUZZ: Trigger PWM Buzzer Pulse Pattern
    
    Note over SENS, BUZZ: Coolant Cools Down (< 98°C)
    SENS->>ITF: Normal Coolant Temperature
    ITF->>CAN: Broadcast ITF_values (Normal Temp)
    CAN-->>LDB: Ingest normal telemetry
    LDB->>BUZZ: Deactivate PWM Buzzer
    LDB->>LDB: Revert UI Icon to Normal
```

---

## 4. Diagnostic Handshake & Protocol Error Handling

```mermaid
sequenceDiagram
    autonumber
    participant TOOL as Diagnostic Tool / Master
    participant ECU as Node Under Test (ITF / Display)

    Note over TOOL, ECU: Request with Pending Response (0x78)
    TOOL->>ECU: Diagnostic Request (Multi-block Data)
    ECU->>ECU: Process request (Long duration task)
    ECU-->>TOOL: Negative Response (0x7F <ServiceId> 0x78 - ResponsePending)
    ECU->>ECU: Complete internal execution
    ECU-->>TOOL: Positive Response (0x40 + ServiceId <Data>)

    Note over TOOL, ECU: Payload Validation Failure (0x7F 0x13)
    TOOL->>ECU: Request with Incorrect Length / Format
    ECU->>ECU: Payload sanity check fails
    ECU-->>TOOL: Negative Response (0x7F <ServiceId> 0x13 - IncorrectMessageLengthOrInvalidFormat)

    Note over TOOL, ECU: General Service Not Supported (0x7F 0x11)
    TOOL->>ECU: Request Unsupported Service ID (0xAA)
    ECU-->>TOOL: Negative Response (0x7F 0xAA 0x11 - ServiceNotSupported)
```

---

## 5. OTA-Over-CAN Firmware Flashing Sequence

```mermaid
sequenceDiagram
    autonumber
    participant MASTER as Flashing Master / Tool
    participant CAN as TWAI / CAN Bus
    participant NODE as Target ECU Node (ITF / LDB / RDB)

    Note over MASTER, NODE: Phase 1: Enter OTA Mode
    MASTER->>CAN: Broadcast OTA Request Session (Target Node ID)
    CAN-->>NODE: Ingest Session Request
    NODE->>NODE: Suspend periodic telemetry transmission
    NODE->>NODE: Set state to OTA (State 5) & Prepare Next OTA Partition
    NODE-->>CAN: ACK Session Ready (Target Partition Ready)
    CAN-->>MASTER: Session ACK

    Note over MASTER, NODE: Phase 2: Segmented Block Streaming
    loop For each 4KB Binary Chunk (Streamed in 8-byte CAN Frames)
        MASTER->>CAN: Send Block Data [Block ID + Frame Index + Data]
        CAN-->>NODE: Receive frames into RAM ring buffer
        NODE->>NODE: Write buffered block to esp_ota partition
        NODE-->>CAN: Block ACK [Block ID, CRC OK]
        CAN-->>MASTER: Block ACK
    end

    Note over MASTER, NODE: Phase 3: Final Verification & Reboot
    MASTER->>CAN: Finalize Transfer [Total Size, MD5/SHA256 Digest]
    NODE->>NODE: Compute full partition hash & verify ESP32-S3 image header
    NODE->>NODE: Set Boot Partition (esp_ota_set_boot_partition)
    NODE-->>CAN: Final Validation Success ACK
    CAN-->>MASTER: Ready for Reset
    MASTER->>CAN: Broadcast Bus Reboot Command
    NODE->>NODE: esp_restart() -> Boot into New Firmware
```
