#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <fltUser.h>
#include "../../Caronte/Caronte/Caronte.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "fltlib.lib")
#pragma comment(lib,"ws2_32.lib") //Winsock Library

WSADATA wsa;
SOCKET s;
struct sockaddr_in server;

int createSocket() {

	printf("Initialising Winsock... \n");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code: %d", WSAGetLastError());
		return 0;
	}

	printf("Initialised.\n");

	//Create a socket
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d", WSAGetLastError());
		return 0;
	}

	printf("Socket created.\n");
	return 1;
}

int serverConnect() {

	inet_pton(AF_INET, "192.168.16.109", &(server.sin_addr));
	server.sin_family = AF_INET;
	server.sin_port = htons(12345);

	//Connect to remote server
	if (connect(s, (struct sockaddr*) & server, sizeof(server)) == 0)
	{
		printf("Connect error \n");
		return 0;
	}

	printf("Connected");
	return 1;
}

int sendData(char *message) {

	//Send some data
	//message = "GET / HTTP / 1.1\r\n\r\n";
	if (send(s, message, strlen(message), 0) == 0)
	{
		printf("Send failed \n");
		return 0;
	}

	printf("Data Send \n");
	return 1;
}


int main(void) {

	int state = 0; 
	HANDLE portHandle = NULL;
	HRESULT result;
	int qta = 0;
	char strToSend[1000];

	CARONTE_RECORD *record;
	PCARONTE_MESSAGE message;

	printf("----- Virgilio ----- \n");

	record = (CARONTE_RECORD*)malloc(sizeof(CARONTE_RECORD));
	message = (CARONTE_MESSAGE*)malloc(sizeof(CARONTE_MESSAGE));

	createSocket();
	serverConnect();

	while (1) {

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

				result = FilterGetMessage(portHandle, &message->MessageHeader, sizeof(CARONTE_MESSAGE), NULL);
				RtlCopyMemory(record, &message->CaronteRecord, sizeof(CARONTE_RECORD));

				snprintf(strToSend,
					1300,
					"{record_id: %lu, start_time: %llu, completion_time: %llu, is_kernel: %d, op_type: %d, file_path: \"%ws\", start_size: %lu, completion_size: %lu, thread_id: %llu, process_id: %llu, write_len: %lu}",
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
					record->WriteLen
					//calcolo entropia
				);
				sendData(strToSend);

				if (qta == 15) {
					printf("\n%-10s | %-20s | %-6s | %-10s | %-27s | %-10s | %-10s | %-20s | %-20s | %-10s |\n", "ID", "TIME START", "KERNEL", "OPERATION", "(VOL.) PATH", "SIZE START", "SIZE END", "THREAD ID", "PROCESS ID", "WRITE SIZE");
					printf("---------- | -------------------- | ------ | ---------- | --------------------------- | ---------- | ---------- | -------------------- | -------------------- | ---------- |\n");
					qta = 0;
				}
				
				printf("%010lu | %020llu | %06d | 0x%08x | (    ) %.20ws | %010lu | %010lu | %020llu | %020llu | %010lu |\n",
					record->RecordID,
					record->StartTime,
					//record->CompletionTime,
					record->IsKernel,
					record->OperationType,
					record->FilePath,
					record->StartFileSize,
					record->CompletionFileSize,
					record->ThreadId,
					record->ProcessId,
					record->WriteLen);

				qta++;

				break;

		}
	}

	return 0;
}

