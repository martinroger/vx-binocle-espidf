# CAN OTA Update Logic (UDS ISO-TP)

This document details the over-the-air (OTA) logic implemented in `main.cpp` (specifically within the `flash_post_handler` function), which bridges an HTTP firmware upload to a CAN bus node using the UDS (Unified Diagnostic Services) protocol over ISO-TP.

## Overview

The ESP32 acts as an HTTP server receiving a firmware file via POST request. Instead of flashing itself, it streams the firmware chunks over the CAN bus to a target node. 

The update process implements the **ISO 15765-2 (ISO-TP)** standard for sending data payloads larger than 8 bytes over classic CAN frames.

## 1. HTTP Chunk Reception and Validation
1. **Chunked Reception:** The firmware is read in chunks using `httpd_req_recv` to avoid loading the entire file into RAM.
2. **Header Validation:** On the first chunk, the code scans for the ESP-IDF image magic word (`0xABCD5432`) to ensure the uploaded file is a valid ESP firmware binary. If not found, the transfer is immediately aborted.

## 2. TWAI Daemon Interception (v6.0 Migration Logic)
During the OTA transfer, the standard background TWAI (CAN) daemon must be bypassed so the OTA loop can synchronously poll for specific responses:
- `vTaskSuspend(CAN_RX_tsk_hdl)` is intercepted via a macro in `main.cpp`. 
- Instead of suspending the task, it points `dispatchCANFrame` to a local `ota_dispatch` function.
- All incoming CAN frames are pushed into a dedicated `ota_rx_queue`.
- `twai_receive` macros pull directly from this queue, giving the OTA loop exclusive, blocking access to incoming frames.
- Upon completion, `vTaskResume(CAN_RX_tsk_hdl)` restores the daemon dispatcher to `NULL`.

## 3. ISO-TP Transfer Sequence

### Step 3.1: First Frame (FF)
Once the daemon is suspended, the OTA process initiates the transfer by sending a **First Frame (FF)**:
- **Byte 0:** `0x10` (Indicates a First Frame for payloads > 4095 bytes)
- **Byte 1:** `0x00` (Escape sequence for extended data length)
- **Bytes 2-5:** Total file size (Big Endian)
- **Bytes 6-7:** First 2 bytes of the firmware payload.

### Step 3.2: Flow Control (FC) Wait
After sending the FF, the ESP32 blocks and waits for a **Flow Control (FC)** frame from the target node:
- **`0x30` (Clear To Send / CTS):** The target is ready to receive.
  - **Byte 1 (`CANBlockSize`):** Maximum number of frames to send before waiting for another FC. (0 = no limit).
  - **Byte 2 (`STmin_MS`):** Minimum Separation Time (in milliseconds) required between consecutive frames.
- **`0x40` (Status Frame):** Handled as an error if the status code is not `0x00`.

An `esp_timer` (`OTA_TO_timer`) enforces a strict timeout during this wait.

### Step 3.3: Consecutive Frames (CF)
Once a CTS is received, the ESP32 loops over the remaining payload, packing it into **Consecutive Frames (CF)**:
- **Byte 0:** `0x20` + `CF_SN` (Sequence Number, rolling over from 0 to 15).
- **Bytes 1-7:** Up to 7 bytes of firmware payload.

The logic respects the Flow Control parameters:
- **Delay:** Blocks for `STmin_MS` between sending each frame.
- **Block Size Limits:** If `CANBlockSize` is reached, it halts transmission and waits for another FC CTS (`0x30`) from the target before continuing.

## 4. Completion and Cleanup
- The loop continues fetching HTTP chunks and shipping CF frames until `sentBytes == file_size`.
- If any timeouts or errors occur, the ESP32 aborts the transfer, returns an HTTP 500/400 error to the client, and safely restores the TWAI daemon.
- On success, the TWAI daemon is restored (`dispatchCANFrame = NULL`), and an HTTP 200 OK is sent.
