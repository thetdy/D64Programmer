PROGRAM main
VAR
	ReadInputs: READINPUTS_FB;
	Start: BOOL;
	Programmer: D64PROGRAMMER_FB;
	LedBusy: BOOL AT %QX0.0;
	LedDone: BOOL AT %QX0.1;
	LedError: BOOL AT %QX0.2;
	Error: INT;
	NodesProgrammed: INT;
	FailedNode: INT;
	SetTargetCurrent: UDINT := 30;
	SetResponseDelay: UDINT := 0;
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