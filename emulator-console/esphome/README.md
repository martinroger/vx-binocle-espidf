# ESPHome Binocle Vehicle Emulator Firmware

Standalone, modern ESPHome firmware for the **VX Binocle Vehicle Emulator (ESP32-S3)**. This firmware replaces the external Node-RED dashboard and UART text console by running directly on the ESP32-S3, providing a real-time web dashboard, Home Assistant native API integration, bidirectional MQTT control, physical UART logging, and sub-hertz precision hardware MCPWM signal generation.

---

## Key Features

- **Standalone Web Server Dashboard (v3)**: Control gauges, switches, fuel levels, and test presets directly from any browser at `http://binocle-emulator.local` (with real-time live terminal log stream).
- **Physical UART Logging**: Real-time debug output on USB-Serial / UART0 at `115200 baud`.
- **Sub-Hertz Precision MCPWM Engine**:
  - **Vehicle Speed**: 0..271 km/h (0..168 mph) with inverted hardware polarity ($f = \text{kph} \times \frac{31285}{7651} \text{ Hz}$ @ 33% duty).
  - **Engine RPM**: 0..9000 RPM ($f = \frac{\text{rpm}}{30} \text{ Hz}$ @ 50% duty).
  - **Coolant Temperature**: 70..130 °C ($\text{duty} = (\text{temp} - 64) \times \frac{100}{71} \text{ \%}$ @ 100 Hz fixed base frequency).
  - **Zero-Glitch Pause**: < 3 Hz pauses channel by forcing hardware level LOW.
- **16 Discrete Vehicle Indicators**:
  - Controlled via Primary TCA9555 (`0x20`) with per-line polarity inversion (`inverted: true/false`).
  - Home Assistant / Web UI logical state is always intuitive (`ON` = Warning Active / Light ON).
- **19-Step Calibrated Fuel Sender Resistor Network**:
  - Controlled via Secondary TCA9555 (`0x21`).
  - 19 discrete steps from 30.2 $\Omega$ (Full Tank) to 270.0 $\Omega$ (Empty Tank), plus raw low/high caliber load divider inputs.
- **Instant Boot Standby**: Early boot sequence (`on_boot` priority 600) immediately applies mask `0xD940` (Ignition ON standby) and initial fuel resistor mask `0x7FFF` on power-up.
- **Automated Routines & Presets**: Lamp Check / Cluster Self-Test, Gauge Sweep, Synchronized Hazard Flasher, All Off Safe State, and Ignition ON Preset.
- **Complete Bidirectional MQTT & Home Assistant Integration**.

---

## Hardware Pinout & Architecture

All pin assignments, I2C addresses, and conversion coefficients are centrally declared in `substitutions:` at the top of `binocle-emulator.yaml` (matching `sdkconfig.defaults` and `main/Kconfig.projbuild`):

| Peripheral / Function | ESP-IDF Kconfig Symbol | Physical Hardware Pin | Substitution Key |
| :--- | :--- | :--- | :--- |
| **I2C SDA** | `CONFIG_SDA_PIN` | GPIO 5 | `pin_sda: "GPIO5"` |
| **I2C SCL** | `CONFIG_SCL_PIN` | GPIO 6 | `pin_scl: "GPIO6"` |
| **5V Power Enable** | `CONFIG_ENABLE_5V_PIN` | GPIO 8 | `pin_5v_enable: "GPIO8"` |
| **Speed MCPWM (Group 1)** | `CONFIG_SPEED_PWM_GEN_GPIO` | GPIO 10 | `pin_pwm_speed: "GPIO10"` |
| **RPM MCPWM (Group 0)** | `CONFIG_RPM_PWM_GEN_GPIO` | GPIO 11 | `pin_pwm_rpm: "GPIO11"` |
| **Coolant MCPWM (Group 0)** | `CONFIG_COOLANT_PWM_GEN_GPIO` | GPIO 12 | `pin_pwm_coolant: "GPIO12"` |
| **Primary Expander (0)** | `CONFIG_PRIMARY_IO_EXPANDER_ADDRESS` | `0x20` (TCA9555) | `i2c_addr_primary: "0x20"` |
| **Secondary Expander (1)** | `CONFIG_SECONDARY_IO_EXPANDER_ADDRESS` | `0x21` (TCA9555) | `i2c_addr_resistor: "0x21"` |

---

## Primary Expander (0x20) Indicator Mappings & Polarity

| Pin | Indicator / Function | Hardware Level | Inversion Substitution | Initial Standby (0xD940) |
| :--- | :--- | :--- | :--- | :--- |
| **Pin 15** | Ignition | Active High | `inv_ignition: "false"` | **ON (1)** |
| **Pin 14** | High Beams | Active High | `inv_hi_beams: "false"` | **ON (1)** |
| **Pin 13** | Alternator - Battery Charge | Active Low | `inv_alternator: "true"` | OFF (0) |
| **Pin 12** | Left Turn Indicator | Active High | `inv_left_turn: "false"` | **ON (1)** |
| **Pin 11** | Right Turn Indicator | Active High | `inv_right_turn: "false"` | OFF (0) |
| **Pin 10** | ABS Warning | Active Low | `inv_abs: "true"` | OFF (0) |
| **Pin 9** | Door Ajar | Active Low | `inv_door: "true"` | **ON (1)** |
| **Pin 8** | Low Coolant | Active Low | `inv_coolant_low: "true"` | OFF (0) |
| **Pin 7** | Cluster Button - Trip | Active Low | `inv_button: "true"` | OFF (0) |
| **Pin 6** | Alarm Status | Active High | `inv_alarm: "false"` | **ON (1)** |
| **Pin 5** | Cluster Backlight | Active High | `inv_backlight: "false"` | **ON (1)** |
| **Pin 4** | Check Engine Light (CEL) | Active Low | `inv_cel: "true"` | OFF (0) |
| **Pin 3** | Airbag Warning | Active Low | `inv_airbag: "true"` | OFF (0) |
| **Pin 2** | Oil Pressure Low | Active Low | `inv_oil_low: "true"` | OFF (0) |
| **Pin 1** | Parking Brake | Active Low | `inv_parking_brake: "true"` | OFF (0) |
| **Pin 0** | Low Brake Fluid | Active Low | `inv_brake_low: "true"` | OFF (0) |

---

## Local Compilation & Validation (CLI)

Use the automated local runner script to validate syntax and compile firmware:

```bash
# 1. Ensure script is executable
chmod +x validate.sh

# 2. Run schema validation and full ESP-IDF toolchain build
./validate.sh
```

### Manual ESPHome Commands
```bash
# Activate virtualenv
source .venv/bin/activate

# Validate YAML syntax only
esphome config binocle-emulator.yaml

# Compile ESP-IDF firmware binary
esphome compile binocle-emulator.yaml

# Flash over USB-Serial or OTA
esphome run binocle-emulator.yaml

# Monitor live serial/wireless logs
esphome logs binocle-emulator.yaml
```

---

## MQTT Topic Reference

`topic_prefix: binocle-emulator`

### 1. Switches & Power
- **Command**: `binocle-emulator/switch/<entity>/command` (`ON`, `OFF`, `TOGGLE`)
- **State**: `binocle-emulator/switch/<entity>/state` (`ON`, `OFF`)
- Entities: `power_5v`, `hazard_flasher`, `ind_ignition`, `ind_hi_beams`, `ind_alternator`, `ind_left_turn`, `ind_right_turn`, `ind_abs`, `ind_door`, `ind_coolant_low`, `ind_button`, `ind_alarm`, `ind_backlight`, `ind_cel`, `ind_airbag`, `ind_oil_low`, `ind_parking_brake`, `ind_brake_low`.

### 2. Sliders & Numbers
- **Command**: `binocle-emulator/number/<entity>/command` (Payload: Float string)
- **State**: `binocle-emulator/number/<entity>/state`
- Entities: `speed_kph` (0..271), `speed_mph` (0..168), `engine_rpm` (0..9000), `coolant_temp` (70..130), `fuel_percent` (0..100), `low_res_divider` (0..8), `high_res_divider` (0..8).

### 3. Fuel Resistor Selection
- **Command / State**: `binocle-emulator/select/fuel_resistor_step/command` (e.g. `"Step 01 - 30.2 Ω (Full)"` .. `"Step 19 - 270.0 Ω (Empty)"`).

### 4. Action Buttons
- **Command**: `binocle-emulator/button/<entity>/command` (`PRESS`)
- Entities: `btn_lamp_test`, `btn_gauge_sweep`, `btn_all_off`, `btn_ignition_on`.

### 5. Custom JSON Migration Topics
- **Batch PWM**: Send `{"speed": 120, "rpm": 3500, "temp": 90}` to `binocle-emulator/custom/pwm`.
- **Expander 0 Mask**: Send `{"mask": 55616}` (0xD940) to `binocle-emulator/custom/set_mask_0`.

