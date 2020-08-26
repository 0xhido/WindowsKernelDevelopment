#include <ntifs.h>
#include <ntddk.h>
#include "PriorityBoosterCommon.h"

// ===========================
// Fucntions Declerations
// ===========================

void BoosterUnload(_In_ PDRIVER_OBJECT DriverObject);
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

// ===========================
// Fucntions Implementations 
// ===========================


void BoosterUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");

	IoDeleteSymbolicLink(&symbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);

	KdPrint(("PriorityBoosterDriver unload successfully\n"));
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(PDEVICE_OBJECT /*DeviceObject*/, PIRP Irp) {
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
		case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY: {
			if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadData)) {
				status = STATUS_BUFFER_TOO_SMALL;
				KdPrint(("Input buffer size too small\n"));
				break;
			}

			ThreadData* pData = (ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
			if (pData == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				KdPrint(("Invalid parameter\n"));
				break;
			}

			if (pData->Priority < 1 || pData->Priority > 31) {
				status = STATUS_INVALID_PARAMETER;
				KdPrint(("Invalid parameter\n"));
				break;
			}

			PETHREAD Thread;
			HANDLE hThreadId = ULongToHandle(pData->ThreadId);
			status = PsLookupThreadByThreadId(hThreadId, &Thread);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not find the thread (according to the thread id %d\n", pData->ThreadId));
				break;
			}

			KeSetPriorityThread((PKTHREAD)Thread, pData->Priority);
			ObDereferenceObject(Thread);
			KdPrint(("Thread priority change for %d to %d succeeded!\n", pData->ThreadId, pData->Priority));

			break;
		}
		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterCreateClose(PDEVICE_OBJECT /*DeviceObject*/, PIRP Irp) {
	NTSTATUS status = STATUS_SUCCESS;

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING /*RegistryPath*/) {
	KdPrint(("Start loading PriorityBoosterDriver\n"));

	// 1. Set Unload function
	DriverObject->DriverUnload = BoosterUnload;

	// 2. Set dispatch routies
	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	// 3. Create DeviceObject
	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");

	NTSTATUS status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to create device object (0x%08X)\n", status));
		return status;
	}

	// 4. Create SymbolicLink
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");

	status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to create symbolic link (0x%08X)\n", status));
		KdPrint(("Deleting device object\n"));

		IoDeleteDevice(DeviceObject);

		return status;
	}

	KdPrint(("PriorityBooster DriverEntry completed successfully\n"));

	return STATUS_SUCCESS;
}