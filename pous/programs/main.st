PROGRAM main
VAR
	ReadInputs : READINPUTS_FB;
	WriteOutputs : WRITEOUTPUTS_FB;
	TriggerScan : bool;
	VerifyConfig : bool; (* Set TRUE to verify, FALSE to just count/check serials *)
	ScanDone : bool;
	TotalNodes : int;
	DuplicateFound : bool;
	ConfigMismatch : bool;
	FailedNodeAddr : int;
	TriggerWrite : bool;
	WriteDone : bool;
	WriteError : int;
END_VAR

// ==============================================================================
// 1. EXECUTE THE NETWORK SCANNER / VERIFIER
// ==============================================================================
ReadInputs(startScan := TriggerScan, verifyConfig := VerifyConfig);

// Pull the status variables out of the block so we can see them in the debugger
ScanDone       := ReadInputs.scanComplete;
TotalNodes     := ReadInputs.nodeCount;
DuplicateFound := ReadInputs.duplicateFound;
ConfigMismatch := ReadInputs.configMismatch;
FailedNodeAddr := ReadInputs.failedNodeAddr;

IF ScanDone THEN
    // Auto-reset the trigger so it doesn't immediately scan again
    TriggerScan := FALSE;
END_IF;

// ==============================================================================
// 2. WRITE OUTPUTS (Programming the D64s)
// ==============================================================================
WriteOutputs(Execute := TriggerWrite);

WriteDone  := WriteOutputs.Done;
WriteError := WriteOutputs.Error;

// Auto-reset trigger after successful write
IF WriteDone AND (WriteError = 0) THEN
    TriggerWrite := FALSE;
END_IF;

END_PROGRAM