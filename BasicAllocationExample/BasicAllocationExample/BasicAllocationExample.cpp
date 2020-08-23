#include <ntddk.h>

#define DEBUG_TAG 'dcba'

UNICODE_STRING gRegistyPath;

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);

	ExFreePool(gRegistyPath.Buffer);

	KdPrint(("BasicAllocationExample driver Unload called\n"));
}

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	KdPrint(("BasicAllocationExample driver loading\n"));
	DriverObject->DriverUnload = DriverUnload;
	
	gRegistyPath.Buffer = (WCHAR*)ExAllocatePoolWithTag(POOL_TYPE::PagedPool, RegistryPath->Length, DEBUG_TAG);
	if (gRegistyPath.Buffer == nullptr) {
		KdPrint(("Failed to allocate memory\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	gRegistyPath.MaximumLength = RegistryPath->MaximumLength;
	RtlCopyUnicodeString(&gRegistyPath, (PCUNICODE_STRING)RegistryPath);
	KdPrint(("RegistryPath copied: %wZ\n", &gRegistyPath));
	
	return STATUS_SUCCESS;
}