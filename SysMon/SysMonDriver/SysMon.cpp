#include "pch.h"
#include "SysMon.h"
#include "SysMonCommon.h"

Globals g_Globals;
int g_MaxItems;

void PushItem(PLIST_ENTRY entry) {
	AutoLock<FastMutex> lock(g_Globals.Mutex);
	if (g_Globals.ItemCount > g_Globals.MaxItemsCount) {
		PLIST_ENTRY head = RemoveHeadList(&g_Globals.ItemsHead);
		g_Globals.ItemCount--;

		FullItem<ItemHeader>* item = CONTAINING_RECORD(head, FullItem<ItemHeader>, Entry);
		ExFreePool(item);
	}
	InsertTailList(&g_Globals.ItemsHead, entry);
	g_Globals.ItemCount++;
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR inforamtion = 0) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = inforamtion;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS SysMonIrpCreateClose(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp) {
	return CompleteIrp(Irp);
}

NTSTATUS SysMonIrpRead(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp) {
	return CompleteIrp(Irp);
}

void OnProcessNotify(_Inout_ PEPROCESS /*Process*/, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo) {
	FullItem<ProcessExitInfo>* info = 
		(FullItem<ProcessExitInfo>*)ExAllocatePoolWithTag(PagedPool, sizeof(FullItem<ProcessExitInfo>), DRIVER_TAG);
	if (info == nullptr) {
		KdPrint((DRIVER_PREFIX "item allocation failed!\n"));
	}

	if (CreateInfo) {
		// Handle process creation
	}
	else {
		// Handle process termination
		ProcessExitInfo& item = info->Data;
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessExit;
		item.ProcessId = HandleToULong(ProcessId);
		item.Size = sizeof(ProcessExitInfo);

		PushItem(&info->Entry);
	}
}

void SysMonUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\SysMon");

	IoDeleteSymbolicLink(&symbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);

	while (!IsListEmpty(&g_Globals.ItemsHead)) {
		auto entry = RemoveHeadList(&g_Globals.ItemsHead);
		ExFreePool(CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry));
	}

	KdPrint(("SysMon driver unloaded successfully\n"));
}

ULONG GetKeyInfoSize(HANDLE hRegistryKey, PUNICODE_STRING pValueName) {
	NTSTATUS ntStatus = STATUS_SUCCESS;

	ULONG ulKeyInfoSizeNeeded;
	ntStatus = ZwQueryValueKey(hRegistryKey, pValueName, KeyValueFullInformation, 0, 0, &ulKeyInfoSizeNeeded);
	if (ntStatus == STATUS_BUFFER_TOO_SMALL || ntStatus == STATUS_BUFFER_OVERFLOW) {
		// Expected don't worry
		return ulKeyInfoSizeNeeded;
	}
	else {
		KdPrint((DRIVER_PREFIX "Could not get key info size (0x%08X)\n", ntStatus));
	}

	return 0;
}

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PDEVICE_OBJECT DeviceObject = nullptr;
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\SysMon");
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\SysMon");
	UNICODE_STRING maxLengthValueName = RTL_CONSTANT_STRING(L"MaxItemCount");
	HANDLE hRegistryKey;
	PKEY_VALUE_FULL_INFORMATION pKeyInfo = nullptr;

	bool symLinkCreated = false;

	// Initailize global members
	InitializeListHead(&g_Globals.ItemsHead);
	g_Globals.Mutex.Init();

	// Create object attributes for registry key querying
	OBJECT_ATTRIBUTES ObjectAttributes = { 0 };
	InitializeObjectAttributes(&ObjectAttributes, RegistryPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	do {
		ntStatus = ZwOpenKey(&hRegistryKey, KEY_QUERY_VALUE, &ObjectAttributes);
		if (!NT_SUCCESS(ntStatus)) {
			KdPrint((DRIVER_PREFIX "Registry key open failed (0x%08X)\n", ntStatus));
			break;
		}

		ULONG ulKeyInfoSize;
		ULONG ulKeyInfoSizeNeeded = GetKeyInfoSize(hRegistryKey, &maxLengthValueName);
		if (ulKeyInfoSizeNeeded == 0) {
			KdPrint((DRIVER_PREFIX "Max item count value not found\n"));
			break;
		}
		ulKeyInfoSize = ulKeyInfoSizeNeeded;

		pKeyInfo = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, ulKeyInfoSize, DRIVER_TAG);
		if (pKeyInfo == nullptr) {
			KdPrint((DRIVER_PREFIX "Could not allocate memory for KeyValueInfo\n"));
			break;
		}
		RtlZeroMemory(pKeyInfo, ulKeyInfoSize);

		ntStatus = ZwQueryValueKey(hRegistryKey, &maxLengthValueName, KeyValueFullInformation, pKeyInfo, ulKeyInfoSize, &ulKeyInfoSizeNeeded);
		if (!NT_SUCCESS(ntStatus) || ulKeyInfoSize != ulKeyInfoSizeNeeded) {
			KdPrint((DRIVER_PREFIX "Registry value querying failed (0x%08X)\n", ntStatus));
			break;
		}

		ULONG maxCount = *(ULONG*)((ULONG_PTR)pKeyInfo + pKeyInfo->DataOffset);
		g_Globals.MaxItemsCount = maxCount;
	} while (false);

	if (hRegistryKey) {
		ZwClose(hRegistryKey);
	}

	if (pKeyInfo) {
		ExFreePoolWithTag(pKeyInfo, DRIVER_TAG);
	}

	if (!NT_SUCCESS(ntStatus)) {
		KdPrint((DRIVER_PREFIX "Setting max item count to default value\n"));
		g_Globals.MaxItemsCount = DEFAULT_MAX_ITEMS_COUNT;
		ntStatus = STATUS_SUCCESS;
	}
	
	// Object creatation (Device, SymbolicLink, Notifies, ...)
	do {
		ntStatus = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(ntStatus)) {
			KdPrint((DRIVER_PREFIX "Device object creatation failed (0x%08X)\n", ntStatus));
			break;
		}
		DeviceObject->Flags |= DO_DIRECT_IO;

		ntStatus = IoCreateSymbolicLink(&symbolicLink, &deviceName);
		if (!NT_SUCCESS(ntStatus)) {
			KdPrint((DRIVER_PREFIX "Symbolic link creatation failed (0x%08X)\n", ntStatus));
			break;
		}
		symLinkCreated = true;

		ntStatus = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(ntStatus)) {
			KdPrint((DRIVER_PREFIX "Process notify registration failed (0x%08X)\n", ntStatus));
			break;
		}
	} while (false);

	// Cleanup if something failed
	if (!NT_SUCCESS(ntStatus)) {
		if (symLinkCreated) {
			IoDeleteSymbolicLink(&symbolicLink);
		}

		if (DeviceObject) {
			IoDeleteDevice(DeviceObject);
		}
	}

	// Unload routine registration
	DriverObject->DriverUnload = SysMonUnload;

	// Dispatch functions registration
	DriverObject->MajorFunction[IRP_MJ_CREATE] = SysMonIrpCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = SysMonIrpCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = SysMonIrpRead;
	
	KdPrint(("SysMon driver DriverEntry finished!\n"));

	return ntStatus;
}