FUNCTION_BLOCK WriteOutputs_FB
VAR_INPUT
	Execute : bool;
END_VAR

VAR_OUTPUT
	Done : bool;
	Error : int;
	Busy : bool;
END_VAR
/* ================================================================
 * D64 Modbus RTU - Write Outputs (Non-Blocking)
 * ================================================================ */

#include <Arduino.h>

// -- Internal State Variables --
static uint8_t  wo_state = 0;
static uint32_t wo_timer = 0;
static bool     wo_latched = false;

// -- Local CRC Calculator --
// Declared static so it doesn't conflict with ReadInputs_FB during linking
static uint16_t calcCRC_Write(uint8_t *buf, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (uint8_t i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

// Called once when the block is initialized
void setup() {
    // Note: Serial2 is already initialized by ReadInputs_FB.
    // We just ensure our state variables are clean.
    wo_state = 0;
    wo_latched = false;
}

// Called at every PLC scan cycle
void loop() {
    // The Execute, Busy, Done, and Error variables are OpenPLC macros
    // and can ONLY be used directly inside setup() or loop().

    // Detect rising edge on Execute input
    if (Execute && !wo_latched) {
        wo_latched = true;
        wo_state = 1;
        Busy = true;
        Done = false;
        Error = 0;
    }

    switch (wo_state) {

        case 0: { // -- IDLE --
            Busy = false;
            if (!Execute) wo_latched = false;
            break;
        }

        case 1: { // -- WRITE OPERATING MODE (32101 = 1) --
            uint8_t req[11];
            req[0] = 173;
            req[1] = 0x10; // Write Multiple Registers
            req[2] = 0x7D; req[3] = 0x65; // Register 32101
            req[4] = 0x00; req[5] = 0x01; // 1 Register
            req[6] = 0x02; // 2 Bytes
            req[7] = 0x00; req[8] = 0x01; // Value 1
            
            uint16_t crc = calcCRC_Write(req, 9);
            req[9]  = crc & 0xFF;
            req[10] = crc >> 8;

            // Clear the RX buffer of any stray bytes before sending
            while(Serial2.available()) Serial2.read();

            Serial2.write(req, 11);
            wo_timer = millis();
            wo_state = 2;
            break;
        }

        case 2: { // -- WAIT FOR MODE RESPONSE --
            // Modbus Function 16 response is always 8 bytes
            if (Serial2.available() >= 8) {
                wo_state = 3;
            } else if (millis() - wo_timer > 600) {
                Error = 1;
                wo_state = 99;
            }
            break;
        }

        case 3: { // -- WRITE RESIDUAL OPERATING CURRENT (32107 = 30) --
            uint8_t req[13];
            req[0] = 173;
            req[1] = 0x10;
            req[2] = 0x7D; req[3] = 0x6B; // Register 32107
            req[4] = 0x00; req[5] = 0x02; // 2 Registers
            req[6] = 0x04; // 4 Bytes
            req[7] = 0x00; req[8] = 0x00;
            req[9] = 0x00; req[10] = 0x1E; // Value 30
            
            uint16_t crc = calcCRC_Write(req, 11);
            req[11] = crc & 0xFF;
            req[12] = crc >> 8;

            while(Serial2.available()) Serial2.read();

            Serial2.write(req, 13);
            wo_timer = millis();
            wo_state = 4;
            break;
        }

        case 4: { // -- WAIT FOR IΔn RESPONSE --
            if (Serial2.available() >= 8) {
                Done = true;
                Error = 0;
                wo_state = 99;
            } else if (millis() - wo_timer > 600) {
                Error = 2;
                wo_state = 99;
            }
            break;
        }

        case 99: { // -- FINISH / ERROR STATE --
            Busy = false;
            // Wait for the user's TriggerWrite variable to go back to FALSE
            // before allowing another execution.
            if (!Execute) {
                wo_latched = false;
                wo_state = 0;
            }
            break;
        }
    }
}

END_FUNCTION_BLOCK