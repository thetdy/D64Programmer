# Eaton D64RP410 Ground Fault Relay Programmer

This project implements a custom **Modbus RTU Master** designed to simultaneously configure up to 50 **Eaton D64RP410 Ground Fault Relays** over an RS-485 daisy-chain network.

## Hardware Platform

- **Microcontroller:** ESP32 (e.g., Generic or WROVER)
- **Networking:** RS-485 via TTL-to-RS485 module (with auto flow control)
- **Bus Power:** 24V bus harness

## Software Platform

- **Runtime:** OpenPLC
- **Architecture:** Hybrid C/C++ Function Blocks and IEC 61131-3 Structured Text

## Purpose

The primary goal of this project is to replace the manual configuration process (previously using QModbus desktop software) with an embedded, one-button programming harness. The device automates the process of:

1. Scanning addresses 100-199 for active nodes.
2. Checking for duplicate serial numbers to prevent bus collisions.
3. Programming the relays to specific hardcoded values:
   - **Operating Mode:** 1
   - **Residual Current:** 30mA
4. Running a verification audit to ensure successful configuration.

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
