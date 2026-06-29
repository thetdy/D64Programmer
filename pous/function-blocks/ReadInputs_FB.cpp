FUNCTION_BLOCK ReadInputs_FB
VAR_INPUT
	startScan : bool;
	verifyConfig : bool; (* If TRUE, checks if the nodes match the targets. If FALSE, just counts them and checks for serials *)
END_VAR

VAR_OUTPUT
	scanComplete : bool;
	nodeCount : int;
	duplicateFound : bool;
	configMismatch : bool; (* Turns TRUE if ANY node has the wrong mode or current *)
	failedNodeAddr : int; (* Tells you exactly which address failed verification *)
END_VAR
/* ================================================================
 * D64 Modbus RTU - Network Scanner & Verifier (Hardcoded Targets)
 * ================================================================ */

#include <Arduino.h> 

static uint8_t  state = 0;
static uint32_t state_timer = 0;
static uint8_t  request[8];
static uint8_t  response[64];
static uint8_t  rx_idx = 0;

static uint8_t  current_addr = 100;
static uint8_t  found_idx = 0;
static bool     scan_latched = false;

// -- Hardcoded Verification Targets --
const uint16_t TARGET_MODE = 1;
const uint32_t TARGET_CURRENT = 30;

// Hidden C++ memory just for the duplicate check
static char internal_serials[50][33]; 

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
    
    scanComplete = false;
    nodeCount = 0;
    duplicateFound = false;
    configMismatch = false;
    failedNodeAddr = 0;
    scan_latched = false;
    state = 0; 
}

void loop() {
    if (startScan && !scan_latched) {
        scan_latched = true;
        scanComplete = false;
        duplicateFound = false;
        configMismatch = false;
        failedNodeAddr = 0;
        current_addr = 100; 
        found_idx = 0;      
        nodeCount = 0;
        state = 1;
    }
    
    if (!startScan) scan_latched = false;

    switch (state) {
        case 0: break;

        case 1: { // 1. REQUEST SERIAL
            request[0] = current_addr;
            request[1] = 0x03; 
            request[2] = 0x00; request[3] = 0x20; 
            request[4] = 0x00; request[5] = 0x10; 
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 2;              
            break;
        } 

        case 2: { // 2. WAIT FOR SERIAL
            while (Serial2.available() && rx_idx < 64) response[rx_idx++] = Serial2.read();

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    
                    uint8_t byteCount = response[2];
                    if (byteCount > 32) byteCount = 32; 
                    
                    for (uint8_t i = 0; i < byteCount; i++) {
                        internal_serials[found_idx][i] = response[3 + i];
                    }
                    internal_serials[found_idx][byteCount] = '\0'; 
                    
                    state_timer = millis(); 
                    state = 3; 
                    break; 
                }
            }
            if (millis() - state_timer > 150) state = 7; // Skip if no response
            break;
        } 

        case 3: { // 3. REQUEST MODE
            request[0] = current_addr;
            request[1] = 0x03; 
            request[2] = 0x7D; request[3] = 0x65; 
            request[4] = 0x00; request[5] = 0x01; 
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 4;              
            break;
        }

        case 4: { // 4. READ & VERIFY MODE
            while (Serial2.available() && rx_idx < 64) response[rx_idx++] = Serial2.read();

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    
                    uint16_t read_mode = (response[3] << 8) | response[4];
                    
                    // ON-THE-FLY VERIFICATION WITH CONSTANT
                    if (verifyConfig && read_mode != TARGET_MODE) {
                        configMismatch = true;
                        failedNodeAddr = current_addr;
                    }

                    state_timer = millis(); 
                    state = 5; 
                    break; 
                }
            }
            if (millis() - state_timer > 300) state = 5; 
            break;
        }

        case 5: { // 5. REQUEST CURRENT
            request[0] = current_addr;
            request[1] = 0x03; 
            request[2] = 0x7D; request[3] = 0x6B; 
            request[4] = 0x00; request[5] = 0x02; 
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 6;              
            break;
        }

        case 6: { // 6. READ & VERIFY CURRENT
            while (Serial2.available() && rx_idx < 64) response[rx_idx++] = Serial2.read();

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    
                    uint32_t read_current = (response[3] << 24) | (response[4] << 16) | (response[5] << 8) | response[6];
                    
                    // ON-THE-FLY VERIFICATION WITH CONSTANT
                    if (verifyConfig && read_current != TARGET_CURRENT) {
                        configMismatch = true;
                        if (failedNodeAddr == 0) failedNodeAddr = current_addr; 
                    }
                    
                    found_idx++;
                    nodeCount = found_idx;
                    
                    state_timer = millis(); 
                    state = 7; 
                    break; 
                }
            }
            if (millis() - state_timer > 300) state = 7; 
            break;
        }

        case 7: { // 7. NEXT ADDRESS & DUPLICATE CHECK
            current_addr++;
            
            if (current_addr > 199 || found_idx >= 50) {
                
                duplicateFound = false;
                if (found_idx > 1) {
                    for (int i = 0; i < found_idx - 1; i++) {
                        for (int j = i + 1; j < found_idx; j++) {
                            if (strcmp(internal_serials[i], internal_serials[j]) == 0) {
                                duplicateFound = true;
                            }
                        }
                    }
                }
                
                scanComplete = true;
                state = 0; 
            } else {
                state = 1; 
            }
            break;
        } 
    }
}

END_FUNCTION_BLOCK