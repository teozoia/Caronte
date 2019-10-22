#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <fltUser.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <psapi.h>
#include "../../Caronte/Caronte/Caronte.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "fltlib.lib")
#pragma comment(lib,"ws2_32.lib") //Winsock Library

WSADATA wsa;
SOCKET s;
struct sockaddr_in server;
boolean spawned = false;
char virgilioClientAddr[] = "255.255.255.255";
int virgilioClientPort = 12345;

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

	inet_pton(AF_INET, virgilioClientAddr, &(server.sin_addr));
	server.sin_family = AF_INET;
	server.sin_port = htons(virgilioClientPort);

	//Connect to remote server
	int err = connect(s, (struct sockaddr*) & server, sizeof(server));
	if (err != 0)
	{
		printf("Connect error %d\n",err);
		return 0;
	}

	printf("Connected\n");
	return 1;
}

int sendData(char *message) {

	int err = send(s, message, strlen(message), 0);

	if ( err <= 0)
	{
		printf("Send failed %d\n", err);
		return 0;
	}

	return 1;
}

double entropy(unsigned char buffer[], unsigned long size) {

	unsigned char c;
	double h = .0;
	int count[256] = { 0 };
	double sum = 0;
	int top = 0;

	if (size > 1000000)
		top = 1000000;
	else
		top = (int)size;

	for (int i = 0; i < top; i++) {
		c = buffer[i];
		count[c]++;
		sum = sum + 1;
	}

	for (int i = 0; i < 256; i++) {

		if (count[i] > 0)
			h -= ((double)count[i] / sum) * log2((double)count[i] / sum);
	}

	return h * log(2.0) / log(sum);
	
}

bool spwanCond(int count) {

	if (count > 1000) {
		return true;
	}
	return false;
}


int main(int argc, char *argv[]) {

	int state = 0; 
	HANDLE portHandle = NULL;
	HRESULT result;
	char strToSend[3000];
	CHAR null[4] = "null";
	double h;
	TCHAR placeHolder[] = "none";
	TCHAR nameProc[1000];
	TCHAR nameModule[100];
	HANDLE Handle;
	int count = 0;
	ULONGLONG maliciousProcess = -1;
	ULONGLONG maliciousThread = -1;
	int gt = 0;

	CARONTE_RECORD *record;
	PCARONTE_MESSAGE message;

	printf("----- Virgilio ----- \n");

	if (argc < 3) {
		printf("usage: Virgilio.exe <ip_addr> <port>");
		return 0;
	}
	
	RtlCopyMemory(virgilioClientAddr, argv[1], strlen(virgilioClientAddr));
	virgilioClientPort = atoi(argv[2]);

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
				
				if (result == 0x80070006) {
					FilterClose(portHandle);
					portHandle = NULL;
					return 0;
				}

				RtlCopyMemory(record, &message->CaronteRecord, sizeof(CARONTE_RECORD));

				// Entropy -----------------------------------
				if (record->WriteLen > 0)
					h = entropy(record->WriteBuffer, record->WriteLen);
				else
					h = .0;
				// Read executable path
				Handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,FALSE, record->ProcessId);
				if (GetProcessImageFileName(Handle, nameProc, sizeof(nameProc) / sizeof(*nameProc)) == 0) {
					RtlCopyMemory(nameProc, placeHolder, sizeof(placeHolder));
				}
				// Read process name
				if (GetModuleBaseNameA(Handle, NULL, nameModule, sizeof(nameProc) / sizeof(*nameProc)) == 0) {
					RtlCopyMemory(nameModule, placeHolder, sizeof(placeHolder));
				}
				// Fill the gound through
				if (maliciousProcess == record->ProcessId || maliciousThread == record->ThreadId)
					gt = 1;
				else
					gt = 0;

				snprintf(strToSend,
					3000,
					"%lu;%llu;%llu;%d;%x;%ws;%lu;%lu;%llu;%llu;%lu;%lf;%s;%s;%d\n",
					record->RecordID,
					record->StartTime,
					record->CompletionTime - record->StartTime,
					record->IsKernel,
					record->OperationType,
					record->FilePath,
					record->StartFileSize,
					record->CompletionFileSize - record->StartFileSize,
					record->ThreadId,
					record->ProcessId,
					record->WriteLen,
					h,
					nameProc,
					nameModule,
					gt
				);
				sendData(strToSend);

				count++;

				printf("Record: %lu \n", record->RecordID);

				break;

		}

		// Process spawn
		if (!spawned && spwanCond(count)) {

			STARTUPINFO info = { sizeof(info) };
			PROCESS_INFORMATION processInfo;
			char spawnPath[] = "C:\\Users\\win-test\\Desktop\\start\\target.exe";
			spawned = true;

			if (CreateProcess(spawnPath, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &info, &processInfo)) {

				printf("PROCESS SPAWNED\n");
				printf("ProcessId=%d ThreadId=%d\n", processInfo.dwProcessId, processInfo.dwThreadId);

				maliciousProcess = processInfo.dwProcessId;
				maliciousThread = processInfo.dwThreadId;
			}
			else {
				printf("SPAWN FAILED\n");
			}

		}

	}

	return 0;
}

