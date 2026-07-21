# Eaton D64RP410 Ground Fault Relay Programmer

This project implements a custom **Modbus RTU Master** designed to simultaneously configure up to 50 **Eaton D64RP410 Ground Fault Relays** over an RS-485 daisy-chain network.

## Hardware Platform

- **Microcontroller:** ESP32 (e.g., Generic or WROVER)
- **Networking:** RS-485 via TTL-to-RS485 module (with auto flow control)
- **Bus Power:** 24V bus harness
- **Display:** DFRobot Gravity I2C 16x2 Arduino LCD with RGB Font Display (Black) - SKU: DFR0554 (Native 3.3V/5V compatible).
- **Physical UI:** 1x Momentary Pushbutton (Start).

## Hardware Wiring Guide

- **Power:** 24V DC to the Eaton relays, stepped down to 5V for the ESP32 Terminal board.
- **Modbus RS-485 (Serial2):**
  - RX -> GPIO 16
  - TX -> GPIO 17
- **Physical UI:**
  - Start Button -> GPIO 18 (INPUT_PULLDOWN)
- **DFRobot LCD (I2C):**
  - VCC -> 5V or 3V3 pin
  - GND -> GND
  - SDA -> GPIO 21
  - SCL -> GPIO 22

## Software Platform

- **Runtime:** OpenPLC
- **Architecture:** Hybrid C/C++ Function Blocks and IEC 61131-3 Structured Text

## Purpose

The primary goal of this project is to replace the manual configuration process (previously using QModbus desktop software) with an embedded, one-button programming harness. The device automates the process of:

1. **Phase 1 (Discovery):** Scans Modbus addresses 100-199 for active nodes. Checks for duplicate serial numbers to prevent bus collisions.
2. **Phase 2 (Programming):** Programs the relays to specific hardcoded values:
   - **Operating Mode:** 1
   - **Residual Current:** 30mA
3. **Phase 3 (Verification):** Audits every node by reading the registers back. If any node fails to match the hardcoded target constants, the sequence aborts and outputs the exact failing Modbus address.

## Key Configurations

- **Baud Rate:** 19200
- **Parity:** 8E1 (8 Data Bits, Even Parity, 1 Stop Bit)
- **Target Device:** Eaton D64RP410 Ground Fault Relay

### Modbus Map Utilized

- **Register 32 (Length 16):** Read Serial Number
- **Register 32101 (Length 1):** Write/Read Operating Mode
- **Register 32107 (Length 2):** Write/Read Residual Operating Current (IΔn)

### Little-Endian / Big-Endian Handling

Modbus integers are reassembled using bitwise shifting:

- **16-bit (Mode):** `(response[3] << 8) | response[4]`
- **32-bit (Current):** `(response[3] << 24) | (response[4] << 16) | (response[5] << 8) | response[6]`

## Documentation

For more detailed information regarding the implementation, refer to the following documents:

- [Technical Documentation](TECHNICAL_DOCS.md): Covers the software architecture, POUs, and routing.
- [Troubleshooting & Bugs](TROUBLESHOOTING.md): Details common issues faced during development (watchdog resets, memory issues, hardware conflicts) and their engineering solutions.
