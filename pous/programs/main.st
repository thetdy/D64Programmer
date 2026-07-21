PROGRAM main
VAR
	ReadInputs: READINPUTS_FB;
	Start: BOOL;
	Programmer: D64PROGRAMMER_FB;
	LedBusy: BOOL;
	LedDone: BOOL;
	LedError: BOOL;
	Error: INT;
	NodesProgrammed: INT;
	FailedNode: INT;
	SetTargetCurrent: UDINT := 30;
	SetResponseDelay: UDINT := 200;
	ErrorCodes: ERRORCODES_ENUM;
END_VAR

// ==============================================================================
// 1. HARDWARE I/O READ
// ==============================================================================
// Call the block to run the C++ debounce logic
ReadInputs(O_BtnStart => Start);

// ==============================================================================
// 2. D64 MASTER PROGRAMMER LOGIC
// ==============================================================================
// Pass the debounced physical button directly into the sequencer's Execute pin
Programmer(I_Execute := Start,
           I_TargetCurrent := SetTargetCurrent,
           I_ResponseDelay := SetResponseDelay,
           O_Busy => LedBusy,
           O_Done => LedDone,
           O_Error => LedError,
           O_ErrorCode => Error,
           O_TotalNodes => NodesProgrammed,
           O_FailedNodeAddr => FailedNode);

END_PROGRAM