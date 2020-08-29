#pragma once 

#include "FastMutex.h"
#include "AutoLock.h"

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