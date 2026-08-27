# RDB Factory App — Theory of Operation

## 1. Architecture Overview

```mermaid
graph TD
    A[Client Browser] -->|Wi-Fi SoftAP / HTTP| B(ESP-IDF HTTP Server)
    B -->|REST & WebSockets| C(Display Diagnostics Engine)
    C -->|RGB / Pattern Flush| D[LCD Display Panel]
    B -->|OTA Binary Upload| E(esp_ota Storage Handler)
    E -->|Write Flash| F[OTA App Partition]
```

---

## 2. Display Calibration Mechanics

- **Geometry & Alignment Grid**: Displays crosshairs and boundary rectangles to calibrate physical bezel alignment.
- **Backlight Curve Tuning**: Allows fine-tuning of PWM backlight curves stored in NVS.
- **Partition Switching**: Handles active partition verification and reboot execution.
