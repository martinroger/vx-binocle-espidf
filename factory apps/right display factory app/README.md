# Right Display Factory App (`right display factory app`)

`right display factory app` is a production diagnostic and flashing firmware for the **Right Screen Cluster** board.

---

## Technical Overview

- **Target SoC**: ESP32-S3
- **Network Mode**: Wi-Fi Access Point (SSID: `right-display`)
- **mDNS Hostname**: [`http://right-display.local`](http://right-display.local)
- **Web Interface**: Embedded ESP HTTP server serving static frontend assets from SPIFFS partition (`/spiffs`)
- **Target ESP-IDF Version**: ESP-IDF v6.2.0 (`eim run "<idf.py command>" master`)

---

## Key Diagnostic Features

- **Hardware Self-Test**: Tests ST7701 display panel, backlight PWM, and touch/button inputs.
- **NVS Management**: Formats default and `nvs_odo` partitions, inspects stored settings, and performs calibration resets.
- **Partition Selection**: Switches active boot partition between `factory`, `ota_0`, and `ota_1` via HTTP POST (`/set_boot`).

---

## Documentation

For a detailed architectural breakdown of web server endpoints, SPIFFS mounting, mDNS registration, and partition management, see [TOO.MD](TOO.MD).
