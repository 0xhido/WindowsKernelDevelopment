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
#include "DeleteProtectorDriver.h"

/*************************************************************************
    Globals
*************************************************************************/

ULONG gTraceFlags = 0;
PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

const int MaxExecutables = 32;
WCHAR* ExeNames[MaxExecutables];
int ExeNamesCount;
FastMutex ExeNamesLock;

const int MaxDirectories = 32;
DirectoryEntry DirNames[MaxDirectories];
int DirNamesCount;
FastMutex DirNamesLock;


/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

bool IsDeleteAllowedByProcess(const PEPROCESS Process);
bool IsDeleteAllowedByDirectory(_In_ PFLT_CALLBACK_DATA Data);
bool FindExecutable(PCWSTR name);
void ClearAllExes();
int FindDirectory(PCUNICODE_STRING name, bool dosName);
NTSTATUS ConvertDosNameToNtName(_In_ PCWSTR dosName, _Out_ PUNICODE_STRING ntName);
void ClearAllDirs();

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
DeleteProtectorCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
);

NTSTATUS
DeleteProtectorDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
);

VOID
DeleteProtectorUnload(
    _In_ PDRIVER_OBJECT DriverObject
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
    UNREFERENCED_PARAMETER(RegistryPath);

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
        ("DeleteProtectorDriver!DriverEntry: Entered\n"));

    NTSTATUS status;
    PDEVICE_OBJECT DeviceObject = nullptr;
    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\DeleteProtector");
    UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\DeleteProtector");
    bool symLinkCreated = false;

    do {
        status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
        if (!NT_SUCCESS(status)) {
            KdPrint(("Device object creatation failed (0x%08X)\n", status));
            break;
        }

        status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
        if (!NT_SUCCESS(status)) {
            KdPrint(("Symbolic link creatation failed (0x%08X)\n", status));
            break;
        }
        symLinkCreated = true;

        status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
        FLT_ASSERT(NT_SUCCESS(status));
        if (!NT_SUCCESS(status)) {
            KdPrint(("Filter registration failed (0x%08X)\n", status));
            break;
        }

        DriverObject->DriverUnload = DeleteProtectorUnload;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = DeleteProtectorCreateClose;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeleteProtectorCreateClose;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeleteProtectorDeviceControl;
        ExeNamesLock.Init();
        DirNamesLock.Init();

        status = FltStartFiltering(gFilterHandle);
        FLT_ASSERT(NT_SUCCESS(status));
        if (!NT_SUCCESS(status)) {
            KdPrint(("Start filtering failed (0x%08X)\n", status));
            break;
        }
    } while (false);

    if (!NT_SUCCESS(status)) {
        if (gFilterHandle) {
            FltUnregisterFilter(gFilterHandle);
        }
        
        if (symLinkCreated) {
            IoDeleteSymbolicLink(&symbolicLink);
        }
        
        if (DeviceObject) {
            IoDeleteDevice(DeviceObject);
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
NTSTATUS 
DeleteProtectorCreateClose(PDEVICE_OBJECT /*DeviceObject*/, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS 
DeleteProtectorDeviceControl(PDEVICE_OBJECT /*DeviceObject*/, PIRP Irp)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ControlCode = stack->Parameters.DeviceIoControl.IoControlCode;

    switch (ControlCode) {
    case IOCTL_DELETEPROTECTOR_ADD_EXE: {
        WCHAR* name = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
        if (!name) {
            KdPrint(("Got invalid executable name\n"));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }

        if (FindExecutable(name)) {
            KdPrint(("Executable name already exists\n"));
            break;
        }

        AutoLock<FastMutex> locker(ExeNamesLock);
        if (ExeNamesCount == MaxExecutables) {
            KdPrint(("Executables black list full\n"));
            ntStatus = STATUS_TOO_MANY_NAMES;
            break;
        }

        for (int i = 0; i < MaxExecutables; i++) {
            if (ExeNames[i] == nullptr) {
                auto len = (wcslen(name) + 1) * sizeof(WCHAR);
                WCHAR* buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, len, DRIVER_TAG);
                if (buffer == nullptr) {
                    KdPrint(("Could not allocate memory for executable name buffer\n"));
                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                }

                wcscpy_s(buffer, len / sizeof(WCHAR), name);

                ExeNames[i] = buffer;
                ExeNamesCount++;
                break;
            }
        }

        break;
    }
        
    case IOCTL_DELETEPROTECTOR_REMOVE_EXE: {
        WCHAR* name = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
        if (!name) {
            KdPrint(("Got invalid name\n"));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }

        AutoLock<FastMutex> locker(ExeNamesLock);
        bool found = false;
        
        for (int i = 0; i < ExeNamesCount; i++) {
            if (ExeNames[i] && _wcsicmp(ExeNames[i], name) == 0) {
                ExFreePoolWithTag(ExeNames[i], DRIVER_TAG);
                ExeNames[i] = nullptr;
                ExeNamesCount--;
                found = true;
                break;
            }
        }

        if (!found) {
            KdPrint(("Executable not found - nothing to delete\n"));
            ntStatus = STATUS_NOT_FOUND;
        }

        break;
    }

    case IOCTL_DELETEPROTECTOR_CLEAR: {
        ClearAllExes();
        break;
    }

    case IOCTL_DELETEPROTECTOR_ADD_DIR: {
        WCHAR* dirName = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
        if (!dirName) {
            KdPrint(("Got invalid directory name\n"));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }

        ULONG bufferLen = stack->Parameters.DeviceIoControl.InputBufferLength;
        if (bufferLen > 1024) {
            KdPrint(("Directory name too long - skipped\n"));
            ntStatus = STATUS_NAME_TOO_LONG;
            break;
        }

        dirName[bufferLen / sizeof(WCHAR) - 1] = L'\0';
        
        size_t dosNameLen = wcslen(dirName);
        if (dosNameLen < 3) {
            KdPrint(("Got invalid directory name\n"));
            ntStatus = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        AutoLock<FastMutex> locker(DirNamesLock);
        if (DirNamesCount == MaxDirectories) {
            KdPrint(("Directories black list is full\n"));
            ntStatus = STATUS_TOO_MANY_NAMES;
            break;
        }

        UNICODE_STRING strName;
        RtlInitUnicodeString(&strName, dirName);
        
        if (FindDirectory(&strName, true) >= 0) {
            KdPrint(("Directory name already exists\n"));
            break;
        }

        for (int i = 0; i < MaxDirectories; i++) {
            if (DirNames[i].DosName.Buffer == nullptr) {
                size_t len = (dosNameLen + 2) * sizeof(WCHAR); // add space for backslash and NULL terminator
                WCHAR* buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, len, DRIVER_TAG);
                if (buffer == nullptr) {
                    KdPrint(("Could not allocate memory for directory name buffer\n"));
                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                }

                wcscpy_s(buffer, len / sizeof(WCHAR), dirName);

                if (dirName[dosNameLen - 1] != L'\\')
                    wcscat_s(buffer, len / sizeof(WCHAR), L"\\");

                ntStatus = ConvertDosNameToNtName(buffer, &DirNames[i].NtName);
                if (!NT_SUCCESS(ntStatus)) {
                    ExFreePool(buffer);
                    break;
                }

                RtlInitUnicodeString(&DirNames[i].DosName, buffer);
                KdPrint(("Add: %wZ <=> %wZ\n", &DirNames[i].DosName, &DirNames[i].NtName));
                DirNamesCount++;
                break;
            }
        }

        break;
    }

    case IOCTL_DELETEPROTECTOR_REMOVE_DIR: {
        WCHAR* dirName = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
        if (!dirName) {
            KdPrint(("Got invalid directory name\n"));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }

        UNICODE_STRING strName;
        RtlInitUnicodeString(&strName, dirName);

        AutoLock<FastMutex> locker(DirNamesLock);
        int directoryIndex = FindDirectory(&strName, true);
        if (directoryIndex < 0) {
            KdPrint(("Directory not found - nothing to delete\n"));
            ntStatus = STATUS_NOT_FOUND;
        }
        else {
            DirNames[directoryIndex].Free();
            DirNamesCount--;
        }

        break;
    }

    case IOCTL_DELETEPROTECTOR_CLEAR_DIRS: {
        ClearAllDirs();

        break;
    }
    
    default:
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = ntStatus;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return ntStatus;
}

_Use_decl_annotations_
VOID
DeleteProtectorUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\DeleteProtector");

    IoDeleteSymbolicLink(&symbolicLink);
    
    if (DriverObject->DeviceObject)
        IoDeleteDevice(DriverObject->DeviceObject);

    ClearAllExes();
    ClearAllDirs();

    KdPrint(("Driver Unloaded!\n"));
}
 
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

        if (!IsDeleteAllowedByProcess(PsGetCurrentProcess()) || !IsDeleteAllowedByDirectory(Data)) {
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

    if (!IsDeleteAllowedByProcess(process) || !IsDeleteAllowedByDirectory(Data)) {
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        fltStatus = FLT_PREOP_COMPLETE;

        KdPrint(("Prevent delete from IRP_MJ_SET_INFOMATION\n"));
    }

    return fltStatus;
}

/*************************************************************************
    Helper Functions.
*************************************************************************/

bool 
IsDeleteAllowedByProcess(const PEPROCESS Process) {
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

            WCHAR* exeName = wcsrchr(processName->Buffer, L'\\');
            NT_ASSERT(exeName);

            if (exeName && FindExecutable(exeName + 1)) { // skip backslash 
                allowDeletion = false;
            }
        }

        ExFreePool(processName);
    }

    if (!currentProcess)
        ZwClose(hProcess);
    
    return allowDeletion;
}

bool
IsDeleteAllowedByDirectory(_In_ PFLT_CALLBACK_DATA Data) {
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFLT_FILE_NAME_INFORMATION nameInfo = nullptr;
    bool allow = true;

    do {
        ntStatus = FltGetFileNameInformation(Data, FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_NORMALIZED, &nameInfo);
        if (!NT_SUCCESS(ntStatus)) {
            KdPrint(("FltGetFileNameInformation failed\n"));
            break;
        }

        ntStatus = FltParseFileNameInformation(nameInfo);
        if (!NT_SUCCESS(ntStatus)) {
            KdPrint(("FltParseFileNameInformation failed\n"));
            break;
        }

        UNICODE_STRING path;
        path.Length = path.MaximumLength =
            nameInfo->Volume.Length + nameInfo->Share.Length + nameInfo->ParentDir.Length;
        path.Buffer = nameInfo->Volume.Buffer; // includes the full path because the path is contiguous in memory

        AutoLock<FastMutex> locker(DirNamesLock);

        if (FindDirectory(&path, false) >= 0) {
            KdPrint(("File not allowed to be deleted: %wZ\n", nameInfo->Name));
            allow = false;
        }
    } while (false);

    if (nameInfo)
        FltReleaseFileNameInformation(nameInfo);

    return allow;
}

bool
FindExecutable(PCWSTR name) {
    AutoLock<FastMutex> locker(ExeNamesLock);

    if (ExeNamesCount == 0) {
        return false;
    }

    for (int i = 0; i < MaxExecutables; i++) {
        if (ExeNames[i] && _wcsicmp(ExeNames[i], name) == 0) {
            return true;
        }
    }

    return false;
}

void 
ClearAllExes() {
    AutoLock<FastMutex> locker(ExeNamesLock);

    for (int i = 0; i < MaxExecutables; i++) {
        if (ExeNames[i]) {
            ExFreePoolWithTag(ExeNames[i], DRIVER_TAG);
            ExeNames[i] = nullptr;
            ExeNamesCount--;
        }
    }

    ExeNamesCount = 0;
}

int 
FindDirectory(PCUNICODE_STRING name, bool dosName) {
    if (DirNamesCount == 0)
        return -1;

    for (int i = 0; i < MaxDirectories; i++) {
        const UNICODE_STRING& dir = dosName ? DirNames[i].DosName : DirNames[i].NtName;

        if (dir.Buffer&& RtlEqualUnicodeString(name, &dir, TRUE)) {
            return i;
        }
    }

    return -1;
}

NTSTATUS 
ConvertDosNameToNtName(_In_ PCWSTR dosName, _Out_ PUNICODE_STRING ntName) {
    NTSTATUS ntStatus = STATUS_SUCCESS;
    size_t dosNameLen = wcslen(dosName);
    
    ntName->Buffer = nullptr;

    if (dosNameLen < 3) {
        KdPrint(("Dos Name is too small for conversion\n"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (dosName[2] != L'\\' || dosName[1] != L':') {
        KdPrint(("Dos Name must be local directory (starting with C: or any drive letter)\n"));
        return STATUS_INVALID_PARAMETER;
    }
    
    kstring symbLink(L"\\??\\");
    symbLink.Append(dosName, 2);

    UNICODE_STRING symbLinkFull;
    symbLink.GetUnicodeString(&symbLinkFull);

    OBJECT_ATTRIBUTES symbLinkAttributes;
    InitializeObjectAttributes(&symbLinkAttributes, &symbLinkFull, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

    HANDLE hSymbLink = nullptr;
    
    do {
        ntStatus = ZwOpenSymbolicLinkObject(&hSymbLink, GENERIC_READ, &symbLinkAttributes);
        if (!NT_SUCCESS(ntStatus)) {
            KdPrint(("Open symbolic link to drive letter failed\n"));
            break;
        }

        USHORT maxLen = 1024;
        ntName->Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, maxLen, DRIVER_TAG);
        if (ntName->Buffer == nullptr) {
            KdPrint(("Could not allocate memory for NtName buffer\n"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }
        ntName->MaximumLength = maxLen;

        ntStatus = ZwQuerySymbolicLinkObject(hSymbLink, ntName, nullptr);
        if (!NT_SUCCESS(ntStatus)) {
            KdPrint(("Query symbolic link object failed\n"));
            break;
        }
    } while (false);

    if (!NT_SUCCESS(ntStatus)) {
        if (ntName->Buffer) {
            ExFreePoolWithTag(ntName->Buffer, DRIVER_TAG);
            ntName->Buffer = nullptr;
        }
    }
    else {
        RtlAppendUnicodeToString(ntName, dosName + 2);
    }

    if (hSymbLink)
        ZwClose(hSymbLink);

    return ntStatus;
}

void
ClearAllDirs() {
    AutoLock<FastMutex> locker(DirNamesLock);

    for (int i = 0; i < MaxDirectories; i++) {
        DirNames[i].Free();
    }

    DirNamesCount = 0;
}