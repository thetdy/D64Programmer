#!/bin/bash
# ==============================================================================
# OpenPLC v4.1.4 Build Environment & ESP32 Compilation Script
# Targeted for: Jules AI Agent Execution
# Target Hardware: ESP32 Generic [3.3.7]
# ==============================================================================

set -e # Exit immediately if a command exits with a non-zero status

# 1. Define Version Constants & Target Paths
OPENPLC_VERSION="v4.1.4"
TARGET_BOARD="esp32:esp32:generic" # Standard arduino-cli FQBN for Generic ESP32
PROJECT_DIR="$(pwd)"
IEC_PROGRAM="program.st" # Replace with your primary Structured Text file name
OUTPUT_C_DIR="$PROJECT_DIR/src_generated"
BUILD_DIR="$PROJECT_DIR/build_esp32"

echo "=== [1/5] Updating System Packages & Installing Dependencies ==="
sudo apt-get update -y
sudo apt-get install -y git build-essential bison flex python3 python3-pip curl

echo "=== [2/5] Fetching OpenPLC Editor Source Code (${OPENPLC_VERSION}) ==="
# Clone the specific version tag provided by the user
git clone --depth 1 --branch ${OPENPLC_VERSION} https://github.com /tmp/openplc-editor

echo "=== [3/5] Building matiec (IEC 61131-3 to C Compiler Toolchain) ==="
cd /tmp/openplc-editor/matiec
autoreconf -i
./configure
make -j$(nproc)
sudo make install
cd $PROJECT_DIR

echo "=== [4/5] Setting up Arduino CLI & ESP32 Core Packages ==="
# Download and install Arduino-CLI toolchain used by OpenPLC under the hood
curl -fsSL https://githubusercontent.com | sh
export PATH=$PATH:$PROJECT_DIR/bin

# Configure arduino-cli for ESP32 boards
bin/arduino-cli config init --overwrite
bin/arduino-cli config set board_manager.additional_urls https://githubusercontent.com
bin/arduino-cli core update-index

echo "Installing ESP32 core components..."
bin/arduino-cli core install esp32:esp32

echo "=== [5/5] Compiling Structured Text for Generic ESP32 ==="
if [ ! -f "$IEC_PROGRAM" ]; then
    echo "ERROR: Target file '$IEC_PROGRAM' not found in workspace directory."
    exit 1
fi

mkdir -p "$OUTPUT_C_DIR"
mkdir -p "$BUILD_DIR"

echo "Translating IEC 61131-3 Structured Text to C..."
# iec2c is the underlying compilation engine used inside OpenPLC
iec2c "$IEC_PROGRAM" -I /tmp/openplc-editor/matiec/lib -T "$OUTPUT_C_DIR"

echo "Cross-compiling C payload into ESP32 Binary..."
# Invokes cross-compilation targeting the generic ESP32 architecture hardware profile 
bin/arduino-cli compile --fqbn "$TARGET_BOARD" --output-dir "$BUILD_DIR" "$OUTPUT_C_DIR"

echo "=============================================================================="
echo " SUCCESS: OpenPLC ${OPENPLC_VERSION} logic successfully compiled for ESP32 Generic!"
echo " Binary payload artifact located at: ${BUILD_DIR}/"
echo "=============================================================================="
