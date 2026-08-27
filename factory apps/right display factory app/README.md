# Right Display Factory App (RDB Factory)

## Overview
The **RDB Factory App** provides the provisioning, calibration, and OTA environment for the Right Display Board. It mirrors the LDB factory test suite for display diagnostics and secondary UI calibration.

## Key Capabilities
- **Wi-Fi AP & mDNS**: Broadcasts `rdb-factory.local`.
- **Display Test Patterns**: Generates full-screen RGB and geometry calibration grids.
- **Web OTA**: Web browser-based firmware upgrades.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For detailed architecture, see [TOO.MD](TOO.MD).
