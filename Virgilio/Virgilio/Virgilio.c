#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <fltUser.h>
#include "../../Caronte/Caronte/Caronte.h"
#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include <stdio.h>


#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "fltlib.lib")

int main(void) {

	// 0: Not connected
	// 1: Connected
	int state = 0; 
	HANDLE portHandle = NULL;
	HRESULT result;
	int qta = 0;

	//void* rawBuffer;
	//DWORD dataSize = 1001072 + sizeof(FILTER_MESSAGE_HEADER); // 1MB + Header
	CARONTE_RECORD *record;
	PCARONTE_MESSAGE message;
	PCARONTE_REPLY reply;

	mongoc_client_t* client;
	mongoc_collection_t* collection;
	bson_error_t error;
	bson_oid_t oid;
	bson_t* doc;

	mongoc_init();

	client = mongoc_client_new("mongodb://192.168.16.110:27017/?appname=virgilio");
	collection = mongoc_client_get_collection(client, "test", "caronte");

	//doc = bson_new();
	//bson_oid_init(&oid, NULL);
	//BSON_APPEND_OID(doc, "_id", &oid);
	//BSON_APPEND_UTF8(doc, "hello", "world");

	

	printf("----- Virgilio ----- \n");

	//rawBuffer = (void *)malloc(dataSize);
	record = (CARONTE_RECORD*)malloc(sizeof(CARONTE_RECORD));
	message = (CARONTE_MESSAGE*)malloc(sizeof(CARONTE_MESSAGE));
	reply = (CARONTE_REPLY*)malloc(sizeof(FILTER_REPLY_HEADER) + sizeof(CARONTE_STATUS));

	while (1) {

		//printf("State: %d \n", state);

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
				//printf("Waiting for msg \n");
				result = FilterGetMessage(portHandle, &message->MessageHeader, sizeof(CARONTE_MESSAGE), NULL);
				//printf("Message recived:0x%08x \n", result);

				RtlCopyMemory(record, &message->CaronteRecord, sizeof(CARONTE_RECORD));
				//reply->Reply.Status = 1L;

				//result = FilterReplyMessage(portHandle, &reply->Header, sizeof(FILTER_REPLY_HEADER) + sizeof(CARONTE_STATUS));
				//printf("Result: %x \n",result); 4294967295

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

				doc = bson_new();
				bson_oid_init(&oid, NULL);

				BSON_APPEND_INT64(doc, "recordId", record->RecordID);
				BSON_APPEND_INT64(doc, "startTime", record->StartTime);
				BSON_APPEND_INT64(doc, "completionTime", record->CompletionTime);
				BSON_APPEND_BOOL(doc, "isKernel", record->IsKernel);
				BSON_APPEND_INT32(doc, "operationType", record->OperationType);
				BSON_APPEND_UTF8(doc, "filePath", record->FilePath);
				BSON_APPEND_INT64(doc, "startFileSize", record->StartFileSize);
				BSON_APPEND_INT64(doc, "completionFileSize", record->CompletionFileSize);
				BSON_APPEND_INT64(doc, "threadId", record->ThreadId);
				BSON_APPEND_INT64(doc, "processId", record->ProcessId);
				BSON_APPEND_INT64(doc, "writeLen", record->WriteLen);
				BSON_APPEND_VALUE(doc, "writeBuffer", record->WriteBuffer);

				if (!mongoc_collection_insert_one(
					collection, doc, NULL, NULL, &error)) {
					fprintf(stderr, "%s\n", error.message);
				}

				bson_destroy(doc);
				mongoc_collection_destroy(collection);
				mongoc_client_destroy(client);
				mongoc_cleanup();


				break;

		}
	}

	return 0;
}

