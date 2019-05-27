typedef struct _CARONTE_RECORD {

	ULONG RecordID;
	ULONGLONG StartTime;
	ULONGLONG CompletionTime;
	BOOLEAN IsKernel;

	ULONG OperationType;
	CHAR TargetVolume[5];
	CHAR FilePath[1000];
	LONGLONG StartFileSize;
	LONGLONG CompletionFileSize;

	ULONGLONG ThreadId;
	ULONGLONG ProcessId;

	ULONG WriteLen;
	CHAR WriteBuffer[1000000]; // 1MB of raw data write

} CARONTE_RECORD, * PCARONTE_RECORD;

typedef struct _CARONTE_MESSAGE {

	FILTER_MESSAGE_HEADER MessageHeader;
	CARONTE_RECORD CaronteRecord;

} CARONTE_MESSAGE, * PCARONTE_MESSAGE;

