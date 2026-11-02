# Left Display Factory App (LDB Factory)

## Overview
The **LDB Factory App** is the recovery and maintenance firmware for the Left Display Board. It runs from the `factory` partition and serves a small web page over its own Wi-Fi access point for firmware uploads, boot partition selection and NVS maintenance. It does not drive the LCD.

## Key Capabilities
- **Wi-Fi AP & mDNS**: Starts a SoftAP named `LDB-XXXXXX` (last three MAC bytes) and answers on `left-display.local`.
- **NVS Inspection & Wipe**: Scans all active NVS partitions and entries; supports full NVS wipe.
- **OTA Updates**: Browser upload of an app image to the next OTA partition, which then becomes the boot partition (`/upload`); manual boot partition selection (`/set_boot`) and reboot.
- **CAN**: Starts `twai_daemon` (TX GPIO 19, RX GPIO 20) so the transceiver TX line is driven recessive while the factory app runs.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For detailed architecture, see [TOO.MD](TOO.MD).
