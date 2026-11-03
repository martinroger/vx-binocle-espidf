# Naming Conventions Rule

Applies to every sub-project in this repository: `interface_board`, `left_screen`, `right_screen`, the three apps in `factory apps/`, `bennu`, `emulator-console` and `common/`.

## NVS namespaces and keys

- **Single source of truth**: Every NVS namespace and key is declared in [`common/nvs_keys.hpp`](../../common/nvs_keys.hpp). Never write a namespace or key as a string literal in application code; add it to that header and use the constant.
- **Length limit**: ESP-IDF limits namespaces and keys to 15 characters. Each block in `nvs_keys.hpp` ends with a `static_assert(fits(...))`; add every new name to it.
- **Namespaces**: `vx_<domain>`, lower case, one namespace per group of settings that are edited and reset together. Current ones: `vx_sys` (boot bookkeeping), `vx_fuel`, `vx_coolant`, `vx_r39`, `vx_disp`, `vx_gear` (owned by the vx-gear-estimator component).
- **Keys**: short lower `snake_case`, no domain prefix (the namespace already says it), pattern `<thing>[_<qualifier>][_<unit>]`.
- **Units** end the key, from this list only: `m`, `km`, `ohm`, `pct`, `degc`, `rpm`, `ms`, `bp` (basis points), `ckph` (0.01 km/h). A raw count or ratio has no unit.
- **Booleans**: a feature switch ends in `_en`; a choice between two states names the state that `1` means (`theme_light`, `unit_mph`).
- **Abbreviations**: only `en`, `bl` (backlight), `ovr` (override). Spell out everything else.
- **Frozen names**: the odometer and trip (`nvs_odo` partition, namespace `storage`, keys `odometer_m`, `trip_m`) must not be renamed.
- **Renames and type changes break stored data**. Reading a key under a new name or a new type finds nothing on a deployed unit. When a key must change:
  1. Add a row to `vx::nvs::legacy::RENAMES` so `vx::nvs::migrateLegacyKeys()` copies the old value once at boot.
  2. Keep the old entry readable until every unit runs the matching factory app (deployed through Bennu).
  3. Update the NVS tables in the affected `TOO.MD` files.
- **Loading settings**: use `vx::nvs::loadOrInit<T>()` from [`common/nvs_settings.hpp`](../../common/nvs_settings.hpp), with a range check where the value has limits, so a missing or out-of-range value is replaced by its default.

## C++ symbols

| Kind | Style | Example |
| :--- | :--- | :--- |
| Variables, globals, struct/class members | `camelCase` | `fuelFullOhm`, `rollbackPossible` |
| Functions and methods | `camelCase`, verb first | `sendFastMetrics()`, `loadR39Params()` |
| Types (`struct`, `class`, `enum class`, aliases) | `PascalCase`, no `_t` on new C++ types | `DisplayBoardState` |
| `enum class` enumerators | `PascalCase` | `Source::Nvs` |
| `constexpr` constants and macros | `UPPER_SNAKE` | `A_BP_MAX`, `COEFF_MPH_TO_KPH` |
| Namespaces | short lower case | `r39`, `vx::nvs` |
| Files | `snake_case.hpp` / `.cpp` | `nvs_settings.hpp` |
| Kconfig | `CONFIG_UPPER_SNAKE` | `CONFIG_R39_BLEND_END_KPH` |

- **Unit as the last word**: `speedKph`, `odometerKm`, `coolantDegC`, `fuelLevelPct`, `supplyV`, `latchMs`. Never `_v`, `_pc` or no unit.
- **Booleans read as a question or a state**: `is...`, `has...`, `show...`, `use...`, or an adjective (`themeLocked`, `coolantHot`). Do not use an `_ST` suffix.
- **Previous-value copies** use `prev` (`prevSpeedKph`), never `p_`.
- **CAN frame functions** keep the frame name with a verb: `pack<Frame>()` builds and sends a frame, `on<Frame>()` handles a received one, `on<Frame>Timeout()` is its timeout callback (for example `packItfFastMetrics`, `onItfBoardSt`, `onItfFastMetricsTimeout`).
- **Prefer C++**: new code goes in `.hpp`/`.cpp`, with `constexpr` in a namespace rather than `#define`. The `COEFF_*` macros in `common/coefficients.h` stay as macros for now.
- **Locals and parameters** in older code may still be `snake_case`. Rename them to `camelCase` when you touch the function, not in unrelated changes.

## Names that must not be renamed

- Symbols generated from the DBC (`binocan_*`, `BINOCAN_*`) and the CAN signal names.
- ESP-IDF, LVGL and esp_lvgl_adapter APIs, and EEZ Studio generated UI symbols (`objects.*`, `action_*` callbacks).
- Symbols declared by external components, even when they appear in app code. For example, `CAN_RX_TimedOut` belongs to twai_daemon, and `gear_params_t` / `gear_params_nvs_*` belong to vx-gear-estimator.
- The C APIs of the in-repo C helper headers and components (`odometer_get`, `sma_add`, `display_hw_*`, `adc_helpers.h`...), which follow ESP-IDF `module_verb` style. Use camelCase inside a namespace only when one is converted to C++.
- REST paths and JSON field names of the factory apps (`/api/r39`, `"fuel_full_r"`). The web UIs depend on them.
