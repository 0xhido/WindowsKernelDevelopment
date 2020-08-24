// Booster.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <stdio.h>
#include "../PriorityBoosterDriver/PriorityBoosterCommon.h"

#define THREAD_ID 1
#define PRIORITY 2

int ExitWithError(const char* message) {
	printf("%s (error=%d)\n", message, GetLastError());
	return 1;
}

int main(int argc, const char* argv[])
{
	if (argc < 3) {
		printf("Usage: Booster.exe <threadid> <priority>\n");
		return 0;
	}

	HANDLE hDevice = CreateFile(L"\\\\.\\PriorityBooster", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		ExitWithError("Failed to open device");
	}

	ThreadData data;
	data.ThreadId = atoi(argv[THREAD_ID]);
	data.Priority = atoi(argv[PRIORITY]);

	printf("Setting %d priority to %d\n", data.ThreadId, data.Priority);

	DWORD returned;
	BOOL success = DeviceIoControl(hDevice, IOCTL_PRIORITY_BOOSTER_SET_PRIORITY, &data, sizeof(data), nullptr, 0, &returned, nullptr);
	if (success)
		printf("Priority change succeeded!\n");
	else
		ExitWithError("Priority change failed!");

	CloseHandle(hDevice);
	return 0;
}