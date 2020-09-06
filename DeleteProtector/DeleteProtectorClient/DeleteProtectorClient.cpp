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
    printf("Usage: DeleteProtectorClient.exe <method> <object> [name/path]\n");
    
    printf("\tMethod:\n");
    printf("\tadd (must include name/path) - Adds an OBJECT to the black list\n");
    printf("\tremove (must include name/path) - Removes an OBJECT from the black list\n");
    printf("\tclear - Clears the black list\n");

    printf("\tObject:\n");
    printf("\texe - Executable - prevent deletion from that executable name (cmd.exe)\n");
    printf("\tdir - Directory - prevent deletion from that directory path\n");

    printf("NOTE: Directories should be slash(\\) terminaterd\n");
    return 0;
}

int wmain(int argc, const wchar_t* argv[])
{
    if (argc < 3) {
        return PrintUsage();
    }

    const wchar_t* symbolicLink = L"\\\\.\\DeleteProtector";
    HANDLE hDevice = CreateFile(symbolicLink, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE) {
        return Error("Could not open device");
    }

    BOOL ok = true;
    DWORD returned;
    int ioctl;
    
    if (_wcsicmp(argv[1], L"add") == 0) {
        if (argc < 4) {
            return PrintUsage();
        }

        if (_wcsicmp(argv[2], L"exe") == 0) ioctl = IOCTL_DELETEPROTECTOR_ADD_EXE;
        else if (_wcsicmp(argv[2], L"dir") == 0) ioctl = IOCTL_DELETEPROTECTOR_ADD_DIR;
        else return PrintUsage();

        ok = DeviceIoControl(
            hDevice,
            ioctl,
            (PVOID)argv[3], (wcslen(argv[3]) + 1) * sizeof(WCHAR),
            nullptr, 0,
            &returned,
            nullptr);
    }
    else if (_wcsicmp(argv[1], L"remove") == 0) {
        if (argc < 4) {
            return PrintUsage();
        }

        if (_wcsicmp(argv[2], L"exe") == 0) ioctl = IOCTL_DELETEPROTECTOR_REMOVE_EXE;
        else if (_wcsicmp(argv[2], L"dir") == 0) ioctl = IOCTL_DELETEPROTECTOR_REMOVE_DIR;
        else return PrintUsage();

        ok = DeviceIoControl(
            hDevice,
            ioctl,
            (PVOID)argv[3], (wcslen(argv[3]) + 1) * sizeof(WCHAR),
            nullptr, 0,
            &returned,
            nullptr);
    }
    else if (_wcsicmp(argv[1], L"clear") == 0) {
        if (_wcsicmp(argv[2], L"exe") == 0) ioctl = IOCTL_DELETEPROTECTOR_CLEAR;
        else if (_wcsicmp(argv[2], L"dir") == 0) ioctl = IOCTL_DELETEPROTECTOR_CLEAR_DIRS;
        else return PrintUsage();

        ok = DeviceIoControl(hDevice, ioctl, nullptr, 0, nullptr, 0, &returned, nullptr);
    }
    else {
        return PrintUsage();
    }

    
    if (!ok) {
        return Error("Operation failed.");
    }

    CloseHandle(hDevice);
}