#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <ntddk.h>
#include <stdarg.h>
#include "Caronte.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
PFLT_PORT port = NULL;
PFLT_PORT ClientPort = NULL;
PSECURITY_DESCRIPTOR sd;
PVOID Cookie;
ULONG RecordID = 0;
CARONTE_RECORD record;

/*************************************************************************
	Pool Tags
*************************************************************************/

#define BUFFER_SWAP_TAG     'bdBS'
#define CONTEXT_TAG         'xcBS'
#define NAME_TAG            'mnBS'
#define PRE_2_POST_TAG      'ppBS'
#define LOG					1

/*************************************************************************
	Local structures
*************************************************************************/

//  This is a volume context, one of these are attached to each volume
//  we monitor.  This is used to get a "DOS" name for debug display.
typedef struct _VOLUME_CONTEXT {

	//  Holds the name to display
	UNICODE_STRING Name;

	//  Holds the sector size for this volume.
	ULONG SectorSize;

} VOLUME_CONTEXT, * PVOLUME_CONTEXT;

#define MIN_SECTOR_SIZE 0x200


//  This is a lookAside list used to allocate our pre-2-post structure.
NPAGED_LOOKASIDE_LIST Pre2PostContextList;

/* Typedef for ZwQueryInformationProcess */
typedef NTSTATUS(*ZWQUERYINFORMATIONPROCESS)
(__in HANDLE ProcessHandle,
	__in PROCESSINFOCLASS ProcessClassInfo,
	PVOID ProcessInformation,
	__in ULONG ProcessInformationLength,
	PULONG ReturnLenght);

/*************************************************************************
	Prototypes
*************************************************************************/

NTSTATUS
CaronteConnect(
	PFLT_PORT clientport,
	PVOID serverportcookie,
	PVOID context,
	ULONG size,
	PVOID connectioncookie
);

NTSTATUS
InstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

VOID
CleanupVolumeContext(
	_In_ PFLT_CONTEXT Context,
	_In_ FLT_CONTEXT_TYPE ContextType
);

NTSTATUS
InstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
FilterUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
PreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
PostWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_POSTOP_CALLBACK_STATUS
PostWriteWhenSafe(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

NTSTATUS
GetFileSize(
	_In_    PFLT_INSTANCE Instance,
	_In_    PFILE_OBJECT FileObject,
	_Out_   PLONGLONG Size
);

VOID KernPrint(PCSTR msg, ...);

//  Assign text sections for each routine.
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, InstanceSetup)
#pragma alloc_text(PAGE, CleanupVolumeContext)
#pragma alloc_text(PAGE, InstanceQueryTeardown)
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FilterUnload)
#endif

//  Operation we currently care about.
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

	{ IRP_MJ_WRITE,
	  0,
	  PreWrite,
	  PostWrite },

	{ IRP_MJ_OPERATION_END }
};

//  Context definitions we currently care about.  Note that the system will
//  create a lookAside list for the volume context because an explicit size
//  of the context is specified.
CONST FLT_CONTEXT_REGISTRATION ContextNotifications[] = {

	 { FLT_VOLUME_CONTEXT,
	   0,
	   CleanupVolumeContext,
	   sizeof(VOLUME_CONTEXT),
	   CONTEXT_TAG },

	 { FLT_CONTEXT_END }
};

//  This defines what we want to filter with FltMgr
CONST FLT_REGISTRATION FilterRegistration = {

	sizeof(FLT_REGISTRATION),           //  Size
	FLT_REGISTRATION_VERSION,           //  Version
	0,                                  //  Flags

	ContextNotifications,               //  Context
	Callbacks,                          //  Operation callbacks

	FilterUnload,                       //  MiniFilterUnload

	InstanceSetup,                      //  InstanceSetup
	InstanceQueryTeardown,              //  InstanceQueryTeardown
	NULL,                               //  InstanceTeardownStart
	NULL,                               //  InstanceTeardownComplete

	NULL,                               //  GenerateFileName
	NULL,                               //  GenerateDestinationFileName
	NULL                                //  NormalizeNameComponent

};

NTSTATUS CaronteConnect(PFLT_PORT clientport, PVOID serverportcookie, PVOID context, ULONG size, PVOID connectioncookie) {

	//size;
	ClientPort = clientport;
	Cookie = connectioncookie;
	KdPrint(("[Caronte][INFO] - Caronte connected. \n"));

	return STATUS_SUCCESS;
}

VOID CaronteDisconnect(PVOID connectioncookie) {

	FltCloseClientPort(gFilterHandle, &ClientPort);
	KdPrint(("[Caronte][INFO] - Caronte disconnected."));
}

NTSTATUS CaronteRecNotify(
	PVOID portcookie,
	PVOID inputbuffer,
	ULONG inputbufferlength,
	PVOID outputbuffer,
	ULONG outputbufferlength,
	PULONG retlength
) {


	KernPrint("[Caronte][INFO] - Message recived from Virgilio of size: %lu", inputbufferlength);
	return STATUS_SUCCESS;

}

NTSTATUS InstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
){
	PDEVICE_OBJECT devObj = NULL;
	PVOLUME_CONTEXT ctx = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	ULONG retLen;
	PUNICODE_STRING workingName;
	USHORT size;
	UCHAR volPropBuffer[sizeof(FLT_VOLUME_PROPERTIES) + 512];
	PFLT_VOLUME_PROPERTIES volProp = (PFLT_VOLUME_PROPERTIES)volPropBuffer;

	PAGED_CODE();

	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);
	UNREFERENCED_PARAMETER(VolumeFilesystemType);

	try {

		//  Allocate a volume context structure.
		status = FltAllocateContext(FltObjects->Filter, FLT_VOLUME_CONTEXT, sizeof(VOLUME_CONTEXT), NonPagedPool, &ctx);
		if (!NT_SUCCESS(status)) {

			KernPrint("[Caronte][ERR] - Could not allocate a context, quit. \n");
			leave;
		}

		//  Always get the volume properties, so I can get a sector size
		status = FltGetVolumeProperties(FltObjects->Volume, volProp, sizeof(volPropBuffer), &retLen);
		if (!NT_SUCCESS(status)) {

			KernPrint("[Caronte][ERR] - Could not get volume property. \n");
			leave;
		}

		//  Save the sector size in the context for later use.  Note that
		//  we will pick a minimum sector size if a sector size is not
		//  specified.
		FLT_ASSERT((volProp->SectorSize == 0) || (volProp->SectorSize >= MIN_SECTOR_SIZE));

		ctx->SectorSize = max(volProp->SectorSize, MIN_SECTOR_SIZE);

		//  Init the buffer field (which may be allocated later).
		ctx->Name.Buffer = NULL;

		//  Get the storage device object we want a name for.
		status = FltGetDiskDeviceObject(FltObjects->Volume, &devObj);

		if (NT_SUCCESS(status)) {

			//  Try and get the DOS name.  If it succeeds we will have
			//  an allocated name buffer.  If not, it will be NULL
			status = IoVolumeDeviceToDosName(devObj, &ctx->Name);
		}

		//  If we could not get a DOS name, get the NT name.
		if (!NT_SUCCESS(status)) {

			FLT_ASSERT(ctx->Name.Buffer == NULL);

			//  Figure out which name to use from the properties
			if (volProp->RealDeviceName.Length > 0) {

				workingName = &volProp->RealDeviceName;
			}
			else if (volProp->FileSystemDeviceName.Length > 0) {

				workingName = &volProp->FileSystemDeviceName;
			}
			else {

				//  No name, don't save the context
				status = STATUS_FLT_DO_NOT_ATTACH;
				leave;
			}

			//  Get size of buffer to allocate.  This is the length of the
			//  string plus room for a trailing colon.
			size = workingName->Length + sizeof(WCHAR);

			//  Now allocate a buffer to hold this name
#pragma prefast(suppress:__WARNING_MEMORY_LEAK, "[Caronte] ctx->Name.Buffer will not be leaked because it is freed in CleanupVolumeContext")
			ctx->Name.Buffer = ExAllocatePoolWithTag(NonPagedPool,
				size,
				NAME_TAG);
			if (ctx->Name.Buffer == NULL) {

				status = STATUS_INSUFFICIENT_RESOURCES;
				leave;
			}

			//  Init the rest of the fields
			ctx->Name.Length = 0;
			ctx->Name.MaximumLength = size;

			//  Copy the name in
			RtlCopyUnicodeString(&ctx->Name, workingName);

			//  Put a trailing colon to make the display look good
			RtlAppendUnicodeToString(&ctx->Name, L":");
		}

		//  Set the context
		status = FltSetVolumeContext(FltObjects->Volume, FLT_SET_CONTEXT_KEEP_IF_EXISTS, ctx, NULL);

		//  Log debug info
		KernPrint("[Caronte][ERR] - SectSize=0x%04x, SectSizeCtx=0x%04x, VolumeName=\"%wZ\"\n",
			&volProp->SectorSize,
			&ctx->SectorSize,
			&ctx->Name);

		//  It is OK for the context to already be defined.
		if (status == STATUS_FLT_CONTEXT_ALREADY_DEFINED)
			status = STATUS_SUCCESS;

	}
	finally{

		//  Always release the context.  If the set failed, it will free the
		//  context.  If not, it will remove the reference added by the set.
		//  Note that the name buffer in the ctx will get freed by the context
		//  cleanup routine.
		if (ctx)
			FltReleaseContext(ctx);

		//  Remove the reference added to the device object by
		//  FltGetDiskDeviceObject.
		if (devObj) 
			ObDereferenceObject(devObj);
	}

	return status;
}

// The given context is being freed. Free the allocated name buffer if there one.
// Context - The context being freed, ContextType - The type of context this is
VOID CleanupVolumeContext(
	_In_ PFLT_CONTEXT Context,
	_In_ FLT_CONTEXT_TYPE ContextType
){
	PVOLUME_CONTEXT ctx = Context;

	PAGED_CODE();

	UNREFERENCED_PARAMETER(ContextType);

	FLT_ASSERT(ContextType == FLT_VOLUME_CONTEXT);

	if (ctx->Name.Buffer != NULL) {

		ExFreePool(ctx->Name.Buffer);
		ctx->Name.Buffer = NULL;
	}
}

// This is called when an instance is being manually deleted by a call to FltDetachVolume or FilterDetach.
// We always return it is OK to detach.
NTSTATUS InstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
){
	PAGED_CODE();

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	return STATUS_SUCCESS;
}

// This is the initialization routine. This registers with FltMgr and initializes all global data structures.
// DriverObject - Pointer to driver object created by the system to represent this driver.
// RegistryPath - Unicode string identifying where the parameters for this driver are located in the registry.
NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
){
	NTSTATUS status;
	OBJECT_ATTRIBUTES oa = { 0 };
	UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Caronte");

	//  Default to NonPagedPoolNx for non paged pool allocations where supported.
	ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

	//  Init lookaside list used to allocate our context structure used to
	//  pass information from out preOperation callback to our postOperation
	//  callback.
	ExInitializeNPagedLookasideList(&Pre2PostContextList,
		NULL,
		NULL,
		0,
		sizeof(CARONTE_RECORD),
		PRE_2_POST_TAG,
		0);

	//  Register with FltMgr
	status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);

	if (!NT_SUCCESS(status)) {

		goto SwapDriverEntryExit;
	}

	//  Start communication
	status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
	KernPrint("[Caronte][INFO] - FltBuildDefaultDescriptor status=%x \n",status);

	if (!NT_SUCCESS(status)) {

		FltUnregisterFilter(gFilterHandle);
		goto SwapDriverEntryExit;
	}
	else {
		InitializeObjectAttributes(&oa, &name, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sd);
		status = FltCreateCommunicationPort(gFilterHandle, &port, &oa, NULL, CaronteConnect, CaronteDisconnect, CaronteRecNotify, 1);
		
		if (NT_SUCCESS(status)) {
			KernPrint("[Caronte][INFO] - FltCreateCommunicationPort status=%x \n", status);
		}
		else {
			KernPrint("[Caronte][ERR] - FltCreateCommunicationPort status=%x something wrong\n", status);
		}
		
	}

	//  Start filtering i/o
	status = FltStartFiltering(gFilterHandle);

	if (!NT_SUCCESS(status)) {

		FltUnregisterFilter(gFilterHandle);
		goto SwapDriverEntryExit;
	}

SwapDriverEntryExit:

	if (!NT_SUCCESS(status)) {

		ExDeleteNPagedLookasideList(&Pre2PostContextList);
		FltCloseCommunicationPort(port);
	}

	KdPrint(("[Caronte][INFO] - Loaded."));

	return status;
}

// Called when this mini-filter is about to be unloaded. We unregister from the FltMgrand then return it is OK to unload
// Flags - Indicating if this is a mandatory unload.
NTSTATUS FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags){

	KdPrint(("[Caronte][INFO] - Unload driver start."));

	PAGED_CODE();
	UNREFERENCED_PARAMETER(Flags);

	CaronteDisconnect(Cookie);

	//  Close comunication port
	FltCloseCommunicationPort(port);

	//  Unregister from FLT mgr
	FltUnregisterFilter(gFilterHandle);
	
	//  Delete lookaside list
	ExDeleteNPagedLookasideList(&Pre2PostContextList);

	KdPrint(("[Caronte][INFO] - Unloaded driver."));

	return STATUS_SUCCESS;
}

FLT_PREOP_CALLBACK_STATUS PreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
){
/*++

Routine Description:

	This routine demonstrates how to swap buffers for the WRITE operation.

	Note that it handles all errors by simply not doing the buffer swap.

Arguments:

	Data - Pointer to the filter callbackData that is passed to us.

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance, its associated volume and
		file object.

	CompletionContext - Receives the context that will be passed to the
		post-operation callback.

Return Value:

	FLT_PREOP_SUCCESS_WITH_CALLBACK - we want a postOpeation callback
	FLT_PREOP_SUCCESS_NO_CALLBACK - we don't want a postOperation callback
	FLT_PREOP_COMPLETE -
--*/

	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	PVOID newBuf = NULL;
	PMDL newMdl = NULL;
	PVOLUME_CONTEXT volCtx = NULL;
	PCARONTE_RECORD p2pCtx;
	PVOID origBuf;
	NTSTATUS status;
	ULONG writeLen = iopb->Parameters.Write.Length;
	PFLT_FILE_NAME_INFORMATION FileNameInfo;
	WCHAR FileName[1000] = { 0 };
	PVOID *processBuffer = NULL;
	LARGE_INTEGER time;
	LONGLONG FileSize = 0;
	FILE_STANDARD_INFORMATION standardInfo;

	try {

		//  If they are trying to write ZERO bytes, then don't do anything and
		//  we don't need a post-operation callback.
		if (writeLen == 0)
			leave;

		//  Get our volume context so we can display our volume name in the
		//  debug output.
		status = FltGetVolumeContext(FltObjects->Filter, FltObjects->Volume, &volCtx);

		if (!NT_SUCCESS(status)) {

			KernPrint("[Caronte][ERR] - Error getting volume context (status: %x)\n", status);
			leave;
		}

		//  If this is a non-cached I/O we need to round the length up to the
		//  sector size for this device.  We must do this because the file
		//  systems do this and we need to make sure our buffer is as big
		//  as they are expecting.
		if (FlagOn(IRP_NOCACHE, iopb->IrpFlags)) 
			writeLen = (ULONG)ROUND_TO_SIZE(writeLen, volCtx->SectorSize);

		//  Allocate aligned nonPaged memory for the buffer we are swapping
		//  to. This is really only necessary for noncached IO but we always
		//  do it here for simplification. If we fail to get the memory, just
		//  don't swap buffers on this operation.
		newBuf = FltAllocatePoolAlignedWithTag(FltObjects->Instance, NonPagedPool, (SIZE_T)writeLen, BUFFER_SWAP_TAG);

		if (newBuf == NULL) {

			KernPrint("[Caronte][ERR] - %wZ failed to allocate %d bytes of memory.\n", &volCtx->Name, writeLen);
			leave;
		}

		//  We only need to build a MDL for IRP operations.  We don't need to
		//  do this for a FASTIO operation because it is a waste of time since
		//  the FASTIO interface has no parameter for passing the MDL to the
		//  file system.
		if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION)) {

			//  Allocate a MDL for the new allocated memory.  If we fail
			//  the MDL allocation then we won't swap buffer for this operation
			newMdl = IoAllocateMdl(newBuf, writeLen, FALSE, FALSE, NULL);

			if (newMdl == NULL) {

				KernPrint("[Caronte][ERR] - %wZ failed to allocate MDL.\n", &volCtx->Name);
				leave;
			}

			//  setup the MDL for the non-paged pool we just allocated
			MmBuildMdlForNonPagedPool(newMdl);
		}

		//  If the users original buffer had a MDL, get a system address.
		if (iopb->Parameters.Write.MdlAddress != NULL) {

			//  This should be a simple MDL. We don't expect chained MDLs
			//  this high up the stack
			FLT_ASSERT(((PMDL)iopb->Parameters.Write.MdlAddress)->Next == NULL);

			origBuf = MmGetSystemAddressForMdlSafe(iopb->Parameters.Write.MdlAddress, NormalPagePriority | MdlMappingNoExecute);

			if (origBuf == NULL) {

				KernPrint("[Caronte][ERR] - %wZ failed to get system address for MDL: %p\n",
						&volCtx->Name,
						iopb->Parameters.Write.MdlAddress);

				//  If we could not get a system address for the users buffer,
				//  then we are going to fail this operation.
				Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Data->IoStatus.Information = 0;
				retValue = FLT_PREOP_COMPLETE;
				leave;
			}

		}
		else {

			//  There was no MDL defined, use the given buffer address.
			origBuf = iopb->Parameters.Write.WriteBuffer;
		}

		status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &FileNameInfo);

		if (NT_SUCCESS(status)) {

			status = FltParseFileNameInformation(FileNameInfo);

			if (NT_SUCCESS(status)) {

				if (FileNameInfo->Name.MaximumLength < 1000) {

					RtlCopyMemory(FileName, FileNameInfo->Name.Buffer, FileNameInfo->Name.MaximumLength);

				}
			}

			FltReleaseFileNameInformation(FileNameInfo);
		}

		status = GetFileSize(FltObjects->Instance, FltObjects->FileObject, &FileSize);

		KeQuerySystemTime(&time);

		//  Copy the memory, we must do this inside the try/except because we
		//  may be using a users buffer address
		try {
			RtlCopyMemory(newBuf, origBuf, writeLen);

		} except(EXCEPTION_EXECUTE_HANDLER) {

			//  The copy failed, return an error, failing the operation.
			Data->IoStatus.Status = GetExceptionCode();
			Data->IoStatus.Information = 0;
			retValue = FLT_PREOP_COMPLETE;

			KernPrint("[CARONTE] PreWrite: %wZ Invalid user buffer, oldB=%p, status=%x\n",
					&volCtx->Name,
					origBuf,
					Data->IoStatus.Status);

			leave;
		}

		try {

			record.RecordID = RecordID;
			record.StartTime = time.QuadPart;
			record.CompletionTime = 0LL; 
			record.IsKernel = Data->RequestorMode;
			record.OperationType = Data->Iopb->IrpFlags;
			RtlCopyMemory(record.FilePath, FileName, 1000);
			record.StartFileSize = FileSize;
			record.CompletionFileSize = 0LL;
			record.ThreadId = (ULONGLONG)PsGetThreadId(Data->Thread);
			record.ProcessId = (ULONGLONG)PsGetProcessId(PsGetThreadProcess(Data->Thread));

			record.WriteLen = writeLen;
			if (1000000 > writeLen)
				RtlCopyMemory(record.WriteBuffer, origBuf, writeLen);
			else
				RtlCopyMemory(record.WriteBuffer, origBuf, 1000000);

			//  We are ready to swap buffers, get a pre2Post context structure.
			//  We need it to pass the volume context and the allocate memory
			//  buffer to the post operation callback.
			p2pCtx = ExAllocateFromNPagedLookasideList(&Pre2PostContextList);
			if (p2pCtx == NULL) {
				KernPrint("[Caronte][ERR] - %wZ Failed to allocate pre2Post context structure\n",&volCtx->Name);
				leave;
			}
			else {
				RtlCopyMemory(p2pCtx, &record, sizeof(CARONTE_RECORD));
			}

			RecordID++;
			*CompletionContext = p2pCtx;

		} except(EXCEPTION_EXECUTE_HANDLER) {
			KernPrint("[Caronte][ERR] - Can't send message to user-space Virgilio\n");
			KernPrint("[Caronte][ERR] - Status of send: %x\n", status);
		}

		//  Return we want a post-operation callback
		retValue = FLT_PREOP_SUCCESS_WITH_CALLBACK;

	}finally{

		//  If we don't want a post-operation callback, then free the buffer
		//  or MDL if it was allocated.

		if (newBuf != NULL) 
			FltFreePoolAlignedWithTag(FltObjects->Instance, newBuf, BUFFER_SWAP_TAG);

		if (newMdl != NULL) 
			IoFreeMdl(newMdl);

		if (volCtx != NULL) 
			FltReleaseContext(volCtx);
	}

	return retValue;
}

FLT_POSTOP_CALLBACK_STATUS PostWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
){
	PCARONTE_RECORD p2pCtx = CompletionContext;
	NTSTATUS status;
	FLT_POSTOP_CALLBACK_STATUS retVal;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	if (FltDoCompletionProcessingWhenSafe(Data, FltObjects, CompletionContext, Flags, PostWriteWhenSafe, &retVal)) {
		KernPrint("[Caronte][INFO] - Safe op OK \n");
	}
	else {
		KernPrint("[Caronte][ERR] - Safe op FAIL! \n");
	}
	
	ExFreeToNPagedLookasideList(&Pre2PostContextList, p2pCtx);

	return retVal;
}

FLT_POSTOP_CALLBACK_STATUS
PostWriteWhenSafe(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
){

	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PCARONTE_RECORD p2pCtx = CompletionContext;
	PVOID origBuf;
	NTSTATUS status;
	LARGE_INTEGER time;
	ULONGLONG FileSize;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	status = GetFileSize(FltObjects->Instance, FltObjects->FileObject, &FileSize);
	KeQuerySystemTime(&time);

	p2pCtx->CompletionTime = time.QuadPart;
	p2pCtx->CompletionFileSize = FileSize;

	status = FltSendMessage(gFilterHandle, &ClientPort, p2pCtx, sizeof(CARONTE_RECORD), NULL, NULL, NULL);
	KernPrint("[Caronte][INFO] - sendmessagestatus %x \n", status);

	return FLT_POSTOP_FINISHED_PROCESSING;
}


/*++
Routine Description:
	This routine obtains the size.
Arguments:
	Instance - Opaque filter pointer for the caller. This parameter is required and cannot be NULL.

	FileObject - File object pointer for the file. This parameter is required and cannot be NULL.
	Size - Pointer to a LONGLONG indicating the file size. This is the output.
Return Value:
	Returns statuses forwarded from FltQueryInformationFile.
--*/
NTSTATUS GetFileSize(
	_In_    PFLT_INSTANCE Instance,
	_In_    PFILE_OBJECT FileObject,
	_Out_   PLONGLONG Size
){
	NTSTATUS status = STATUS_SUCCESS;
	FILE_STANDARD_INFORMATION standardInfo;

	//  Querying for FileStandardInformation gives you the offset of EOF.
	status = FltQueryInformationFile(Instance,
		FileObject,
		&standardInfo,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation,
		NULL);

	if (NT_SUCCESS(status)) 
		*Size = standardInfo.EndOfFile.QuadPart;

	return status;
}

// Wrapper to KdPrint 
VOID KernPrint(PCSTR msg, ...) {

	va_list valist;
	va_start(valist, 10);

	if (LOG) 
		KdPrint((msg, valist));

}
