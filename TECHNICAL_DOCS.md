# Technical Documentation

This document describes the software architecture of the custom Modbus RTU Master for the Eaton D64RP410 Ground Fault Relay Programmer.

## Software Architecture

The codebase relies on a strict separation of concerns, utilizing a hybrid architecture of C/C++ for low-level interactions and Structured Text (ST) for high-level logic routing. The project is split into three main Program Organization Units (POUs).

### A. READINPUTS_FB (Hardware I/O & Debouncer)

- **Language:** C/C++ Function Block.
- **Role:** Directly interfaces with the ESP32 hardware pins to bypass OpenPLC's default scanning lag for initialization.
- **Mechanism:**
  - Uses standard Arduino `pinMode()` in the `setup()` function to initialize the physical Start button (configured with `INPUT_PULLDOWN`) (the status LEDs have been removed in favor of the I2C LCD).
  - Implements a `millis()`-based 50ms software debounce to filter out electrical noise from the physical pushbutton.
  - Outputs a clean, rising-edge boolean signal to the OpenPLC engine.

### B. D64PROGRAMMER_FB (The Unified Sequencer)

- **Language:** C/C++ Function Block.
- **Role:** Acts as the core Modbus state machine and handles I2C LCD updates. It is a completely self-contained "black box" executing the programming cycle in three phases.
- **Phases:**
  - **Phase 1 (Discovery):** Pings Modbus addresses 100–199 using Function Code `0x03`. If a node replies, its serial number is parsed and saved to a hidden internal C++ array. The sequence checks for duplicate serial numbers to ensure no two relays share the same identifier (which would cause bus collisions).
  - **Phase 2 (Programming):** Iterates through valid nodes, sending Function Code `0x10` (Write Multiple Registers). It sets Register `32101` (Operating Mode) to `1`, and Register `32107` (Residual Current) to `30`.
  - **Phase 3 (Verification):** Audits every node by reading the registers back. If any node fails to match the hardcoded target constants, the sequence aborts and outputs the exact failing Modbus address.
- **Mechanism:**
  - Operates as a non-blocking `switch(state)` machine.
  - Because OpenPLC runs on a strict 20ms cyclic scan, this block fires a Modbus request, immediately exits the loop to allow the PLC engine to breathe, and checks for the serial response on subsequent 20ms scans.
  - Controls a DFRobot I2C 16x2 LCD via the `DFRobot_LCD` library to display the current state of the programming process (e.g., Scanning, Programming, Verifying, Success, or Error). The LCD is updated on state transitions rather than every cycle to minimize I2C bus congestion.

### C. main (Structured Text Routing)

- **Language:** IEC 61131-3 Structured Text (ST).
- **Role:** Acts as the wiring diagram holding the blocks together.
- **Mechanism:**
  - Maps physical hardware to OpenPLC's native addressing (e.g., `%IX0.0` for the Start button, internal variables for Busy, Done, and Error states).
  - Triggers the sequencer using the debounced button.
  - Routes the sequencer.s outputs directly to internal OpenPLC and OpenPLC debugger variables.
