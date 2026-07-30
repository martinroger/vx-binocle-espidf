# ITF Factory App (`factory apps/ITF factory app`)

`ITF factory app` is a production diagnostic and Over-The-Air (OTA) flashing tool for the central Interface Board and display cluster hardware.

---

## Technical Overview

- **Target SoC**: ESP32-S3
- **Network Mode**: Wi-Fi Access Point (SSID: `ITF-factory`)
- **mDNS Hostname**: [`http://itf-factory.local`](http://itf-factory.local)
- **Web Interface**: Embedded ESP HTTP server serving static frontend assets from SPIFFS partition (`/spiffs`)
- **Target ESP-IDF Version**: ESP-IDF v6.2.0 (`eim run "<idf.py command>" master`)

---

## Key Diagnostic & Flashing Features

- **UDS ISO-TP Over CAN Flashing**: Streams firmware binaries chunk-by-chunk via HTTP `POST /flash` directly to target CAN nodes (Left/Right display clusters or Interface board) using ISO 15765-2 framing.
- **Hardware Diagnostics**: Reads raw ADC channels, tests 5V power channels, and verifies peripheral status.
- **NVS & Odometer Management**: Reads/writes calibration coefficients and odometer metrics.
- **Partition Selection**: Switches active boot partition between `factory`, `ota_0`, and `ota_1` via HTTP POST (`/set_boot`).

---

## Documentation

- For general web server architecture, mDNS, and diagnostic APIs, see [TOO.MD](TOO.MD).
- For a detailed breakdown of UDS ISO-TP OTA flashing logic and Mermaid sequence diagrams, see [OTA_TOO.MD](OTA_TOO.MD).
