FUNCTION_BLOCK ReadInputs_FB
VAR_OUTPUT
	serialNumber : string; (* Will show the full serial number in debugger *)
	commsOK : bool; (* TRUE when we successfully read from D64 *)
	lastError : int; (* 0 = OK, other = error code (for future use *)
END_VAR
/* ================================================================
 * D64 Modbus RTU - Read Serial Number (VERBOSE DEBUGGING)
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
    // UART2 on ESP32: RX=16, TX=17
    Serial2.begin(19200, SERIAL_8E1, 16, 17);
    
    commsOK = false;
    lastError = 0;
    
    Serial.println("\n=== D64 Modbus FB Initialized ===");
    Serial.println("Waiting for scan cycle to begin...");
}

void loop() {
    switch (state) {
        
        case 0: { 
            Serial.println("-> Sending Modbus Request...");
            request[0] = SLAVE_ADDR;
            request[1] = 0x03; 
            request[2] = 0x00;
            request[3] = 0x20; // Register 32
            request[4] = 0x00;
            request[5] = 0x10; // 16 registers
            
            uint16_t crc = calcCRC(request, 6);
            request[6] = crc & 0xFF;
            request[7] = crc >> 8;

            Serial2.write(request, 8);
            
            rx_idx = 0;             
            state_timer = millis(); 
            state = 1;              
            break;
        } 

        case 1: { 
            while (Serial2.available() && rx_idx < 64) {
                response[rx_idx++] = Serial2.read();
            }

            // Did we get an error/exception from the D64? (0x83 = Read Holding Reg Error)
            if (rx_idx >= 5 && response[1] == 0x83) {
                commsOK = false;
                lastError = response[2]; // The Modbus exception code
                Serial.printf("<- MODBUS EXCEPTION! Code: %d\n", lastError);
                state_timer = millis();
                state = 2;
                break;
            }

            // Normal successful header check
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
                    
                    Serial.printf("<- SUCCESS! Serial: %s\n", serialNumber.body);
                    
                    state_timer = millis(); 
                    state = 2; 
                    break; 
                }
            }

            // Timeout Check (600ms)
            if (millis() - state_timer > 600) {
                commsOK = false;
                lastError = 1; 
                
                Serial.printf("<- TIMEOUT. Bytes received: %d. Data: ", rx_idx);
                for(int i=0; i<rx_idx; i++) {
                    Serial.printf("%02X ", response[i]);
                }
                Serial.println();
                
                state_timer = millis();
                state = 2; 
            }
            break;
        } 

        case 2: { 
            if (millis() - state_timer > 3000) {
                state = 0; 
            }
            break;
        } 
    }
}

END_FUNCTION_BLOCK