PROGRAM main
VAR
	ReadInputs : READINPUTS_FB;
	TriggerScan : bool; (* Toggle this to TRUE in debugger to start the sweep *)
	ScanDone : bool; (* Goes TRUE when the sweep hits address 199 *)
	TotalNodes : int; (* Will show how many D64s were actually found *)
	DuplicateFound : bool; (* Will flag TRUE if two nodes share the same serial *)
	i : int; (* Used for the FOR loop *)
	j : int; (* Used for the nested FOR loop *)
	WriteOutputs : WRITEOUTPUTS_FB;
	TriggerWrite : bool;
	WriteDone : bool;
	WriteError : int;
END_VAR

// ==============================================================================
// 1. EXECUTE THE NETWORK SCANNER
// ==============================================================================
ReadInputs(startScan := TriggerScan);

// Pull the status variables out of the block so we can see them in the debugger
ScanDone   := ReadInputs.scanComplete;
TotalNodes := ReadInputs.nodeCount;

// ==============================================================================
// 2. PROCESS THE ARRAY DATA (Run once when scan finishes)
// ==============================================================================
IF ScanDone THEN
    
    // Auto-reset the trigger so it doesn't immediately scan again
    TriggerScan := FALSE;
    
    // Check for duplicate Serial Numbers across the network
    DuplicateFound := FALSE;
    
    // Only check if we found more than 1 device
    IF TotalNodes > 1 THEN
        // Standard nested FOR loop to compare every serial to every other serial
        FOR i := 0 TO (TotalNodes - 2) DO
            FOR j := (i + 1) TO (TotalNodes - 1) DO
                // If two strings match, we have a conflict!
                IF ReadInputs.nodeSerials[i] = ReadInputs.nodeSerials[j] THEN
                    DuplicateFound := TRUE;
                END_IF;
            END_FOR;
        END_FOR;
    END_IF;
    
END_IF;

// ==============================================================================
// 3. WRITE OUTPUTS (Left exactly as is)
// ==============================================================================
WriteOutputs(Execute := TriggerWrite);

WriteDone  := WriteOutputs.Done;
WriteError := WriteOutputs.Error;

// Auto-reset trigger after successful write
IF WriteDone AND (WriteError = 0) THEN
    TriggerWrite := FALSE;
END_IF;

END_PROGRAM