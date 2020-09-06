#pragma once

#include "DeleteProtectorCommon.h"
#include "FastMutex.h"
#include "AutoLock.h"
#include "kstring.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

#define DRIVER_TAG 'pled'

struct DirectoryEntry {
    UNICODE_STRING DosName;
    UNICODE_STRING NtName;

    void Free() {
        if (DosName.Buffer) {
            ExFreePoolWithTag(DosName.Buffer, DRIVER_TAG);
            DosName.Buffer = nullptr;
            DosName.Length = DosName.MaximumLength = 0;
        }
        if (NtName.Buffer) {
            ExFreePoolWithTag(NtName.Buffer, DRIVER_TAG);
            NtName.Buffer = nullptr;
            NtName.Length = NtName.MaximumLength = 0;
        }
    }
};