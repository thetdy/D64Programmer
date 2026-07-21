# Troubleshooting and Development Notes

This document outlines key bugs encountered during the development of the Eaton D64RP410 Ground Fault Relay Programmer and the engineering solutions implemented to resolve them.

## Bug 1: Watchdog Resets from Blocking Code

* **Issue:** Initial code used standard Arduino `delay()` and `while()` loops (e.g., `while(millis() - timer < 600)`) to wait for Modbus responses. Because OpenPLC enforces a real-time 20ms cyclic scan, these blocking loops starved the ESP32’s FreeRTOS `loopTask`, triggering the hardware watchdog timer and causing continuous panic reboots.
* **Solution:** Completely rewrote the serial communications into a non-blocking state machine. Modbus TX occurs in one state, and RX polling happens across multiple PLC scan cycles without trapping the processor.

## Bug 2: FreeRTOS Stack Overflow via OpenPLC Strings

* **Issue:** Attempted to export an array of 50 serial numbers (`ARRAY [0..49] OF STRING`) from the Function Block to the OpenPLC ST debugger. The ESP32 instantly suffered a Guru Meditation Error (Stack canary watchpoint triggered).
* **Solution:** OpenPLC `STRING` variables reserve ~85 bytes each. 50 strings demanded over 4,200 bytes of contiguous memory, blowing out the strict 4KB-8KB FreeRTOS task stack allocated for OpenPLC. We abandoned storing the arrays in the PLC memory. Instead, we shifted to a **"Command and Verify"** pattern, using a hidden, native C++ static array (`static char internal_serials[50][33]`) strictly for internal duplicate checking, drastically reducing the memory footprint.

## Bug 3: OpenPLC MATIEC Compiler Scope Errors

* **Issue:** Received the error `'vars' was not declared in this scope` when trying to wrap logic inside a custom C++ function (`void WriteOutputs_FB()`).
* **Solution:** The MATIEC compiler translates OpenPLC interface variables (like `Execute` and `Busy`) into C++ macros that point to an internal `vars` pointer. This pointer is only injected into the template's `setup()` and `loop()` functions. Moving all custom functions into the main `loop()` scope resolved the pointer issues. Furthermore, local variables inside `switch` cases required explicit `{ }` scoping to prevent C++ `crosses initialization` errors.

## Bug 4: The "Ghost Characters" & Hardware Pin Collision

* **Issue:** The serial monitor displayed garbage characters (`dX`, `D~M`) mixed with text, and Modbus packets were timing out with 0 bytes received.
* **Solution:** Identified an ESP32 hardware conflict. The initial wiring used GPIO 16 and 17 for `Serial2`. On ESP32 **WROVER** boards, pins 16 and 17 are internally hardwired to the PSRAM chip. Serial traffic was colliding with memory allocation. The fix required swapping/remapping the RX/TX pins away from the PSRAM lanes and ensuring the RS-485 A/B wires were correctly oriented.

## Bug 5: Modbus Polling Spam (Edge Detection Failure)

* **Issue:** Because the PLC scans every 20ms, a physical button push lasting 500ms would cause the Modbus logic to evaluate `TRUE` 25 times in a row, flooding the RS-485 bus with duplicate write commands.
* **Solution:** Implemented a software latch (Rising Edge Detection: `if (Execute && !seq_latched)`). This ensures the Modbus state machine locks itself upon activation and runs exactly one time per physical button press.
