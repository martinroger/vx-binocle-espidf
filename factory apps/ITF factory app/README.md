# Interface Board Factory App (ITF Factory)

## Overview
The **ITF Factory App** provides a dedicated provisioning, calibration, and maintenance environment for the Interface Board. It exposes a captive Wi-Fi Access Point and responsive Web UI for sensor calibration and NVS parameter management.

## Key Capabilities
- **Wi-Fi AP & mDNS**: Broadcasts `itf-factory.local` for instant browser access.
- **NVS Provisioning**: Auto-populates default constants if uninitialized; persists pulses/km, wheel radius, and sensor lookup curves.
- **Controlled Power-Down**: Gracefully turns off the 5V boost regulator prior to rebooting into the main application.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For detailed architecture, see [TOO.MD](TOO.MD).
