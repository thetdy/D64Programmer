PROGRAM main
VAR
	ReadInputs : READINPUTS_FB;
	Start : bool;
	Programmer : D64PROGRAMMER_FB;
	LedBusy : bool AT %QX0.0;
	LedDone : bool AT %QX0.1;
	LedError : bool AT %QX0.2;
	Code : int;
	NodesProgrammed : int;
	FailedNode : int;
END_VAR

// ==============================================================================
// 1. HARDWARE I/O READ
// ==============================================================================
// Call the block to run the C++ debounce logic
ReadInputs(BtnStart => Start);

// ==============================================================================
// 2. D64 MASTER PROGRAMMER LOGIC
// ==============================================================================
// Pass the debounced physical button directly into the sequencer's Execute pin
Programmer(Execute        := Start,
           Busy           => LedBusy,
           Done           => LedDone,
           Error          => LedError,
           ErrorCode      => Code,
           TotalNodes     => NodesProgrammed,
           FailedNodeAddr => FailedNode);

END_PROGRAM