// SysMonClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include "../SysMonDriver/SysMonCommon.h"
#include "SysMonClient.h"
#include <map>

std::map<int, std::wstring> g_ProcessesImage;

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

            g_ProcessesImage[info->ProcessId] = Image;

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

        case ItemType::ThreadCreate: {
            DisplayTime(header->Time);
            ThreadCreateExitInfo* info = (ThreadCreateExitInfo*)buffer;
            printf("Thread %d created:\n", info->ThreadId);
            printf("\tProcess: %ws (%d)\n", g_ProcessesImage[info->ProcessId].c_str(), info->ProcessId);

            break;
        }

        case ItemType::ThreadExit: {
            DisplayTime(header->Time);
            ThreadCreateExitInfo* info = (ThreadCreateExitInfo*)buffer;
            printf("Thread %d exited:\n", info->ThreadId);
            printf("\tProcess: %ws (%d)\n", g_ProcessesImage[info->ProcessId].c_str(), info->ProcessId);

            break;
        }

        case ItemType::ImageLoad: {
            DisplayTime(header->Time);
            LoadImageInfo* info = (LoadImageInfo*)buffer;
            std::wstring imageName((WCHAR*)(buffer + info->ImagePathOffset), info->ImagePathLength);

            printf("Image loaded:\n");
            printf("\tProcess = %ws (%d)\n", g_ProcessesImage[info->ProcessId].c_str(), info->ProcessId);
            printf("\tImageName = %ws\n", imageName.c_str());
            printf("\tImageBaseAddress = %p\n", info->ImageBaseAddress);

            break;
        }
        
        default:
            break;
        }

        buffer += header->Size;
        count -= header->Size;
    }
}

void PrintUsageAndExit() {
    printf("Usage: Sysmon.exe <command> [<parameter>]\n");
    printf("\tcommand - add | remove | clear | monitor | get\n");
    printf("\tparameter - image full path\n");
    printf("\tmonitor and get don't need a parameter\n");
    exit(1);
}

int wmain(int argc, wchar_t* argv[]) 
{
    if (argc < 2) {
        PrintUsageAndExit();
    }
    
    std::wstring command(argv[1]);

    BYTE buffer[4096];
    HANDLE hFile = CreateFile(L"\\\\.\\SysMon", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return Error("Could not open file");
    }

    if (!wcscmp(command.c_str(), L"monitor")) {
        while (true) {
            DWORD bytesRead;
            if (!ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr)) {
                return Error("Could not read file");
            }

            if (bytesRead != 0)
                DisplayItems(buffer, bytesRead);

            Sleep(200);
        }
    }
    else if (_wcsicmp(argv[1], L"add") == 0) {
        if (argc < 3) {
            PrintUsageAndExit();
        }

        /*std::wstring image(argv[2]);
        DWORD allocSize = sizeof(BlackListItem) + (image.capacity() * sizeof(WCHAR));

        BlackListItem* item = (BlackListItem*)calloc(allocSize, sizeof(char));
        if (item == NULL) {
            return Error("Could not create buffer for image");
        }

        item->ImageLength = image.capacity();
        item->ImageOffset = sizeof(BlackListItem);
        memcpy((UCHAR*)item + item->ImageOffset, image.c_str(), image.capacity() * sizeof(WCHAR));*/

        DWORD bytesWritten;
        if (!WriteFile(hFile, (PVOID)argv[2], ((DWORD)wcslen(argv[2]) + 1) * sizeof(WCHAR), &bytesWritten, nullptr)) {
            return Error("Add image failed");
        }
    }
}