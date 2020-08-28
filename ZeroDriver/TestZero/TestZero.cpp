// TestZero.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdio.h>
#include <windows.h>
#include "../ZeroDriver/ZeroDriverCommon.h"

int ExitWithError(const char* msg) {
	printf("%s: error=%d\n", msg, GetLastError());
	return 1;
}

int TestReadFile(HANDLE hDevice) {
	printf("Starting read test: \n");
	
	BYTE buffer[64];
	for (int i = 0; i < sizeof(buffer); i++)
		buffer[i] = i + 1;


	DWORD bytesRead;
	BOOL ok = ReadFile(hDevice, buffer, sizeof(buffer), &bytesRead, 0);
	if (!ok) {
		ExitWithError("ReadFile failed!");
	}

	if (bytesRead != sizeof(buffer))
		printf("\tWrong number of bytes read\n");

	long total = 0;
	for (BYTE b : buffer)
		total += b;
	if (total != 0)
		printf("\tWrong values (No bytes read)\n");

	printf("\tRead test done.\n");
	return 0;
}

int TestWriteFile(HANDLE hDevice) {
	printf("Starting write test: \n");

	BYTE buffer[1024];
	for (int i = 0; i < sizeof(buffer); i++)
		buffer[i] = i + 1;
	
	DWORD bytesWritten;
	BOOL ok = WriteFile(hDevice, buffer, sizeof(buffer), &bytesWritten, 0);
	if (!ok) {
		return ExitWithError("WriteFile failed!");
	}

	if (bytesWritten != sizeof(buffer))
		printf("\tWrong number of bytes written\n");

	printf("\tWrite test done.\n");
	return 0;
}

int main() 
{
	HANDLE hDevice = CreateFile(L"\\\\.\\Zero", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		return ExitWithError("Open device failed!");
	}

	int status = 0;
	status += TestReadFile(hDevice);
	status += TestWriteFile(hDevice);
	
	printf("Tests done.\n");

	if (status == 0) {
		printf("Quering bytes\n");

		Bytes input = { 10, 199 };
		Bytes result = { 10, 100 };
		DWORD bytes = 0;
		BOOL ok = DeviceIoControl(hDevice,
			IOCTL_ZERO_QUERY_BYTES,
			&input, sizeof(input),
			&result, sizeof(result),
			&bytes,
			NULL);

		if (!ok) {
			return ExitWithError("Command failed");
		}
		
		printf("\tBytes Read: %lld\n", result.Read);
		printf("\tBytes Written: %lld\n", result.Write);
	}

	CloseHandle(hDevice);

	return status;
}