# Emulator Console Firmware

Interactive REPL console firmware for the **VX Binocle Vehicle Emulator**. This firmware turns an ESP32-S3 into a comprehensive vehicle signal simulator and hardware test bench, generating precise PWM signals, driving active high/low indicator lines, and simulating analog fuel level sender resistances.

---

## Features

- **MCPWM Signal Generation**: Frequency and duty cycle modulation for Speed, RPM, and Coolant gauge signals (with GPIO matrix hardware inversion for inverted board circuitry).
- **Vehicle Indicator Emulation**: 16 discrete Active High / Active Low vehicle warning signals and telltale indicators via I2C GPIO expander.
- **Fuel Sender Resistor Network**: 19 discrete switched resistance steps (30.2 Ohm to 270.0 Ohm) simulating standard automotive fuel sender levels.
- **Power Rail Control**: 5V auxiliary supply enable/toggle. _Only used for circuitry testing._
- **Interactive REPL**: Rich terminal interface with tab autocompletion, persistent command history in FATFS/flash, and system diagnostics.

---

## Quickstart

### 1. Connect

Connect to the console via USB-Serial or UART at **115200 baud**. Press `Enter` to display the prompt:

```text
esp32s3# help
```

---

## Command Reference

### Custom Vehicle & Hardware Commands

| Category         | Command              | Syntax                                    | Description                                                                            |
| :--------------- | :------------------- | :---------------------------------------- | :------------------------------------------------------------------------------------- |
| **Combined**     | `pwm`                | `pwm <speed> <rpm> <temp>`                | Set Speed (kph), RPM, and Coolant temp (degC) simultaneously.                          |
| **Speed**        | `setSpeedKPH`        | `setSpeedKPH <speed>`                     | Set vehicle speed in km/h.                                                             |
|                  | `incSpeedKPH`        | `incSpeedKPH [<step>]`                    | Increment speed in km/h (default step: 5).                                             |
|                  | `decSpeedKPH`        | `decSpeedKPH [<step>]`                    | Decrement speed in km/h (default step: 5).                                             |
|                  | `setSpeedMPH`        | `setSpeedMPH <speed>`                     | Set vehicle speed in mph.                                                              |
|                  | `incSpeedMPH`        | `incSpeedMPH [<step>]`                    | Increment speed in mph (default step: 5).                                              |
|                  | `decSpeedMPH`        | `decSpeedMPH [<step>]`                    | Decrement speed in mph (default step: 5).                                              |
|                  | `getSpeed`           | `getSpeed`                                | Display current speed in kph, mph, and actual frequency.                               |
| **Engine RPM**   | `setRPM`             | `setRPM <rpm>`                            | Set engine RPM (0..9000). Pauses signal at 0 RPM.                                      |
|                  | `incRPM`             | `incRPM [<step>]`                         | Increment engine RPM (default step: 250).                                              |
|                  | `decRPM`             | `decRPM [<step>]`                         | Decrement engine RPM (default step: 250).                                              |
|                  | `getRPM`             | `getRPM`                                  | Display current RPM and actual frequency.                                              |
| **Coolant**      | `setCoolant`         | `setCoolant <temp>`                       | Set coolant temperature in degC (70..130 degC).                                        |
|                  | `getCoolant`         | `getCoolant`                              | Display current coolant temperature and PWM duty cycle.                                |
| **Raw MCPWM**    | `setChannelDutyFreq` | `setChannelDutyFreq <chan> <duty> <freq>` | Set raw duty cycle (%) and frequency (Hz) for channel `0:Coolant`, `1:RPM`, `2:Speed`. |
|                  | `setChannelDuty`     | `setChannelDuty <chan> <duty>`            | Set raw duty cycle (%) on channel (0..2).                                              |
|                  | `setChannelFreq`     | `setChannelFreq <chan> <freq>`            | Set raw frequency (Hz) on channel (0..2). Frequency < 3 Hz pauses channel.             |
|                  | `getChannelsInfo`    | `getChannelsInfo`                         | Query active/paused status, frequency, and duty cycle for all 3 channels.              |
| **Power**        | `set_5V`             | `set_5V [<1\|0>]`                         | Turn 5V output rail ON (1), OFF (0), or toggle if omitted.                             |
| **Fuel Level**   | `setFuelResLevel`    | `setFuelResLevel [<1..19>]`               | Set fuel sender resistance level (Level 1: 30.2 Ohm to Level 19: 270.0 Ohm).           |
|                  | `incFuelResLevel`    | `incFuelResLevel`                         | Step fuel resistance up to next level (towards Empty).                                 |
|                  | `decFuelResLevel`    | `decFuelResLevel`                         | Step fuel resistance down to previous level (towards Full).                            |
|                  | `setLowResOut`       | `setLowResOut <0..8>`                     | Low caliber resistor emulator (max 270 Ohm, 0 is open circuit).                        |
|                  | `setHighResOut`      | `setHighResOut <0..8>`                    | High caliber resistor emulator (max 2000 Ohm, 0 is open circuit).                      |
| **Indicators**   | `set_ignition`       | `set_ignition [<1\|0>]`                   | Set or toggle Ignition line.                                                           |
|                  | `set_hi_beams`       | `set_hi_beams [<1\|0>]`                   | Set or toggle High Beams indicator.                                                    |
|                  | `set_alternator`     | `set_alternator [<1\|0>]`                 | Set or toggle Alternator / Battery charge warning.                                     |
|                  | `set_brake`          | `set_brake [<1\|0>]`                      | Set or toggle Brake fluid warning.                                                     |
|                  | `set_parking_brake`  | `set_parking_brake [<1\|0>]`              | Set or toggle Parking Brake / Handbrake.                                               |
|                  | `set_oil_low`        | `set_oil_low [<1\|0>]`                    | Set or toggle Low Oil Pressure warning.                                                |
|                  | `set_airbag`         | `set_airbag [<1\|0>]`                     | Set or toggle Airbag warning light.                                                    |
|                  | `set_CEL`            | `set_CEL [<1\|0>]`                        | Set or toggle Check Engine Light (CEL).                                                |
|                  | `set_right_turn`     | `set_right_turn [<1\|0>]`                 | Set or toggle Right Turn signal indicator.                                             |
|                  | `set_left_turn`      | `set_left_turn [<1\|0>]`                  | Set or toggle Left Turn signal indicator.                                              |
|                  | `set_ABS`            | `set_ABS [<1\|0>]`                        | Set or toggle ABS warning light.                                                       |
|                  | `set_door`           | `set_door [<1\|0>]`                       | Set or toggle Door Open indicator.                                                     |
|                  | `set_coolant_low`    | `set_coolant_low [<1\|0>]`                | Set or toggle Low Coolant warning light.                                               |
|                  | `set_button`         | `set_button [<1\|0>]`                     | Set or toggle Cluster Button / Trip input.                                             |
|                  | `set_alarm`          | `set_alarm [<1\|0>]`                      | Set or toggle Alarm status output.                                                     |
|                  | `set_backlight`      | `set_backlight [<1\|0>]`                  | Set or toggle Cluster Backlight illumination.                                          |
| **Raw Expander** | `setExpIO`           | `setExpIO <expander> <gpio> <1\|0>`       | Set individual GPIO on expander (0 or 1).                                              |
|                  | `setAllExpIO`        | `setAllExpIO <expander> <1\|0>`           | Set all pins on target expander to HIGH (1) or LOW (0).                                |
|                  | `getExpMask`         | `getExpMask <expander>`                   | Read 16-bit hex mask of current expander pin states.                                   |
|                  | `setExpMask`         | `setExpMask <expander> <mask>`            | Write 16-bit hex mask directly to expander outputs.                                    |
|                  | `printExpStatus`     | `printExpStatus [<expander>]`             | Print pin configuration and levels of expander(s).                                     |

---

### Built-in ESP-IDF System Commands

| Category   | Command               | Syntax                            | Description                                                          |
| :--------- | :-------------------- | :-------------------------------- | :------------------------------------------------------------------- |
| **System** | `help`                | `help [<command>]`                | Print list of all registered commands or details for a command.      |
|            | `free`                | `free`                            | Get current free heap memory size.                                   |
|            | `heap`                | `heap`                            | Get minimum free heap size recorded during execution.                |
|            | `version`             | `version`                         | Display chip information and ESP-IDF SDK version.                    |
|            | `restart`             | `restart`                         | Software reset the ESP32-S3 chip.                                    |
|            | `tasks`               | `tasks`                           | Display running FreeRTOS tasks and runtime statistics.               |
|            | `log_level`           | `log_level <tag> <level>`         | Set log level (`none`, `error`, `warn`, `info`, `debug`, `verbose`). |
|            | `light_sleep`         | `light_sleep [-t <ms>]`           | Put SoC into light sleep mode.                                       |
|            | `deep_sleep`          | `deep_sleep [-t <ms>] [--io=<n>]` | Put SoC into deep sleep mode with timer or GPIO wakeup.              |
| **NVS**    | `nvs_get`             | `nvs_get <key> <type>`            | Read key value from Non-Volatile Storage.                            |
|            | `nvs_set`             | `nvs_set <key> <type> -v <val>`   | Write key value to Non-Volatile Storage.                             |
|            | `nvs_erase`           | `nvs_erase <key>`                 | Erase specific NVS key.                                              |
|            | `nvs_erase_namespace` | `nvs_erase_namespace <ns>`        | Erase entire NVS namespace.                                          |
|            | `nvs_list`            | `nvs_list [<part>]`               | List stored NVS entries.                                             |
|            | `nvs_dump`            | `nvs_dump <part> <ns>`            | Dump NVS data partition contents.                                    |
| **Wi-Fi**  | `join`                | `join <ssid> [<password>]`        | Connect to an external Wi-Fi Access Point.                           |
|            | `scan`                | `scan`                            | Scan for nearby Wi-Fi Access Points.                                 |
|            | `sta`                 | `sta <status\|scan\|disconnect>`  | Manage Wi-Fi Station interface.                                      |
|            | `ap`                  | `ap <ssid> [<password>]`          | Configure ESP32 softAP mode.                                         |
|            | `query`               | `query`                           | Query Wi-Fi connection parameters.                                   |

---

## Hardware Defaults

| Peripheral     | Function / Signal                     | Default GPIO / Address       |
| :------------- | :------------------------------------ | :--------------------------- |
| **5V Rail**    | 5V Enable Output                      | GPIO 8                       |
| **MCPWM Ch 0** | Coolant PWM Output                    | GPIO 12                      |
| **MCPWM Ch 1** | RPM PWM Output                        | GPIO 11                      |
| **MCPWM Ch 2** | Speed PWM Output                      | GPIO 10                      |
| **I2C Bus**    | SDA / SCL                             | GPIO 5 / GPIO 6              |
| **Expander 0** | Primary IO Expander (Active Hi/Lo)    | I2C Address `0x20` (TCA9555) |
| **Expander 1** | Secondary IO Expander (Resistor Bank) | I2C Address `0x21` (TCA9555) |
| **DAC Out**    | DAC0 / DAC1                           | GPIO 13 / GPIO 14            |
