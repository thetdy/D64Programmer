# Agent Instructions

This repository contains the OpenPLC firmware for the D64 Programmer targeting an ESP32 Generic board.

## Available Tools

### Build Environment Setup (`jules_setup_env.sh`)

**Purpose**: Sets up the OpenPLC build environment, installs `matiec` (IEC 61131-3 to C compiler), downloads the `arduino-cli`, and compiles the Structured Text (`program.st`) into an ESP32 binary payload.

**Usage**: You can execute this script from the workspace root to perform a full build.
```bash
./jules_setup_env.sh
```

**Inputs**: None. It automatically reads `program.st` from the workspace root.

**Outputs**: 
- Generated C source files: `src_generated/`
- Compiled ESP32 binary payload: `build_esp32/`

## Project Structure

- `jules_setup_env.sh`: Automated environment setup and build script.
- `project.json`: Project configuration.
- `devices/`: Hardware configuration, including ESP32 pin mappings.
- `pous/`: Program Organization Units. Contains custom C++ function blocks (`pous/function-blocks/`) and standard Structured Text programs (`pous/programs/`).
- `program.st`: The primary entry point / Structured Text file compiled by the toolchain.

## Conventions

- Before running the build, ensure any changes to `pous/` are correctly referenced.
- When making changes to the hardware interface, update the C++ function blocks in `pous/function-blocks/` rather than modifying the generated C code directly.
