#include "pch.h"
#include "ZeroDriverCommon.h"

#define DRIVER_PREFIX "[ZeroDriver] " 

long long g_ReadCount;
long long g_WriteCount;

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR inforamtion = 0) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = inforamtion;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	
	return status;
}

void ZeroUnloadDriver(_In_ PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	IoDeleteSymbolicLink(&symbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);

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
	InterlockedAdd64(&g_ReadCount, readLength);

	// Set the Information to be the size of bytes' read
	return CompleteIrp(Irp, STATUS_SUCCESS, readLength);
}

NTSTATUS ZeroIrpMajorWrite(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp) {
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	
	ULONG writeLength = stack->Parameters.Write.Length;
	InterlockedAdd64(&g_WriteCount, writeLength);

	// Just set Information to be the length of bytes provided by the user 
	return CompleteIrp(Irp, STATUS_SUCCESS, writeLength);
}

NTSTATUS ZeroIrpMajorDeviceControl(_In_ PDEVICE_OBJECT /*DeviceObject*/, _In_ PIRP Irp) {
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	
	ULONG IoControlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	if (IoControlCode != IOCTL_ZERO_QUERY_BYTES) {
		return CompleteIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}

	ULONG bufferSize = stack->Parameters.DeviceIoControl.OutputBufferLength;
	if (bufferSize < sizeof(Bytes)) {
		return CompleteIrp(Irp, STATUS_BUFFER_TOO_SMALL);
	}

	Bytes* state = (Bytes*)Irp->AssociatedIrp.SystemBuffer;
	state->Read = g_ReadCount;
	state->Write = g_WriteCount;

	return CompleteIrp(Irp, STATUS_SUCCESS, sizeof(*state));
}

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING /*RegistryPath*/) {
	NTSTATUS status = STATUS_SUCCESS;
	g_ReadCount = 0;
	g_WriteCount = 0;

	// Setting unload routine
	DriverObject->DriverUnload = ZeroUnloadDriver;
	
	// Setting dispatch functions
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ZeroIrpMajorCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = ZeroIrpMajorCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ZeroIrpMajorRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroIrpMajorWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ZeroIrpMajorDeviceControl;

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