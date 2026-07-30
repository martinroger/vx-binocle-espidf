# Left Display Factory App - Theory of Operation (`TOO.MD`)

The **Left Display Factory App** provides a standalone web interface for manufacturing diagnostics, display self-tests, NVS partition management, and partition boot switching on the Left Display hardware.

---

## 1. System Architecture

```
 +-----------------------------------------------------------------------------------+
 |                                   Client Device                                   |
 |                (Connected to Wi-Fi SoftAP: "left-display")                        |
 +-----------------------------------------------------------------------------------+
                                          |
                        [HTTP GET/POST to left-display.local]
                                          v
 +-----------------------------------------------------------------------------------+
 |                        ESP32-S3 HTTP Web Server                                   |
 |                                                                                   |
 |  +--------------------+   +-----------------------+   +------------------------+  |
 |  |  SPIFFS Asset Host |   | Diagnostic REST APIs  |   | Partition Manager      |  |
 |  |  (/spiffs)         |   | (NVS, Calibration)    |   | (Boot Switching)       |  |
 |  +--------------------+   +-----------------------+   +------------------------+  |
 +-----------------------------------------------------------------------------------+
                                          |
                                          v
 +-----------------------------------------------------------------------------------+
 |                             Hardware Peripheral Self-Tests                        |
 +-----------------------------------------------------------------------------------+
```

---

## 2. Network & Server Setup

1. **SoftAP Mode**: Configures ESP-IDF Wi-Fi driver in Access Point mode (`start_ap_mode()`) broadcasting SSID `left-display` using MAC address derivation.
2. **mDNS Service**: Registers hostname `left-display` (`http://left-display.local`) via `start_mdns()`.
3. **SPIFFS Storage Mounting**: Registers VFS SPIFFS filesystem (`storage` partition) at `/spiffs` (`start_webserver()`), serving static files (`index.html`, `simple.min.css`).

---

## 3. HTTP Endpoints

| Endpoint | Method | Function |
| :--- | :--- | :--- |
| `/` | `GET` | Serves main diagnostic web page (`/spiffs/index.html`) |
| `/simple.min.css` | `GET` | Serves CSS stylesheet (`/spiffs/simple.min.css`) |
| `/version` | `GET` | Returns firmware app version, compile date, and commit SHA |
| `/partitions` | `GET` | Returns active app partition and flash table layout |
| `/prevboot` | `GET` | Returns previous boot partition state |
| `/odometer` | `GET` | Reads odometer/trip data from NVS (`nvs_odo`) |
| `/calibration` | `GET` | Reads factory calibration coefficients from NVS |
| `/set_cal` | `POST` | Writes updated sensor calibration values to NVS |
| `/set_trip_odo` | `POST` | Overwrites odometer/trip values in NVS |
| `/set_boot` | `POST` | Switches OTA boot partition (`factory`, `ota_0`, `ota_1`) and reboots |
| `/reboot` | `POST` | Executes system software reset (`esp_restart()`) |
