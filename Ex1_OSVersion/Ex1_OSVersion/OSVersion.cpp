#include <ntddk.h>

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);
	KdPrint(("OSVersion driver Unload called\n"));
}

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);
	
	KdPrint(("OSVersion driver loading\n"));
	DriverObject->DriverUnload = DriverUnload;

	/*	Using RTL_OSVERSIONINFOW osVersionInfo = { sizeof(RTL_OSVERSIONINFOW) };
	*	is also an option because the size with set the first field in the struct
	*	which is dwOSVersionInfoSize, and the rest fields will set to 0.
	*	But I prefer the below option for more clarity.
	*/
	RTL_OSVERSIONINFOW osVersionInfo = { 0 }; 
	osVersionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	RtlGetVersion(&osVersionInfo);

	KdPrint(("OS Version Details:\n"));
	KdPrint(("\tBuild Number: %d\n", osVersionInfo.dwBuildNumber));
	KdPrint(("\tMajor Version: %d\n", osVersionInfo.dwMajorVersion));
	KdPrint(("\tMinor Version: %d\n", osVersionInfo.dwMinorVersion));

	return STATUS_SUCCESS;
}