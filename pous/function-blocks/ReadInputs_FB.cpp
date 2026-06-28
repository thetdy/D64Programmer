FUNCTION_BLOCK ReadInputs_FB
VAR_OUTPUT
	serialNumber : string; (* Will show the full serial number in debugger *)
	operatingMode : int;
	residualCurrent : dint;
	commsOK : bool; (* TRUE when we successfully read from D64 *)
	lastError : int; (* 0 = OK, other = error code (for future use *)
END_VAR
/* ================================================================
 * D64 Modbus RTU - Read Serial, Mode & Residual Current
 * ================================================================ */

#include <Arduino.h> 

uint8_t state = 0;
uint32_t state_timer = 0;
uint8_t request[8];
uint8_t response[64];
uint8_t rx_idx = 0;
const uint8_t SLAVE_ADDR = 173;

uint16_t calcCRC(uint8_t *buf, uint16_t len) {
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

void setup() {
    Serial.begin(115200);
    Serial2.begin(19200, SERIAL_8E1, 16, 17);
    
    commsOK = false;
    lastError = 0;
    operatingMode = 0; 
    residualCurrent = 0; // Initialize the new variable
}

void loop() {
    switch (state) {
        
        case 0: { // -- 1. REQUEST SERIAL NUMBER --
            request[0] = SLAVE_ADDR;
            request[1] = 0x03; 
            request[2] = 0x00; request[3] = 0x20; // Reg 32
            request[4] = 0x00; request[5] = 0x10; // 16 Regs
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 1;              
            break;
        } 

        case 1: { // -- 2. WAIT FOR SERIAL NUMBER --
            while (Serial2.available() && rx_idx < 64) {
                response[rx_idx++] = Serial2.read();
            }

            if (rx_idx >= 5 && response[1] == 0x83) { 
                commsOK = false;
                lastError = response[2]; 
                state_timer = millis();
                state = 6; // Abort to sleep
                break;
            }

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    commsOK = true;
                    lastError = 0;
                    
                    uint8_t byteCount = response[2];
                    if (byteCount > 32) byteCount = 32; 
                    
                    serialNumber.len = byteCount;
                    for (uint8_t i = 0; i < byteCount; i++) {
                        serialNumber.body[i] = response[3 + i];
                    }
                    serialNumber.body[byteCount] = '\0'; 
                    
                    state_timer = millis(); 
                    state = 2; // Move to Operating Mode
                    break; 
                }
            }

            if (millis() - state_timer > 600) { 
                commsOK = false;
                lastError = 1; 
                state_timer = millis();
                state = 6; 
            }
            break;
        } 

        case 2: { // -- 3. REQUEST OPERATING MODE --
            request[0] = SLAVE_ADDR;
            request[1] = 0x03; 
            request[2] = 0x7D; request[3] = 0x65; // Reg 32101
            request[4] = 0x00; request[5] = 0x01; // 1 Reg (2 bytes)
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 3;              
            break;
        }

        case 3: { // -- 4. WAIT FOR OPERATING MODE --
            while (Serial2.available() && rx_idx < 64) {
                response[rx_idx++] = Serial2.read();
            }

            if (rx_idx >= 5 && response[1] == 0x83) {
                commsOK = false;
                lastError = response[2]; 
                state_timer = millis();
                state = 6; 
                break;
            }

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    commsOK = true;
                    lastError = 0;
                    
                    operatingMode = (response[3] << 8) | response[4];
                    
                    state_timer = millis(); 
                    state = 4; // Move to Residual Current
                    break; 
                }
            }

            if (millis() - state_timer > 600) {
                commsOK = false;
                lastError = 1; 
                state_timer = millis();
                state = 6; 
            }
            break;
        }

        case 4: { // -- 5. REQUEST RESIDUAL CURRENT --
            request[0] = SLAVE_ADDR;
            request[1] = 0x03; 
            request[2] = 0x7D; request[3] = 0x6B; // Reg 32107
            request[4] = 0x00; request[5] = 0x02; // 2 Regs (4 bytes)
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 5;              
            break;
        }

        case 5: { // -- 6. WAIT FOR RESIDUAL CURRENT --
            while (Serial2.available() && rx_idx < 64) {
                response[rx_idx++] = Serial2.read();
            }

            if (rx_idx >= 5 && response[1] == 0x83) {
                commsOK = false;
                lastError = response[2]; 
                state_timer = millis();
                state = 6; 
                break;
            }

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    commsOK = true;
                    lastError = 0;
                    
                    // Reassemble the 32-bit DINT from four 8-bit bytes
                    residualCurrent = (response[3] << 24) | (response[4] << 16) | (response[5] << 8) | response[6];
                    
                    state_timer = millis(); 
                    state = 6; // SUCCESS! Move to sleep
                    break; 
                }
            }

            if (millis() - state_timer > 600) {
                commsOK = false;
                lastError = 1; 
                state_timer = millis();
                state = 6; 
            }
            break;
        }

        case 6: { // -- 7. DELAY BETWEEN POLLS --
            if (millis() - state_timer > 3000) {
                state = 0; // Restart cycle
            }
            break;
        } 
    }
}

END_FUNCTION_BLOCK