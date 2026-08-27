# Interface Board (ITF)

## Overview
The **Interface Board (ITF)** is the primary sensor acquisition node of the VX Binocle system. It captures physical vehicle signals (wheel speed pulses, ignition pulses, fuel resistance, analog sensor voltages, and discrete switch inputs) and broadcasts standardized CAN frames over the TWAI bus.

## Hardware Architecture
- **Microcontroller**: ESP32-S3 (240 MHz dual-core, 8 MB Flash, 2 MB PSRAM).
- **Analog Front-End**: Texas Instruments ADS1115 (16-bit I2C ADC) for precise fuel level, battery, and auxiliary voltage sampling.
- **I/O Expansion**: NXP/TI TCA9555 16-bit I2C GPIO expander for discrete vehicle switch inputs (lights, indicators, handbrake, etc.).
- **Pulse Ingestion**: ESP32-S3 MCPWM capture channels for high-frequency wheel speed and engine RPM pulses.
- **Bus Transceiver**: 3.3V CAN/TWAI transceiver with termination.

## Building & Flashing
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

For detailed architecture, signal processing loops, and telemetry timing, see [TOO.MD](TOO.MD).
