#pragma once

#include "AutoLock.h"
#include "FastMutex.h"

template<typename T, typename TLock = FastMutex>
class LinkedList
{
public:
	void Init() {
		InitializeListHead(&_head);
		_lock.Init();
	}

	// expects a LIST_ENTRY named "Entry"
	void PushBack(T* item) {
		AutoLock<TLock> locker(_lock);
		InsertTailList(&_head, &item->Entry);
	}

	void PushFront(T* item) {
		AutoLock<TLock> locker(_lock);
		InsertHeadList(&_head, &item->Entry);
	}

	PLIST_ENTRY GetHead() {
		AutoLock<TLock> locker(_lock);
		return &_head;
	}

	T* RemoveHead() {
		AutoLock<TLock> locker(_lock);
		PLIST_ENTRY entry = RemoveHeadList(&_head);
		return CONTAINING_RECORD(entry, T, Entry);
	}

	void RemoveItem(T* item) {
		AutoLock<TLock> locker(_lock);
		PLIST_ENTRY entry = RemoveEntryList(&item->Entry);
		return CONTAINING_RECORD(entry, T, Entry);
	}
private:
	LIST_ENTRY _head;
	TLock _lock;
};

