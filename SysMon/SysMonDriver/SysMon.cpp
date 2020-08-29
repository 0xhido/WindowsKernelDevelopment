#include "pch.h"
#include "SysMon.h"
#include "SysMonCommon.h"

Globals g_Globals;

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
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG userBufferLength = stack->Parameters.Read.Length;
	ULONG bytesTransfered = 0;

	NT_ASSERT(Irp->MdlAddress);
	UCHAR* buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer) {
		KdPrint((DRIVER_PREFIX "could not get system memroy address\n"));
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	}
	else {
		AutoLock<FastMutex> lock(g_Globals.Mutex);
		while (!IsListEmpty(&g_Globals.ItemsHead)) {
			PLIST_ENTRY entry = RemoveHeadList(&g_Globals.ItemsHead);
			FullItem<ItemHeader>* item = CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry);
			
			ULONG size = sizeof(item->Data.Size);
			if (userBufferLength < size) {
				// User could not get any more items 
				// We need to return the head back to the list
				InsertHeadList(&g_Globals.ItemsHead, entry);
				break;
			}

			g_Globals.ItemCount--;
			memcpy(buffer, &item->Data, size);
			buffer += size;
			userBufferLength -= size;
			bytesTransfered += size;

			ExFreePoolWithTag(item, DRIVER_TAG);
		}
	}

	return CompleteIrp(Irp, ntStatus, bytesTransfered);
}

void OnProcessNotify(_Inout_ PEPROCESS /*Process*/, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo) {
	// Handle process creation
	if (CreateInfo) {
		KdPrint(("Process (%wZ [%ul]) has been created!\n", CreateInfo->ImageFileName, HandleToULong(ProcessId)));
		USHORT allocSize = sizeof(FullItem<ProcessCreateInfo>);
		USHORT imageSize = 0;
		USHORT commandLineSize = 0;

		if (CreateInfo->ImageFileName) {
			imageSize = CreateInfo->ImageFileName->Length;
			allocSize += imageSize;
		}
		if (CreateInfo->CommandLine) {
			commandLineSize = CreateInfo->CommandLine->Length;
			allocSize += commandLineSize;
		}

		FullItem<ProcessCreateInfo>* info = (FullItem<ProcessCreateInfo>*)ExAllocatePoolWithTag(PagedPool, allocSize, DRIVER_TAG);
		if (info == nullptr) {
			KdPrint((DRIVER_PREFIX "process create item allocation failed!\n"));
			return;
		}

		ProcessCreateInfo& item = info->Data;
		// ItemHeader
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessCreate;
		item.Size = sizeof(ProcessCreateInfo);
		// ProcessCreateInfo
		item.ProcessId = HandleToULong(ProcessId);
		item.ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
		
		item.ImageLength = 0;
		item.ImageOffset = sizeof(item);
		if (imageSize > 0) {
			// TODO debug and inspect values of the line below
			memcpy((UCHAR*)&item + item.ImageOffset, CreateInfo->ImageFileName->Buffer, imageSize);
			item.ImageLength = imageSize / sizeof(WCHAR);
		}

		item.CommandLineLength = 0;
		// could also be item.ImageOffset + imageSize
		item.CommandLineOffset = item.ImageOffset + (item.ImageLength * sizeof(WCHAR));
		if (commandLineSize > 0) {
			memcpy((UCHAR*)&item + item.CommandLineOffset, CreateInfo->CommandLine->Buffer, commandLineSize);
			item.CommandLineLength = commandLineSize / sizeof(WCHAR);
		}

		PushItem(&info->Entry);
	}
	// Handle process termination
	else {
		KdPrint(("Process (%ul) has been terminated!\n", HandleToULong(ProcessId)));

		FullItem<ProcessExitInfo>* info = 
			(FullItem<ProcessExitInfo>*)ExAllocatePoolWithTag(PagedPool, sizeof(FullItem<ProcessExitInfo>), DRIVER_TAG);
		if (info == nullptr) {
			KdPrint((DRIVER_PREFIX "process exit item allocation failed!\n"));
			return;
		}

		ProcessExitInfo& item = info->Data;
		// ItemHeader
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessExit;
		item.Size = sizeof(ProcessExitInfo);
		// ProcessExitInfo
		item.ProcessId = HandleToULong(ProcessId);

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

ULONG GetDwordValueFromRegistry(PUNICODE_STRING RegistryPath, PUNICODE_STRING ValueName) {
	NTSTATUS ntStatus = STATUS_SUCCESS;
	HANDLE hRegistryKey;
	PKEY_VALUE_FULL_INFORMATION pKeyInfo = nullptr;
	ULONG value = 0;
	
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
		ULONG ulKeyInfoSizeNeeded = GetKeyInfoSize(hRegistryKey, ValueName);
		if (ulKeyInfoSizeNeeded == 0) {
			KdPrint((DRIVER_PREFIX "Max item count value not found\n"));
			break;
		}
		ulKeyInfoSize = ulKeyInfoSizeNeeded;

		pKeyInfo = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePoolWithTag(PagedPool, ulKeyInfoSize, DRIVER_TAG);
		if (pKeyInfo == nullptr) {
			KdPrint((DRIVER_PREFIX "Could not allocate memory for KeyValueInfo\n"));
			break;
		}
		RtlZeroMemory(pKeyInfo, ulKeyInfoSize);

		ntStatus = ZwQueryValueKey(hRegistryKey, ValueName, KeyValueFullInformation, pKeyInfo, ulKeyInfoSize, &ulKeyInfoSizeNeeded);
		if (!NT_SUCCESS(ntStatus) || ulKeyInfoSize != ulKeyInfoSizeNeeded) {
			KdPrint((DRIVER_PREFIX "Registry value querying failed (0x%08X)\n", ntStatus));
			break;
		}

		value = *(ULONG*)((ULONG_PTR)pKeyInfo + pKeyInfo->DataOffset);
	} while (false);

	if (hRegistryKey) {
		ZwClose(hRegistryKey);
	}

	if (pKeyInfo) {
		ExFreePoolWithTag(pKeyInfo, DRIVER_TAG);
	}

	if (!NT_SUCCESS(ntStatus)) {
		KdPrint((DRIVER_PREFIX "Setting max item count to default value\n"));
		return 0;
	}

	return value;
}

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PDEVICE_OBJECT DeviceObject = nullptr;
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\SysMon");
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\SysMon");
	UNICODE_STRING maxItemCountName = RTL_CONSTANT_STRING(L"MaxItemCount");

	bool symLinkCreated = false;

	// Initailize global members
	InitializeListHead(&g_Globals.ItemsHead);
	g_Globals.Mutex.Init();

	// Get max items count from registry
	ULONG maxItemCount = GetDwordValueFromRegistry(RegistryPath, &maxItemCountName);
	if (maxItemCount == 0)
		maxItemCount = DEFAULT_MAX_ITEMS_COUNT;
	g_Globals.MaxItemsCount = maxItemCount;
	
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