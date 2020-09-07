/*++

Module Name:

    FileBackupDriver.c

Abstract:

    This is the main module of the FileBackupDriver miniFilter driver.

Environment:

    Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>

#include "FileBackupDriver.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

ULONG gTraceFlags = 0;

/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

bool IsBackupDirectory(_In_ PCUNICODE_STRING directory);
NTSTATUS BackupFile(_In_ PUNICODE_STRING FileName, _In_ PCFLT_RELATED_OBJECTS FltObjects);

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
FileBackupDriverInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
FileBackupDriverInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
FileBackupDriverInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
FileBackupDriverUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
FileBackupDriverInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
FileBackupPreWrite (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
FileBackupPostCreate(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_POSTOP_CALLBACK_STATUS
FileBackupPostCleanup (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    );

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FileBackupDriverUnload)
#pragma alloc_text(PAGE, FileBackupDriverInstanceQueryTeardown)
#pragma alloc_text(PAGE, FileBackupDriverInstanceSetup)
#pragma alloc_text(PAGE, FileBackupDriverInstanceTeardownStart)
#pragma alloc_text(PAGE, FileBackupDriverInstanceTeardownComplete)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE, 0, nullptr, FileBackupPostCreate },
    { IRP_MJ_WRITE, FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO, FileBackupPreWrite, nullptr },
    { IRP_MJ_CLEANUP, 0, nullptr, FileBackupPostCleanup },
    { IRP_MJ_OPERATION_END }
};

//
//  Context registration
//

CONST FLT_CONTEXT_REGISTRATION Contexts[] = {
    { FLT_FILE_CONTEXT, 0, nullptr, sizeof(FileContext), DRIVER_CONTEXT_TAG }, 
    { FLT_CONTEXT_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    Contexts,                           //  Context
    Callbacks,                          //  Operation callbacks

    FileBackupDriverUnload,                           //  MiniFilterUnload

    FileBackupDriverInstanceSetup,                    //  InstanceSetup
    FileBackupDriverInstanceQueryTeardown,            //  InstanceQueryTeardown
    FileBackupDriverInstanceTeardownStart,            //  InstanceTeardownStart
    FileBackupDriverInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};



NTSTATUS
FileBackupDriverInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType)
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );

    PAGED_CODE();

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
        ("FileBackupDriver!FileBackupDriverInstanceSetup: Entered\n"));

    if (VolumeFilesystemType != FLT_FSTYPE_NTFS) {
        KdPrint(("Can not attaching to non-NTFS volume"));
        return STATUS_FLT_DO_NOT_ATTACH;
    }

    return STATUS_SUCCESS;
}


NTSTATUS
FileBackupDriverInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackupDriver!FileBackupDriverInstanceQueryTeardown: Entered\n") );

    return STATUS_SUCCESS;
}


VOID
FileBackupDriverInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags)
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackupDriver!FileBackupDriverInstanceTeardownStart: Entered\n") );
}


VOID
FileBackupDriverInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags)
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackupDriver!FileBackupDriverInstanceTeardownComplete: Entered\n") );
}


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackupDriver!DriverEntry: Entered\n") );

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering(gFilterHandle);

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter(gFilterHandle);
        }
    }

    return status;
}

NTSTATUS
FileBackupDriverUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags) {
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackupDriver!FileBackupDriverUnload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/

FLT_PREOP_CALLBACK_STATUS
FileBackupPreWrite (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext) 
{
    NTSTATUS status;
    
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(CompletionContext);

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
        ("FileBackupDriver!FileBackupDriverPreOperation: Entered\n"));
    
    FileContext* context;
    status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context); // context ObReference += 1
    if (!NT_SUCCESS(status) || context == nullptr) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    { // Locked scope
        AutoLock<Mutex> locker(context->Lock);
        if (!context->Written) {
            status = BackupFile(&context->FileName, FltObjects);
            if (!NT_SUCCESS(status)) {
                KdPrint(("Failed backup - %zW, (0x%X)\n", context->FileName, status));
            }
            context->Written = TRUE;
        }
    } // End locked scope
    
    FltReleaseContext(context); // context ObReference -= 1

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

// Creates context and attaches it to file after it created
FLT_POSTOP_CALLBACK_STATUS
FileBackupPostCreate(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
        ("FileBackupDriver!FileBackupDriverPostOperation: Entered\n"));

    // Kernel and Create operation
    const auto& params = Data->Iopb->Parameters.Create;
    if (Data->RequestorMode == MODE::KernelMode ||                             
        (params.SecurityContext->DesiredAccess & FILE_WRITE_DATA) == 0 ||      
        (Data->IoStatus.Information & FILE_DOES_NOT_EXIST) == 0) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FilterFileNameInformation fileNameInfo(Data);
    if (!fileNameInfo) {
        KdPrint(("Could not create file name information object\n"));
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (!NT_SUCCESS(fileNameInfo.Parse())) {
        KdPrint(("Could not parse file name information"));
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    // Check directory
    if (!IsBackupDirectory(&fileNameInfo->ParentDir)) {
        KdPrint(("%wZ is not a backup directory\n", fileNameInfo->ParentDir));
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    // Check file stream
    if (fileNameInfo->Stream.Length > 0) {
        KdPrint(("Alternative streams aren't supported\n"));
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    // Create the file context
    FileContext* context;
    NTSTATUS status = FltAllocateContext(FltObjects->Filter, FLT_FILE_CONTEXT, sizeof(FileContext), PagedPool, (PFLT_CONTEXT*)&context); // context ObReference += 1
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to allocate file context (0x%08X)\n", status));
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    context->Written = FALSE;

    context->FileName.MaximumLength = fileNameInfo->Name.Length;
    context->FileName.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, fileNameInfo->Name.Length, DRIVER_TAG);
    if (!context->FileName.Buffer) {
        KdPrint(("Could not allocate memory for file context buffer\n"));
        FltReleaseContext(context);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }
    RtlCopyUnicodeString(&context->FileName, &fileNameInfo->Name);

    context->Lock.Init();

    // Attach the initialize context to the file
    status = FltSetFileContext(FltObjects->Instance, FltObjects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, context, nullptr); // context ObReference += 1
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed attach initialized context to the file (0x%08X)\n", status));
        ExFreePoolWithTag(context->FileName.Buffer, DRIVER_TAG);
    }
    FltReleaseContext(context); // should be released for both success and failure - context ObReference -= 1

    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_POSTOP_CALLBACK_STATUS
FileBackupPostCleanup(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FileBackupDriver!FileBackupDriverPostOperation: Entered\n") );

    FileContext* context;
    NTSTATUS status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
    if (!NT_SUCCESS(status) || context == nullptr) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (context->FileName.Buffer)
        ExFreePoolWithTag(context->FileName.Buffer, DRIVER_TAG);

    FltReleaseContext(context);
    FltDeleteContext(context);

    return FLT_POSTOP_FINISHED_PROCESSING;
}

/*************************************************************************
    Helper functions.
*************************************************************************/

bool IsBackupDirectory(_In_ PCUNICODE_STRING directory) {
    ULONG maxSize = 1024; // arbitary max length
    if (directory->Length > maxSize) {
        return false;
    }

    WCHAR* copy = (WCHAR*)ExAllocatePoolWithTag(PagedPool, maxSize + sizeof(WCHAR), DRIVER_TAG);
    if (!copy) {
        KdPrint(("Could not allocate memory for copy\n"));
        return false;
    }
    RtlZeroMemory(copy, maxSize + sizeof(WCHAR));

    wcsncpy_s(copy, 1 + maxSize / sizeof(WCHAR), directory->Buffer, directory->Length / sizeof(WCHAR));
    _wcslwr(copy);

    bool doBackup = wcsstr(copy, L"\\pictures\\") || wcsstr(copy, L"\\documents\\");
    
    ExFreePoolWithTag(copy, DRIVER_TAG);

    return doBackup;

}

NTSTATUS BackupFile(_In_ PUNICODE_STRING FileName, _In_ PCFLT_RELATED_OBJECTS FltObjects) {
    NTSTATUS ntStatus = STATUS_SUCCESS;
    HANDLE hTargetFile = nullptr;
    HANDLE hSourceFile = nullptr;
    IO_STATUS_BLOCK ioStatus;
    PVOID buffer = nullptr;

    LARGE_INTEGER fileSize;
    ntStatus = FsRtlGetFileSize(FltObjects->FileObject, &fileSize);
    if (!NT_SUCCESS(ntStatus) || fileSize.QuadPart == 0) {
        KdPrint(("Failed to get the file size\n"));
        return ntStatus;
    }

    do {
        OBJECT_ATTRIBUTES sourceFileAttr = { 0 };
        InitializeObjectAttributes(&sourceFileAttr, FileName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        ntStatus = FltCreateFile(
            FltObjects->Filter,
            FltObjects->Instance,
            &hSourceFile,
            FILE_READ_DATA | SYNCHRONIZE,
            &sourceFileAttr,
            &ioStatus,
            nullptr,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_OPEN,
            FILE_SYNCHRONOUS_IO_NONALERT,
            nullptr,
            0,
            IO_IGNORE_SHARE_ACCESS_CHECK);
        if (!NT_SUCCESS(ntStatus)) {
            KdPrint(("Open source file failed (0x%08X)\n", ntStatus));
            break;
        }

        UNICODE_STRING targetFileName;
        const WCHAR backupStream[] = L":backup";
        targetFileName.MaximumLength = FileName->Length + sizeof(backupStream);
        targetFileName.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, targetFileName.MaximumLength, DRIVER_TAG);
        if (!targetFileName.Buffer) {
            KdPrint(("Could not allocate memory for target file name buffer\n"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }
        RtlCopyUnicodeString(&targetFileName, FileName);
        RtlAppendUnicodeToString(&targetFileName, backupStream);

        OBJECT_ATTRIBUTES targetFileAttr = { 0 };
        InitializeObjectAttributes(&targetFileAttr, &targetFileName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        ntStatus = FltCreateFile(
            FltObjects->Filter,
            FltObjects->Instance,
            &hTargetFile,
            GENERIC_WRITE | SYNCHRONIZE,
            &targetFileAttr, 
            &ioStatus,
            nullptr,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_OVERWRITE_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            nullptr,
            0,
            0);

        ExFreePoolWithTag(targetFileName.Buffer, DRIVER_TAG);

        if (!NT_SUCCESS(ntStatus)) {
            KdPrint(("Open target file failed (0x%08X)\n", ntStatus));
            break;
        }


        ULONG size = 1 << 21; // 2MB
        buffer = ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
        if (!buffer) {
            KdPrint(("Could not allocate memory for copy buffer\n"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        LARGE_INTEGER offset = { 0 };
        LARGE_INTEGER writeOffset = { 0 };
        ULONG bytes;
        LARGE_INTEGER saveSize = fileSize;

        while (fileSize.QuadPart > 0) {
            ntStatus = ZwReadFile(
                hSourceFile, 
                nullptr, 
                nullptr, nullptr, 
                &ioStatus, 
                buffer, (ULONG)min((LONGLONG)size, fileSize.QuadPart), 
                &offset, 
                nullptr);
            if (!NT_SUCCESS(ntStatus)) {
                KdPrint(("Failed to read chunk from source file (0x%08X)\n", ntStatus));
                break;
            }

            bytes = (ULONG)ioStatus.Information;

            ntStatus = ZwWriteFile(
                hTargetFile,
                nullptr,
                nullptr, nullptr,
                &ioStatus,
                buffer,
                bytes,
                &writeOffset,
                nullptr);
            if (!NT_SUCCESS(ntStatus)) {
                KdPrint(("Failed to write chunk to target file (0x%08X)\n", ntStatus));
                break;
            }

            offset.QuadPart += bytes;
            writeOffset.QuadPart += bytes;
            fileSize.QuadPart -= bytes;
        }

        FILE_END_OF_FILE_INFORMATION info;
        info.EndOfFile = saveSize;
        NT_VERIFY(NT_SUCCESS(ZwSetInformationFile(hTargetFile, &ioStatus, &info, sizeof(info), FileEndOfFileInformation)));
    } while (false);

    if (buffer)
        ExFreePoolWithTag(buffer, DRIVER_TAG);
    if (hTargetFile)
        FltClose(hTargetFile);
    if (hSourceFile)
        FltClose(hSourceFile);

    return ntStatus;
}