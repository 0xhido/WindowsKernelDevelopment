#include "pch.h"
#include "SysMon.h"
#include "SysMonCommon.h"
#include "LinkedList.h"
#include "kstring.h"

#define IMAGE_PREFIX L"\\??\\"
#define IMAGE_PREFIX_BYTES 8

Globals g_Globals;
LinkedList<BlackListItem, FastMutex> g_BlackList;

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
			
			ULONG size = item->Data.Size;
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

NTSTATUS SysMonIrpWrite(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp) {
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG bytesFromUser = stack->Parameters.Write.Length;
	ULONG bytesWritten = 0;

	if (bytesFromUser < 3) {
		KdPrint((DRIVER_PREFIX "Invalid image name size\n"));
		ntStatus = STATUS_INVALID_PARAMETER;
	}
	else {
		BlackListItem* item = static_cast<BlackListItem*>(ExAllocatePoolWithTag(PagedPool, sizeof(BlackListItem), DRIVER_TAG));
		if (item == nullptr) {
			KdPrint((DRIVER_PREFIX "could not allocate memory for BlackListItem\n"));
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			return CompleteIrp(Irp, ntStatus);
		}
		memset(item, 0, sizeof(BlackListItem));

		NT_ASSERT(Irp->MdlAddress);
		WCHAR* buffer = (WCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
		if (!buffer) {
			KdPrint((DRIVER_PREFIX "could not get system memroy address\n"));
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		}
		else {
			kstring imageString(IMAGE_PREFIX);
			imageString.Append(buffer, bytesFromUser);

			UNICODE_STRING imageUnicodeString = { 0 };
			imageString.GetUnicodeString(&imageUnicodeString);
			if (g_BlackList.IsExist(&imageUnicodeString) == false) {
				item->Image.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, imageString.Length(), DRIVER_TAG);
				if (!item->Image.Buffer) {
					KdPrint((DRIVER_PREFIX "could allocate memory for image name buffer\n"));
					ntStatus = STATUS_INSUFFICIENT_RESOURCES;
				}
				else {
					wcscpy_s(item->Image.Buffer, imageString.Length(), imageString.Get());
					item->Image.Length = (USHORT)(wcslen(item->Image.Buffer) * sizeof(WCHAR));
					item->Image.MaximumLength = (USHORT)(wcslen(item->Image.Buffer) * sizeof(WCHAR));
				}
				g_BlackList.PushFront(item);

				bytesWritten += bytesFromUser;
			}
		}
	}
	
	return CompleteIrp(Irp, ntStatus, bytesWritten);
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

			if (g_BlackList.IsExist(CreateInfo->ImageFileName) == true) {
				KdPrint(("File not allowed to Execute: %ws\n", CreateInfo->ImageFileName->Buffer));
				CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
				return;
			}
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
		item.Size = sizeof(ProcessCreateInfo) + imageSize + commandLineSize;
		// ProcessCreateInfo
		item.ProcessId = HandleToULong(ProcessId);
		item.ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
		
		item.ImageLength = 0;
		item.ImageOffset = sizeof(item);
		if (imageSize > 0) {
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

void OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create) {
	auto info = (FullItem<ThreadCreateExitInfo>*)ExAllocatePoolWithTag(
		PagedPool,
		sizeof(FullItem<ThreadCreateExitInfo>),
		DRIVER_TAG);
	if (info == nullptr) {
		KdPrint((DRIVER_PREFIX "Thread item allocation failed!\n"));
		return;
	}

	ThreadCreateExitInfo& item = info->Data;
	// ItemHeader
	KeQuerySystemTimePrecise(&item.Time);
	item.Type = Create ? ItemType::ThreadCreate : ItemType::ThreadExit;
	item.Size = sizeof(ThreadCreateExitInfo);
	// Thread Create/Exit
	item.ProcessId = HandleToULong(ProcessId);
	item.ThreadId = HandleToULong(ThreadId);

	/*HANDLE CurrentProcessId = PsGetCurrentProcessId();
	if (HandleToULong(ProcessId) != HandleToULong(CurrentProcessId))
		KdPrint(("Remote thread created from %d to %d\n", HandleToULong(CurrentProcessId), HandleToULong(ProcessId)));*/

	PushItem(&info->Entry);
}

void OnLoadImage(_In_opt_ PUNICODE_STRING FullImageName, _In_ HANDLE ProcessId, _In_ PIMAGE_INFO ImageInfo) {
	if (ProcessId == nullptr)
		return; // system image - ignore

	USHORT allocSize = sizeof(FullItem<LoadImageInfo>);
	USHORT imageNameSize = 0;

	if (FullImageName) {
		imageNameSize = FullImageName->Length;
		allocSize += imageNameSize;
	}

	auto info = (FullItem<LoadImageInfo>*)ExAllocatePoolWithTag(PagedPool, allocSize, DRIVER_TAG);
	if (info == nullptr) {
		KdPrint((DRIVER_PREFIX "LoadImage item allocation failed!\n"));
		return;
	}

	LoadImageInfo& item = info->Data;
	// ItemHeader
	KeQuerySystemTimePrecise(&item.Time);
	item.Type = ItemType::ImageLoad;
	item.Size = sizeof(LoadImageInfo) + imageNameSize;
	// LoadImageInfo
	item.ProcessId = HandleToULong(ProcessId);
	item.ImageBaseAddress = ImageInfo->ImageBase;

	item.ImagePathLength = 0;
	item.ImagePathOffset = sizeof(item);
	if (imageNameSize > 0) {
		memcpy((UCHAR*)&item + item.ImagePathOffset, FullImageName->Buffer, imageNameSize);
		item.ImagePathLength = imageNameSize / sizeof(WCHAR);
	}

	PushItem(&info->Entry);
}

void SysMonUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\SysMon");

	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
	PsRemoveLoadImageNotifyRoutine(OnLoadImage);

	IoDeleteSymbolicLink(&symbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);

	while (!IsListEmpty(&g_Globals.ItemsHead)) {
		auto entry = RemoveHeadList(&g_Globals.ItemsHead);
		ExFreePool(CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry));
	}

	while (!IsListEmpty(g_BlackList.GetHead())) {
		BlackListItem* entry = g_BlackList.RemoveHead();
		if (entry->Image.Buffer) {
			ExFreePoolWithTag(entry->Image.Buffer, DRIVER_TAG);
		}
		ExFreePoolWithTag(entry, DRIVER_TAG);
	}

	KdPrint(("SysMon driver unloaded successfully\n"));
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
	g_BlackList.Init();

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
		}

		ntStatus = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(ntStatus)) {
			KdPrint((DRIVER_PREFIX "Thread notify registration failed (0x%08X)\n", ntStatus));
		}

		ntStatus = PsSetLoadImageNotifyRoutine(OnLoadImage);
		if (!NT_SUCCESS(ntStatus)) {
			KdPrint((DRIVER_PREFIX "Load Image notify registration failed (0x%08X)\n", ntStatus));
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
	DriverObject->MajorFunction[IRP_MJ_WRITE] = SysMonIrpWrite;
	
	KdPrint(("SysMon driver DriverEntry finished!\n"));

	return ntStatus;
}