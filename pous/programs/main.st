PROGRAM main
VAR
	ReadInputs : READINPUTS_FB;
	WriteOutputs : WRITEOUTPUTS_FB;
	TriggerWrite : bool;
	WriteDone : bool;
	WriteError : int;
END_VAR

ReadInputs();

// One-shot write trigger
WriteOutputs(Execute := TriggerWrite);

WriteDone  := WriteOutputs.Done;
WriteError := WriteOutputs.Error;

// Optional: auto-reset trigger after successful write
IF WriteDone AND (WriteError = 0) THEN
    TriggerWrite := FALSE;
END_IF;

END_PROGRAM