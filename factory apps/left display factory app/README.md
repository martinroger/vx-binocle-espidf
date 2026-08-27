# Left Display Factory App (LDB Factory)

## Overview
The **LDB Factory App** is the calibration, self-test, and recovery firmware for the Left Display Board. It provides Wi-Fi captive portal access for display alignment, touch/button calibration, and firmware OTA uploads.

## Key Capabilities
- **Wi-Fi AP & mDNS**: Broadcasts `ldb-factory.local`.
- **Display Test Patterns**: Generates primary RGB color screens, grid alignment patterns, and font readability tests.
- **OTA Updates**: Web-based partition flashing for easy field updates.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For detailed architecture, see [TOO.MD](TOO.MD).
