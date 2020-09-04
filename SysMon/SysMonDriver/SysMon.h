#pragma once 

#include "FastMutex.h"
#include "AutoLock.h"
#include "kstring.h"

#define DRIVER_PREFIX "[Sysmon] "
#define DRIVER_TAG 'nmys'
#define DEFAULT_MAX_ITEMS_COUNT 1024

template<typename T>
struct FullItem {
	LIST_ENTRY Entry;
	T Data;
};

struct Globals {
	LIST_ENTRY ItemsHead;
	LIST_ENTRY ProcessBlackListHead;
	int ItemCount;
	int MaxItemsCount;
	FastMutex Mutex;
};

struct BlackList {
	LIST_ENTRY Head;
	USHORT Size;
};

struct BlackListItem {
	LIST_ENTRY Entry;
	UNICODE_STRING Image;
};