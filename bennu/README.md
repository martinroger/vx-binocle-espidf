# Bennu — Special Delivery Firmware

## 📖 Purpose & Overview

**Bennu** is a lightweight "special delivery" utility firmware for ESP32-S3 nodes in the VX Binocle ecosystem. It hijacks the ESP32 OTA update mechanism to perform bare-metal deployment of factory applications and SPIFFS storage partitions directly onto target boards.

### Operational Principle:
1. When a factory application needs to be deployed or updated remotely, a Bennu delivery package (`<TARGET>_bennu.bin`) is uploaded to an OTA partition via the board's web server OTA endpoint.
2. The board reboots into the Bennu OTA partition.
3. Bennu locates the target `factory` app partition and `storage` (SPIFFS) partition.
4. It overwrites the `factory` partition with the embedded factory application payload (`payload.bin`).
5. It overwrites the `storage` partition with the embedded SPIFFS image (`storage.bin`).
6. Bennu marks its own OTA partition as invalid (`esp_ota_mark_app_invalid_rollback()`), sets the `factory` partition as the active boot partition, and restarts the microcontroller into the newly flashed factory application.

---

## 🗂️ Project Structure

```
bennu/
├── CMakeLists.txt              # Root ESP-IDF CMakeLists
├── partitionTable.csv          # Partition layout definition
├── components/
│   └── payload/
│       ├── CMakeLists.txt      # Parameterized binary embedding (PAYLOAD_BIN_PATH / STORAGE_BIN_PATH)
│       ├── payload.bin         # Default / fallback factory app binary
│       └── storage.bin         # Default / fallback SPIFFS storage image
├── main/
│   ├── CMakeLists.txt          # Main component registration
│   └── main.cpp                # Flash overwrite and boot handover logic
├── TOO.MD                      # Theory of Operation & Sequence Diagrams
└── README.md                   # Subproject overview (this file)
```

---

## 🛠️ Building Bennu Delivery Packages

Delivery packages are built automatically using the repository-level Python tool:

```bash
# Build all 3 special delivery packages (ITF_bennu.bin, LDB_bennu.bin, RDB_bennu.bin)
python tools/build_deliveries.py --app all

# Build only ITF delivery package
python tools/build_deliveries.py --app itf

# Package delivery binaries from pre-built factory binaries
python tools/build_deliveries.py --from-bins dist/
```

Manual build with custom payload paths:
```bash
cd bennu
idf.py -B build_itf \
  -DPAYLOAD_BIN_PATH="/path/to/ITF_factory.bin" \
  -DSTORAGE_BIN_PATH="/path/to/storage.bin" \
  reconfigure build
```

---

## 📚 Technical Documentation
For detailed flash layout specifications, state transitions, and recovery diagrams, refer to [TOO.MD](TOO.MD).

---

## 🦅 Culture & Name Origin

The project name **Bennu** draws dual inspiration from ancient mythology and modern space exploration:

- **The Egyptian Phoenix (Bennu)**: In ancient Egyptian mythology, Bennu is the sacred solar heron of creation, rebirth, and renewal (the precursor to the Greek Phoenix). Like the Phoenix, Bennu executes a self-immolation cycle: it boots to erase and reconstruct the system from the ground up, invalidates and "kills" its own staging partition, and allows the target board to rise freshly reborn in factory mode.
- **Asteroid 101955 Bennu**: Named after the mythological bird, Asteroid Bennu was the target of NASA's *OSIRIS-REx* mission, which touched down on the asteroid to collect and deliver a pristine sample payload back to Earth. Similarly, this firmware functions as a specialized transport vehicle carrying and delivering a mission-critical payload to the MCU.
