// DeleteProtectorTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <strsafe.h>

int PrintUsage() {
    printf("Usage: DeleteProtectorTest.exe <method> <filename>\n");
    printf("\tMethod:\n");
    printf("\t1 - delete using DeleteFile\n");
    printf("\t2 - delete using CreateFile with DELETE_ON_CLOSE flag\n");
    printf("\t3 - delete using SetFileInformation\n");
    return 0;
}

void ErrorExit(const wchar_t* lpFunction)
{
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    printf("%ls failed - %ls\n", lpFunction, (wchar_t*)lpMsgBuf);
    
    LocalFree(lpMsgBuf);
    ExitProcess(dw);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 3) {
        return PrintUsage();
    }

    int method = _wtoi(argv[1]);
    switch (method)
    {
    case 1: {
        BOOL deleted = DeleteFile(argv[2]);
        if (deleted) {
            printf("File has been deleted using DeleteFile\n");
        }
        else {
            ErrorExit(L"DeleteFile");
        }

        break;
    }

    case 2: {
        BOOL deleted = false;
        HANDLE hFile = CreateFile(argv[2], DELETE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            deleted = CloseHandle(hFile);
            if (deleted) {
                printf("File has been deleted using CreateFile\n");
            }
        }

        if (!deleted)
            ErrorExit(L"CreateFile");

        break;
    }

    case 3: {
        BOOL deleted = false;
        FILE_DISPOSITION_INFO info = { 0 };
        info.DeleteFile = TRUE;

        HANDLE hFile = CreateFile(argv[2], DELETE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            deleted = SetFileInformationByHandle(hFile, FileDispositionInfo, &info, sizeof(FILE_DISPOSITION_INFO));
            if (deleted) {
                printf("File has been deleted using SetFileInformationByHandle\n");
            }

            CloseHandle(hFile);
        }

        if (!deleted)
            ErrorExit(L"CreateFile");
        
        break;
    }

    default:
        printf("Unsupported option\n");
        PrintUsage();
        break;
    }
}