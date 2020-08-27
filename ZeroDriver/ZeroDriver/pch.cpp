#include "pch.h"

#define DRIVER_PREFIX "[ZeroDriver] " 

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR inforamtion = 0) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = inforamtion;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	
	return status;
}

void ZeroUnloadDriver(_In_ PDRIVER_OBJECT /*DriverObject*/) {
	KdPrint(("Zero driver unloaded successfully\n"));
}

NTSTATUS ZeroIrpMajorCreateClose(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp) {
	return CompleteIrp(Irp);
}

NTSTATUS ZeroIrpMajorRead(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp) {
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

	// Validate user request
	ULONG readLength = stack->Parameters.Read.Length;
	if (readLength == 0) {
		return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE);
	}

	// Map MDL to SYSTEM address
	PVOID pBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!pBuffer) {
		return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES);
	}

	// Implement the driver's functionality - zero out the buffer
	memset(pBuffer, 0, readLength);

	// Set the Information to be the size of bytes' read
	return CompleteIrp(Irp, STATUS_SUCCESS, readLength);
}

NTSTATUS ZeroIrpMajorWrite(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp) {
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG writeLength = stack->Parameters.Write.Length;

	// Just set Information to be the length of bytes provided by the user 
	return CompleteIrp(Irp, STATUS_SUCCESS, writeLength);
}

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING /*RegistryPath*/) {
	NTSTATUS status = STATUS_SUCCESS;

	// Setting unload routine
	DriverObject->DriverUnload = ZeroUnloadDriver;
	
	// Setting dispatch functions
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ZeroIrpMajorCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = ZeroIrpMajorCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ZeroIrpMajorRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroIrpMajorWrite;

	// Create device object and symbolic link
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\Zero");
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\Zero");

	PDEVICE_OBJECT DeviceObject = nullptr;
	
	// do-while pattern for clean error handling
	do {
		status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create device (0x%08X)\n", status));
			break;
		}

		// Set Direct I/O flag
		DeviceObject->Flags |= DO_DIRECT_IO;

		status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create symbolic link to %wZ (0x%08X)\n", deviceName, status));
			break;
		}
	} while (false);

	// Check for errors occured inside the do-while loop
	if (!NT_SUCCESS(status)) {
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
	}

	KdPrint(("Zero driver DriverEntry finished!\n"));
	return status;
}