# Interface Board Factory App (ITF Factory)

## Overview
The **ITF Factory App** provides a dedicated provisioning, calibration, and maintenance environment for the Interface Board. It exposes a captive Wi-Fi Access Point and responsive Web UI for sensor calibration and NVS parameter management.

## Key Capabilities
- **Wi-Fi AP & mDNS**: Broadcasts `interface-board.local` (`ITF-XXXXXX` AP) for instant browser access.
- **NVS Provisioning & Calibration**: Auto-populates default constants if uninitialized; manages odometer/trip values, coolant overtemperature warning thresholds, low fuel threshold (`lo_fuel_thr`), fuel full resistance baseline (`fuel_full_r`), and fuel self-learning toggle (`fuel_learn_en`).
- **NVS Dynamic Inspection & Wipe**: Scans and displays all partitioned NVS keys/namespaces dynamically; provides safe full NVS storage wipe.
- **Controlled Power-Down**: Gracefully turns off the 5V boost regulator prior to rebooting into the main application.
- **OTA & Display Flashing**: Browser-based multipart OTA flashing for interface board partitions and segmented CAN flashing for Left/Right displays.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For detailed architecture and REST APIs, see [TOO.MD](TOO.MD).
