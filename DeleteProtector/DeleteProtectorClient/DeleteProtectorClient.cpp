// DeleteProtectorClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include "../DeleteProtectorDriver/DeleteProtectorCommon.h"

int Error(const char* text) {
    printf("%s (%d)\n", text, ::GetLastError());
    return 1;
}

int PrintUsage() {
    printf("Usage: DeleteProtectorClient.exe <method> [exe]\n");
    printf("\tMethod:\n");
    printf("\tadd (must include exe) - Adds an EXE to the black list\n");
    printf("\tremove (must include exe) - Removes an EXE from the black list\n");
    printf("\tclear - Clears the black list\n");
    return 0;
}

int wmain(int argc, const wchar_t* argv[])
{
    if (argc < 2) {
        return PrintUsage();
    }

    const wchar_t* symbolicLink = L"\\\\.\\DeleteProtector";
    HANDLE hDevice = CreateFile(symbolicLink, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE) {
        return Error("Could not open device");
    }

    BOOL ok = true;
    DWORD returned;

    if (_wcsicmp(argv[1], L"add") == 0) {
        if (argc < 3) {
            return PrintUsage();
        }

        ok = DeviceIoControl(
            hDevice, 
            IOCTL_DELETEPROTECTOR_ADD_EXE, 
            (PVOID)argv[2], (wcslen(argv[2]) + 1) * sizeof(WCHAR), 
            nullptr, 0, 
            &returned, 
            nullptr);
    }
    else if (_wcsicmp(argv[1], L"remove") == 0) {
        if (argc < 3) {
            return PrintUsage();
        }

        ok = DeviceIoControl(
            hDevice, 
            IOCTL_DELETEPROTECTOR_REMOVE_EXE, 
            (PVOID)argv[2], (wcslen(argv[2]) + 1) * sizeof(WCHAR), 
            nullptr, 0, 
            &returned, 
            nullptr);
    }
    else if (_wcsicmp(argv[1], L"clear") == 0) {
        ok = DeviceIoControl(hDevice, IOCTL_DELETEPROTECTOR_CLEAR, nullptr, 0, nullptr, 0, &returned, nullptr);
    }
    else {
        return PrintUsage();
    }

    if (!ok) {
        return Error("Operation failed.");
    }

    CloseHandle(hDevice);
}