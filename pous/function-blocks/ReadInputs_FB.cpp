FUNCTION_BLOCK ReadInputs_FB
VAR_OUTPUT
	BtnStart : bool; (* Debounced output for the Start sequence *)
END_VAR
/* ================================================================
 * C/C++ FUNCTION BLOCK : Hardware INIT & Debounce
 * ================================================================ */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>

// Called once when the block is initialized
void setup()
{
    // --- Configure Input Pins ---
    // Change '18' to whichever GPIO pin your physical button is wired to
    pinMode(18, INPUT_PULLDOWN);   // BtnStart 

    // --- Configure LED Output Pins ---
    // Change these to match your LED GPIO pins. 
    // Initializing them here prevents them from flashing randomly on boot.
    pinMode(19, OUTPUT);           // Busy LED 
    pinMode(21, OUTPUT);           // Done LED
    pinMode(22, OUTPUT);           // Error LED

    digitalWrite(19, LOW);
    digitalWrite(21, LOW);
    digitalWrite(22, LOW);

    printf("BoardINIT: Pins configured with software debounce\n");
}

// Called at every PLC scan cycle
void loop()
{
    static unsigned long lastDebounceTime = 0;
    static bool          lastButtonState  = false;
    static bool          buttonState      = false;

    // Read the raw physical pin
    bool raw = digitalRead(18);

    // Reset the debounce timer if the state changed (noise or actual press)
    if (raw != lastButtonState) {
        lastDebounceTime = millis();
    }

    // If the state has been stable longer than 50ms, lock it in
    if ((millis() - lastDebounceTime) > 50) {   
        if (raw != buttonState) {
            buttonState = raw;
        }
    }
    
    lastButtonState = raw;

    // Output the clean, debounced state to the OpenPLC engine
    BtnStart = buttonState;
}

END_FUNCTION_BLOCK