#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <fltUser.h>
#include "../../Caronte/Caronte/Caronte.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "fltlib.lib")

int main(void) {

	// 0: Not connected
	// 1: Connected
	int state = 0; 
	HANDLE portHandle = NULL;
	HRESULT result;

	//void* rawBuffer;
	//DWORD dataSize = 1001072 + sizeof(FILTER_MESSAGE_HEADER); // 1MB + Header
	CARONTE_RECORD *record;
	PCARONTE_MESSAGE message;
	PCARONTE_REPLY reply;

	printf("----- Virgilio ----- \n");

	//rawBuffer = (void *)malloc(dataSize);
	record = (CARONTE_RECORD*)malloc(sizeof(CARONTE_RECORD));
	message = (CARONTE_MESSAGE*)malloc(sizeof(CARONTE_MESSAGE));
	reply = (CARONTE_REPLY*)malloc(sizeof(FILTER_REPLY_HEADER) + sizeof(CARONTE_STATUS));

	while (1) {

		printf("State: %d \n", state);

		switch (state) {

			case 0:
				printf("Connecting to Caronte... \n");
				result = FilterConnectCommunicationPort(L"\\Caronte", 0, NULL, 0, NULL, &portHandle);
				printf("Connected status:0x%08x \n", result);

				if (result == 0x0)
					state = 2;

				Sleep(5000);
				break;

			case 1:
				printf("Disconnecting... \n");
				FilterClose(portHandle);
				portHandle = NULL;
				state = 0;
				break;

			case 2:
				// Waiting for Caronte stream 801f0020
				printf("Waiting for msg \n");
				result = FilterGetMessage(portHandle, &message->MessageHeader, sizeof(CARONTE_MESSAGE), NULL);
				printf("Message recived:0x%08x \n", result);

				RtlCopyMemory(record, &message->CaronteRecord, sizeof(CARONTE_RECORD));
				//reply->Reply.Status = 1L;

				//result = FilterReplyMessage(portHandle, &reply->Header, sizeof(FILTER_REPLY_HEADER) + sizeof(CARONTE_STATUS));
				//printf("Result: %x \n",result);

				printf("ID \t Start \t End \t Kernel \t Operation \t Volume \t Path \t Size \t Final size \t TID \t PID \t Length \n");
				printf("---\t-------\t-----\t--------\t-----------\t--------\t------\t------\t------------\t-----\t-----\t--------\n");
				printf("%lu\t %llu \t %llu \t %d \t %x \t   \t %ws \t %lu \t %lu \t %llu \t %llu \t %lu",
					record->RecordID,
					record->StartTime,
					record->CompletionTime,
					record->IsKernel,
					record->OperationType,
					record->FilePath,
					record->StartFileSize,
					record->CompletionFileSize,
					record->ThreadId,
					record->ProcessId,
					record->WriteLen);

				break;

		}
	}

	return 0;
}

