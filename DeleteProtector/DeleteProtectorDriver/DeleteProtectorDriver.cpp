/*++

Module Name:

    DeleteProtectorDriver.c

Abstract:

    This is the main module of the DeleteProtectorDriver miniFilter driver.

Environment:

    Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

#define DRIVER_TAG 'pled'

/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

bool IsDeleteAllowed(const PEPROCESS Process);

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

FLT_PREOP_CALLBACK_STATUS
DeleteProtectorDriverPreCreate(
    _Inout_ PFLT_CALLBACK_DATA Data, 
    _In_ PCFLT_RELATED_OBJECTS FltObjects, 
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext);

FLT_PREOP_CALLBACK_STATUS
DeleteProtectorDriverPreSetInformation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

NTSTATUS ZwQueryInformationProcess(
    _In_      HANDLE           ProcessHandle,
    _In_      PROCESSINFOCLASS ProcessInformationClass,
    _Out_     PVOID            ProcessInformation,
    _In_      ULONG            ProcessInformationLength,
    _Out_opt_ PULONG           ReturnLength
);

NTSTATUS
DeleteProtectorDriverInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
DeleteProtectorDriverInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
DeleteProtectorDriverInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
DeleteProtectorDriverUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
DeleteProtectorDriverInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DeleteProtectorDriverUnload)
#pragma alloc_text(PAGE, DeleteProtectorDriverInstanceQueryTeardown)
#pragma alloc_text(PAGE, DeleteProtectorDriverInstanceSetup)
#pragma alloc_text(PAGE, DeleteProtectorDriverInstanceTeardownStart)
#pragma alloc_text(PAGE, DeleteProtectorDriverInstanceTeardownComplete)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE, 0, DeleteProtectorDriverPreCreate, nullptr },
    { IRP_MJ_SET_INFORMATION, 0, DeleteProtectorDriverPreSetInformation, nullptr },
    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    DeleteProtectorDriverUnload,                           //  MiniFilterUnload

    DeleteProtectorDriverInstanceSetup,                    //  InstanceSetup
    DeleteProtectorDriverInstanceQueryTeardown,            //  InstanceQueryTeardown
    DeleteProtectorDriverInstanceTeardownStart,            //  InstanceTeardownStart
    DeleteProtectorDriverInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};



NTSTATUS
DeleteProtectorDriverInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType) 
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);

    PAGED_CODE();

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
        ("DeleteProtectorDriver!DeleteProtectorDriverInstanceSetup: Entered\n"));

    return STATUS_SUCCESS;
}


NTSTATUS
DeleteProtectorDriverInstanceQueryTeardown(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags) {
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DeleteProtectorDriver!DeleteProtectorDriverInstanceQueryTeardown: Entered\n") );

    return STATUS_SUCCESS;
}


VOID
DeleteProtectorDriverInstanceTeardownStart(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags) {
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DeleteProtectorDriver!DeleteProtectorDriverInstanceTeardownStart: Entered\n") );
}


VOID
DeleteProtectorDriverInstanceTeardownComplete(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags) {
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DeleteProtectorDriver!DeleteProtectorDriverInstanceTeardownComplete: Entered\n") );
}


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DeleteProtectorDriver!DriverEntry: Entered\n") );

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( gFilterHandle );
        }
    }

    return status;
}

NTSTATUS
DeleteProtectorDriverUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags) {
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DeleteProtectorDriver!DeleteProtectorDriverUnload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/

_Use_decl_annotations_
FLT_PREOP_CALLBACK_STATUS 
DeleteProtectorDriverPreCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, _Flt_CompletionContext_Outptr_ PVOID* CompletionContext)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    FLT_PREOP_CALLBACK_STATUS fltStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    // Don't intercept kernel requests
    if (Data->RequestorMode == MODE::KernelMode) {
        return fltStatus;
    }

    // Check if it's delete operation
    const auto& createParams = Data->Iopb->Parameters.Create;
    if (createParams.Options & FILE_DELETE_ON_CLOSE) {
        KdPrint(("Delete on close: %wZ\n", &Data->Iopb->TargetFileObject->FileName));

        if (!IsDeleteAllowed(PsGetCurrentProcess())) {
            Data->IoStatus.Status = STATUS_ACCESS_DENIED;
            fltStatus = FLT_PREOP_COMPLETE;

            KdPrint(("Prevent delete from IRP_MJ_CREATE\n"));
        }
    }

    return fltStatus;
}

_Use_decl_annotations_
FLT_PREOP_CALLBACK_STATUS
DeleteProtectorDriverPreSetInformation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, _Flt_CompletionContext_Outptr_ PVOID* CompletionContext) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    FLT_PREOP_CALLBACK_STATUS fltStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

    // Don't intercept kernel requests
    if (Data->RequestorMode == MODE::KernelMode) {
        return fltStatus;
    }

    // Check if SetInformation is delete action
    const auto& setInformationParams = Data->Iopb->Parameters.SetFileInformation;
    if (setInformationParams.FileInformationClass != FileDispositionInformation
        && setInformationParams.FileInformationClass != FileDispositionInformationEx) {
        return fltStatus;
    }
    
    FILE_DISPOSITION_INFORMATION* info = (FILE_DISPOSITION_INFORMATION*)setInformationParams.InfoBuffer;
    if (!info->DeleteFile) {
        return fltStatus;
    }

    // Now we can be sure we're dealing with delete operation

    // Get the calling process object
    PEPROCESS process = PsGetThreadProcess(Data->Thread);
    NT_ASSERT(process);

    if (!IsDeleteAllowed(process)) {
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        fltStatus = FLT_PREOP_COMPLETE;

        KdPrint(("Prevent delete from IRP_MJ_SET_INFOMATION\n"));
    }

    return fltStatus;
}

bool 
IsDeleteAllowed(const PEPROCESS Process) {
    bool currentProcess = PsGetCurrentProcess() == Process;
    HANDLE hProcess;

    // Get Process handle
    if (currentProcess) {
        hProcess = NtCurrentProcess();
    }
    else {
        NTSTATUS status = ObOpenObjectByPointer(Process, OBJ_KERNEL_HANDLE, nullptr, 0, nullptr, KernelMode, &hProcess);
        if (!NT_SUCCESS(status)) {
            KdPrint(("Could not obtain a reference to the process, allowing deletion\n"));
            return true;
        }
    }

    bool allowDeletion = true;

    // Create a Process Name buffer
    ULONG size = sizeof(UNICODE_STRING) + 300; // allocating space for both the structure and the buffer
    UNICODE_STRING* processName = (UNICODE_STRING*)ExAllocatePoolWithTag(PagedPool, size, 0);
    if (processName) {
        // processName stuct isn't zeroed by default
        RtlZeroMemory(processName, size);

        // Fetch Process Name
        NTSTATUS status = ZwQueryInformationProcess(
            hProcess,
            PROCESSINFOCLASS::ProcessImageFileName,
            processName,
            size - sizeof(WCHAR),
            nullptr);
        if (NT_SUCCESS(status)) {
            KdPrint(("Delete operation from %wZ\n", processName));

            if (wcsstr(processName->Buffer, L"\\cmd.exe") != nullptr) {
                allowDeletion = false;
            }
        }

        ExFreePool(processName);
    }

    if (!currentProcess)
        ZwClose(hProcess);
    
    return allowDeletion;
}
