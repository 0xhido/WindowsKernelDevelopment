#pragma once

#include "Mutex.h"
#include "FilterFileNameInformation.h"
#include "AutoLock.h"

#define DRIVER_CONTEXT_TAG 'xcbF' // Fbcx - File Backup Context
#define DRIVER_TAG 'pkbF' // Fbkp - File Backup

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

struct FileContext {
    Mutex Lock;
    UNICODE_STRING FileName;
    BOOLEAN Written;
};