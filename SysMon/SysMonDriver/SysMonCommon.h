#pragma once

enum class ItemType : short {
	None,
	ProcessCreate,
	ProcessExit,
	ThreadCreate,
	ThreadExit,
	ImageLoad
};

// Common field of all events
struct ItemHeader {
	ItemType Type;
	USHORT Size;
	LARGE_INTEGER Time;
};

// Extends the ItemHeader struct
struct ProcessExitInfo : ItemHeader {
	ULONG ProcessId;
};

struct ProcessCreateInfo : ItemHeader {
	ULONG ProcessId;
	ULONG ParentProcessId;
	USHORT ImageLength;
	USHORT ImageOffset;
	USHORT CommandLineLength;
	USHORT CommandLineOffset;
};

struct ThreadCreateExitInfo : ItemHeader {
	ULONG ProcessId;
	ULONG ThreadId;
};

struct LoadImageInfo : ItemHeader {
	ULONG ProcessId;
	PVOID ImageBaseAddress;
	USHORT ImagePathLength;
	USHORT ImagePathOffset;
};
