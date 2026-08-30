# Theory of Operation — Bennu Special Delivery Subsystem

## 1. System Architecture & Context

**Bennu** is an operational recovery and factory bootstrap mechanism. In standard operation, updating the `factory` app partition and the `storage` partition cannot be performed directly while the running application occupies that space or manages active SPIFFS mounts. 

Bennu solves this problem by packaging the target firmware image and its corresponding SPIFFS filesystem into an executable ESP32 binary payload flashed into an OTA slot.

```mermaid
graph TD
    subgraph Host_Environment [Development / CI Environment]
        FactoryApp[Factory Application .bin]
        StorageFS[SPIFFS Filesystem Image .bin]
        BennuSource[Bennu Firmware Source]
        
        FactoryApp & StorageFS & BennuSource -->|CMake EMBED_FILES| DeliveryPkg["Special Delivery Package (<TARGET>_bennu.bin)"]
    end

    subgraph Target_MCU [ESP32-S3 Target Device]
        OTA_Upload[Web Server / OTA Ingestion] -->|Flash into ota_0/ota_1| OTASlot[(OTA Partition)]
        OTASlot -->|Reboot into Bennu| BennuRuntime[Bennu app_main]
        
        BennuRuntime -->|Erase & Write| FactorySlot[(Factory Partition)]
        BennuRuntime -->|Erase & Write| StorageSlot[(Storage Partition)]
        BennuRuntime -->|esp_ota_set_boot_partition| BootSector[(Boot Partition Config)]
        BootSector -->|Restart| FinalApp[Active Factory Application]
    end
```

---

## 2. Sequence Diagram: Delivery Handshake & Flash Rewrite

```mermaid
sequenceDiagram
    autonumber
    actor User as Field Engineer / CI
    participant Web as Factory Web Server
    participant MCU_OTA as Running OTA Handler
    participant Bennu as Bennu app_main()
    participant NVS as Boot / NVS Configuration
    participant Flash as Flash Memory (Factory & Storage)

    User->>Web: Upload <TARGET>_bennu.bin via /update endpoint
    Web->>MCU_OTA: Stream bytes to passive OTA partition (e.g. ota_0)
    MCU_OTA->>NVS: Set next boot partition to ota_0
    MCU_OTA->>MCU_OTA: Restart MCU

    Note over Bennu: MCU Boots from ota_0 (Bennu)
    Bennu->>Bennu: Parse embedded _binary_payload_bin and _binary_storage_bin
    Bennu->>Flash: Locate 'factory' partition (app) and 'storage' partition (spiffs)
    
    alt Factory Partition Not Found or Matches Running Address
        Bennu-->>Bennu: Abort execution (Safety check)
    else Factory Partition Valid
        Bennu->>Flash: Erase factory partition range
        Bennu->>Flash: Write payload binary to factory partition
        Bennu->>NVS: esp_ota_mark_app_invalid_rollback()
        Bennu->>NVS: esp_ota_set_boot_partition(factoryPart)
    end

    alt Storage Binary Size > 0
        Bennu->>Flash: Erase storage partition range
        Bennu->>Flash: Write storage binary to storage partition
    end

    Bennu->>Bennu: 5-second countdown delay
    Bennu->>MCU_OTA: esp_restart()
    Note over Flash: MCU Boots cleanly into newly installed Factory Firmware
```

---

## 3. Flash Layout & Partition Targets

Bennu interacts with the following partition definitions:

| Partition Name | Type | Subtype | Function |
| :--- | :--- | :--- | :--- |
| `nvs` | `data` | `nvs` | Non-volatile settings & calibration storage |
| `otadata` | `data` | `ota` | OTA boot selection metadata |
| `factory` | `app` | `factory` | Primary target partition for payload binary |
| `ota_0` / `ota_1` | `app` | `ota_0` / `ota_1` | Transient staging partition running Bennu |
| `storage` | `data` | `spiffs` | Target filesystem partition for web & config assets |

---

## 4. Safety & Error Recovery Guarantees

1. **Self-Target Abort**: Bennu checks `esp_ota_get_running_partition()` against the destination `factory` partition address. If Bennu itself is running in the `factory` partition, it immediately halts to prevent self-erasure.
2. **Rollback Invalidation**: Immediately following the successful write to the `factory` partition, Bennu invokes `esp_ota_mark_app_invalid_rollback()`. This guarantees that if the subsequent reboot encounters an unexpected power failure, the bootloader will not attempt to boot the temporary Bennu partition again.
3. **Partition Boundary Verification**: Erase and write operations specify exact partition offsets and bounded sizes (`factoryPart->size` and `payload_bin_size`).
