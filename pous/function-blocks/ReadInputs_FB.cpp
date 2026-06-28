FUNCTION_BLOCK ReadInputs_FB
VAR_INPUT
	startScan : bool;
END_VAR

VAR_OUTPUT
	scanComplete : bool;
	nodeCount : int;
	nodeAddresses : ARRAY [0..4] OF INT;
	nodeSerials : ARRAY [0..4] OF STRING;
	nodeModes : ARRAY [0..4] OF INt;
	nodeCurrents : ARRAY [0..4] OF DINT;
END_VAR
/* ================================================================
 * D64 Modbus RTU - 50-Node Network Scanner
 * ================================================================ */

#include <Arduino.h> 

// -- Scanner State Variables --
static uint8_t  state = 0;
static uint32_t state_timer = 0;
static uint8_t  request[8];
static uint8_t  response[64];
static uint8_t  rx_idx = 0;

static uint8_t  current_addr = 100;
static uint8_t  found_idx = 0;
static bool     scan_latched = false;

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
    scan_latched = false;
    state = 0; // Start in IDLE
    
    Serial.println("\n=== D64 Network Scanner Initialized ===");
}

void loop() {
    
    // Trigger detection
    if (startScan && !scan_latched) {
        scan_latched = true;
        scanComplete = false;
        current_addr = 100; // Reset to start of Eaton range
        found_idx = 0;      // Reset found counter
        nodeCount = 0;
        state = 1;
        Serial.println("-> Starting Network Sweep (100-199)...");
    }
    
    if (!startScan) {
        scan_latched = false;
    }

    switch (state) {
        case 0: { // -- IDLE --
            // Waiting for startScan to go TRUE
            break;
        }

        case 1: { // -- REQUEST SERIAL NUMBER (Ping) --
            request[0] = current_addr;
            request[1] = 0x03; 
            request[2] = 0x00; request[3] = 0x20; // Reg 32
            request[4] = 0x00; request[5] = 0x10; // 16 Regs
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 2;              
            break;
        } 

        case 2: { // -- WAIT FOR SERIAL NUMBER --
            while (Serial2.available() && rx_idx < 64) {
                response[rx_idx++] = Serial2.read();
            }

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    
                    // DEVICE FOUND! Save Serial to Array (Direct Pointer Access)
                    uint8_t byteCount = response[2];
                    if (byteCount > 32) byteCount = 32; 
                    
                    nodeSerials[found_idx].len = byteCount;
                    for (uint8_t i = 0; i < byteCount; i++) {
                        nodeSerials[found_idx].body[i] = response[3 + i];
                    }
                    nodeSerials[found_idx].body[byteCount] = '\0'; 
                    
                    Serial.printf("Found D64 at Addr: %d\n", current_addr);
                    
                    state_timer = millis(); 
                    state = 3; // Proceed to read Mode
                    break; 
                }
            }

            // Fast Timeout (150ms). If no answer, node is absent. Skip it.
            if (millis() - state_timer > 150) { 
                state = 7; // Skip to next address limit check
            }
            break;
        } 

        case 3: { // -- REQUEST OPERATING MODE --
            request[0] = current_addr;
            request[1] = 0x03; 
            request[2] = 0x7D; request[3] = 0x65; // Reg 32101
            request[4] = 0x00; request[5] = 0x01; // 1 Reg
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 4;              
            break;
        }

        case 4: { // -- WAIT FOR OPERATING MODE --
            while (Serial2.available() && rx_idx < 64) {
                response[rx_idx++] = Serial2.read();
            }

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    // Save Mode to Array (Direct Pointer Access)
                    nodeModes[found_idx] = (response[3] << 8) | response[4];
                    state_timer = millis(); 
                    state = 5; 
                    break; 
                }
            }
            
            if (millis() - state_timer > 300) { state = 5; } // Timeout, move on
            break;
        }

        case 5: { // -- REQUEST RESIDUAL CURRENT --
            request[0] = current_addr;
            request[1] = 0x03; 
            request[2] = 0x7D; request[3] = 0x6B; // Reg 32107
            request[4] = 0x00; request[5] = 0x02; // 2 Regs
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF; request[7] = crc >> 8;

            while(Serial2.available()) Serial2.read(); 
            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 6;              
            break;
        }

        case 6: { // -- WAIT FOR RESIDUAL CURRENT --
            while (Serial2.available() && rx_idx < 64) {
                response[rx_idx++] = Serial2.read();
            }

            if (rx_idx >= 5 && response[1] == 0x03) {
                uint8_t expected_len = 5 + response[2]; 
                if (rx_idx >= expected_len) {
                    // Save Current to Array (Direct Pointer Access)
                    nodeCurrents[found_idx] = (response[3] << 24) | (response[4] << 16) | (response[5] << 8) | response[6];
                    
                    // Finalize this Node entry (Direct Pointer Access)
                    nodeAddresses[found_idx] = current_addr;
                    found_idx++;
                    nodeCount = found_idx;
                    
                    state_timer = millis(); 
                    state = 7; 
                    break; 
                }
            }

            if (millis() - state_timer > 300) { state = 7; } // Timeout, move on
            break;
        }

        case 7: { // -- NEXT ADDRESS & BOUNDARY CHECK --
            current_addr++;
            
            // Stop if we hit address 200 OR if we filled our 50-node array
            if (current_addr > 199 || found_idx >= 50) {
                Serial.printf("-> Sweep Complete. Found %d devices.\n", found_idx);
                scanComplete = true;
                state = 0; // Return to idle
            } else {
                state = 1; // Ping the next address
            }
            break;
        } 
    }
}

END_FUNCTION_BLOCK