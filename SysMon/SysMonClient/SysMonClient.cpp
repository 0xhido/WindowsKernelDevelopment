// SysMonClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include "../SysMonDriver/SysMonCommon.h"
#include "SysMonClient.h"

int Error(const char* msg) {
    printf("%s: error=%d\n", msg, GetLastError());
    return 1;
}

void DisplayTime(const LARGE_INTEGER& time) {
    SYSTEMTIME systemTime;
    
    int ok = FileTimeToSystemTime((FILETIME*)&time, &systemTime);
    if (!ok) {
        Error("Could not convert time to system time");
        return;
    }

    printf("%02d:%02d:%02d.%03d: ", 
        systemTime.wHour,
        systemTime.wMinute,
        systemTime.wSecond,
        systemTime.wMilliseconds);
}

void DisplayItems(const BYTE* buffer, const DWORD bytes) {
    DWORD count = bytes;
    while (count > 0) {
        ItemHeader* header = (ItemHeader*)buffer;

        switch (header->Type) {
        case ItemType::ProcessCreate: {
            DisplayTime(header->Time);
            ProcessCreateInfo* info = (ProcessCreateInfo*)buffer;
            std::wstring Image((WCHAR*)(buffer + info->ImageOffset), info->ImageLength);
            std::wstring CommandLine((WCHAR*)(buffer + info->CommandLineOffset), info->CommandLineLength);

            printf("Process %d created:\n", info->ProcessId);
            printf("\tImage = %ws\n", Image.c_str());
            printf("\tCommandLine = %ws\n", CommandLine.c_str());
            printf("\tParent Process ID = %d\n", info->ParentProcessId);
            break;
        }
            
        case ItemType::ProcessExit: {
            DisplayTime(header->Time);
            ProcessExitInfo* info = (ProcessExitInfo*)buffer;
            printf("Process %d exited\n", info->ProcessId);
            break;
        }
        
        default:
            break;
        }

        buffer += header->Size;
        count -= header->Size;
    }
}

int main()
{
    HANDLE hFile = CreateFile(L"\\\\.\\SysMon", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return Error("Could not open file");
    }

    BYTE buffer[4096];

    while (true) {
        DWORD bytesRead;
        if (!ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr)) {
            return Error("Could not read file");
        }

        if (bytesRead != 0)
            DisplayItems(buffer, bytesRead);

        Sleep(200);
    }

    std::cout << "Hello World!\n";
}