FUNCTION_BLOCK D64PROGRAMMER_FB
VAR_INPUT
  Execute : bool; (* Rising edge starts the whole sequence *)
END_VAR

VAR_OUTPUT
  Busy : bool; (* TRUE while scanning/programming *)
  Done : bool; (* TRUE when everything passes *)
  Error : bool; (* TRUE if any step fails *)
  ErrorCode : int; (* 1=Dup Serial, 2=Write Fail, 3=Verify Fail, 4=No Nodes Found *)
  TotalNodes : int; (* Total number of D64s found and programmed *)
  FailedNodeAddr : int; (* The specific Modbus address that caused an error *)
END_VAR
/* ================================================================
 * D64 Modbus RTU - Unified Sequencer (Scan -> Program -> Verify)
 * ================================================================ */

#include <Arduino.h> 

static uint8_t  state = 0;
static uint32_t state_timer = 0;
static uint8_t  request[16];
static uint8_t  response[64];
static uint8_t  rx_idx = 0;

static uint8_t  current_addr = 100;
static uint8_t  found_idx = 0;
static uint8_t  node_idx = 0; // Used to iterate through found nodes
static bool     seq_latched = false;

// Arrays to hold discovered nodes during the cycle
static uint8_t valid_nodes[50];
static char    internal_serials[50][33]; 

// -- Hardcoded Verification Targets --
const uint16_t TARGET_MODE = 1;
const uint32_t TARGET_CURRENT = 30;

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
    
    Busy = false;
    Done = false;
    Error = false;
    ErrorCode = 0;
    TotalNodes = 0;
    FailedNodeAddr = 0;
    seq_latched = false;
    state = 0; 
}

void loop() {
    // 1. Edge Detection & Initialization
    if (Execute && !seq_latched) {
        seq_latched = true;
        Busy = true;
        Done = false;
        Error = false;
        ErrorCode = 0;
        TotalNodes = 0;
        FailedNodeAddr = 0;
        
        current_addr = 100; 
        found_idx = 0;      
        state = 1; // Start Scan Phase
    }
    
    if (!Execute) seq_latched = false;

    switch (state) {
        case 0: break; // IDLE

        // ==========================================
        // PHASE 1: DISCOVERY & DUPLICATE CHECK
        // ==========================================
        case 1: { // REQUEST SERIAL
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

        case 2: { // WAIT FOR SERIAL
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
                    valid_nodes[found_idx] = current_addr;
                    found_idx++;
                    
                    state_timer = millis(); 
                    state = 3; 
                    break; 
                }
            }
            if (millis() - state_timer > 150) state = 3; // Timeout, node missing, skip
            break;
        } 

        case 3: { // NEXT ADDRESS / VALIDATE SCAN
            current_addr++;
            if (current_addr > 199 || found_idx >= 50) {
                
                if (found_idx == 0) {
                    ErrorCode = 4; // Error: No Nodes Found
                    state = 99;
                    break;
                }

                // Check Duplicates
                bool dup = false;
                if (found_idx > 1) {
                    for (int i = 0; i < found_idx - 1; i++) {
                        for (int j = i + 1; j < found_idx; j++) {
                            if (strcmp(internal_serials[i], internal_serials[j]) == 0) dup = true;
                        }
                    }
                }
                
                if (dup) {
                    ErrorCode = 1; // Error: Duplicate Serials
                    state = 99;
                } else {
                    TotalNodes = found_idx;
                    node_idx = 0;
                    state = 10; // Move to Programming Phase
                }
            } else {
                state = 1; // Ping next addr
            }
            break;
        }

        // ==========================================
        // PHASE 2: PROGRAMMING
        // ==========================================
        case 10: { // WRITE MODE (32101 = TARGET_MODE)
            request[0] = valid_nodes[node_idx];
            request[1] = 0x10; 
            request[2] = 0x7D; request[3] = 0x65; 
            request[4] = 0x00; request[5] = 0x01; 
            request[6] = 0x02; 
            request[7] = (TARGET_MODE >> 8) & 0xFF; 
            request[8] = TARGET_MODE & 0xFF; 
            
            uint16_t crc = calcCRC(request, 9);
            request[9] = crc & 0xFF; request[10] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 11);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 11;              
            break;
        }

        case 11: { // WAIT WRITE MODE
            if (Serial2.available() >= 8) {
                state = 12;
            } else if (millis() - state_timer > 600) {
                ErrorCode = 2; // Write Timeout
                FailedNodeAddr = valid_nodes[node_idx];
                state = 99;
            }
            break;
        }

        case 12: { // WRITE CURRENT (32107 = TARGET_CURRENT)
            request[0] = valid_nodes[node_idx];
            request[1] = 0x10; 
            request[2] = 0x7D; request[3] = 0x6B; 
            request[4] = 0x00; request[5] = 0x02; 
            request[6] = 0x04; 
            request[7] = (TARGET_CURRENT >> 24) & 0xFF; request[8] = (TARGET_CURRENT >> 16) & 0xFF; 
            request[9] = (TARGET_CURRENT >> 8) & 0xFF;  request[10] = TARGET_CURRENT & 0xFF; 
            
            uint16_t crc = calcCRC(request, 11);
            request[11] = crc & 0xFF; request[12] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 13);
            
            state_timer = millis(); 
            state = 13;              
            break;
        }

        case 13: { // WAIT WRITE CURRENT
            if (Serial2.available() >= 8) {
                node_idx++;
                if (node_idx >= found_idx) {
                    node_idx = 0;
                    state = 20; // Move to Verification Phase
                } else {
                    state = 10; // Program next node
                }
            } else if (millis() - state_timer > 600) {
                ErrorCode = 2; 
                FailedNodeAddr = valid_nodes[node_idx];
                state = 99;
            }
            break;
        }

        // ==========================================
        // PHASE 3: VERIFICATION
        // ==========================================
        case 20: { // READ MODE
            request[0] = valid_nodes[node_idx];
            request[1] = 0x03; 
            request[2] = 0x7D; request[3] = 0x65; 
            request[4] = 0x00; request[5] = 0x01; 
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 21;              
            break;
        }

        case 21: { // VERIFY MODE
            while (Serial2.available() && rx_idx < 64) response[rx_idx++] = Serial2.read();

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected = 5 + response[2]; 
                if (rx_idx >= expected) {
                    uint16_t read_mode = (response[3] << 8) | response[4];
                    if (read_mode != TARGET_MODE) {
                        ErrorCode = 3; // Verify Failed
                        FailedNodeAddr = valid_nodes[node_idx];
                        state = 99;
                    } else {
                        state = 22;
                    }
                    break;
                }
            }
            if (millis() - state_timer > 600) { ErrorCode = 2; FailedNodeAddr = valid_nodes[node_idx]; state = 99; }
            break;
        }

        case 22: { // READ CURRENT
            request[0] = valid_nodes[node_idx];
            request[1] = 0x03; 
            request[2] = 0x7D; request[3] = 0x6B; 
            request[4] = 0x00; request[5] = 0x02; 
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 23;              
            break;
        }

        case 23: { // VERIFY CURRENT
            while (Serial2.available() && rx_idx < 64) response[rx_idx++] = Serial2.read();

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected = 5 + response[2]; 
                if (rx_idx >= expected) {
                    uint32_t read_current = (response[3] << 24) | (response[4] << 16) | (response[5] << 8) | response[6];
                    if (read_current != TARGET_CURRENT) {
                        ErrorCode = 3; // Verify Failed
                        FailedNodeAddr = valid_nodes[node_idx];
                        state = 99;
                    } else {
                        node_idx++;
                        if (node_idx >= found_idx) {
                            Done = true; // ENTIRE PROCESS COMPLETE!
                            state = 99;
                        } else {
                            state = 20; // Verify next node
                        }
                    }
                    break;
                }
            }
            if (millis() - state_timer > 600) { ErrorCode = 2; FailedNodeAddr = valid_nodes[node_idx]; state = 99; }
            break;
        }

        // ==========================================
        // FINISH / ABORT
        // ==========================================
        case 99: { 
            Busy = false;
            if (ErrorCode > 0) Error = true;
            
            // Wait for user to release the Execute button
            if (!Execute) {
                seq_latched = false;
                state = 0;
            }
            break;
        } 
    }
}

END_FUNCTION_BLOCK