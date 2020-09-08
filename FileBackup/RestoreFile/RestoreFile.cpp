// RestoreFile.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <stdio.h>
#include <string>

int PrintUsage() {
    printf("Usage: RestoreFile.exe <filename>\n");
    return 0;
}

int ErrorExit(const wchar_t* lpFunction)
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
    return 1;
}


int wmain(int argc, wchar_t* argv[]) {
	if (argc < 2) {
        return PrintUsage();
	}

    std::wstring stream(argv[1]);
    stream += L":backup";

    HANDLE hSource = CreateFile(stream.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hSource == INVALID_HANDLE_VALUE) {
        ErrorExit(L"Backup stream CreateFile");
    }

    HANDLE hTarget = CreateFile(argv[1], GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hSource == INVALID_HANDLE_VALUE) {
        ErrorExit(L"Target base CreateFile");
    }

    LARGE_INTEGER size;
    GetFileSizeEx(hSource, &size);
    LARGE_INTEGER eofSize = size;

    LONGLONG chunkSize = 1 << 21; // 2 MB
    ULONG bufferSize = (ULONG)min(chunkSize, size.QuadPart);

    PVOID buffer = VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) {
        CloseHandle(hSource);
        CloseHandle(hTarget);
        return ErrorExit(L"Buffer VirtualAlloc");
    }
    
    DWORD bytes;
    bool ok = true;
    std::wstring msg;
    while (size.QuadPart > 0) {
        ok = ReadFile(hSource, buffer, (DWORD)min((LONGLONG)bufferSize, size.QuadPart), &bytes, nullptr);
        if (!ok) {
            msg = L"ReadFile";
            break;
        }

        ok = WriteFile(hTarget, buffer, bytes, &bytes, nullptr);
        if (!ok) {
            msg = L"WriteFile";
            break;
        }

        size.QuadPart -= bytes;
    }

    if (!ok) {
        CloseHandle(hSource);
        CloseHandle(hTarget);
        VirtualFree(buffer, 0, MEM_RELEASE);
        return ErrorExit(msg.c_str());
    }

    FILE_END_OF_FILE_INFO info;
    info.EndOfFile = eofSize;
    ok = SetFileInformationByHandle(hTarget, FileEndOfFileInfo, &info, sizeof(FILE_END_OF_FILE_INFO));
    if (!ok) {
        CloseHandle(hSource);
        CloseHandle(hTarget);
        VirtualFree(buffer, 0, MEM_RELEASE);
        return ErrorExit(msg.c_str());
    }


    printf("Restore has done successfully!\n");

    CloseHandle(hSource);
    CloseHandle(hTarget);
    VirtualFree(buffer, 0, MEM_RELEASE);

    return 0;
}