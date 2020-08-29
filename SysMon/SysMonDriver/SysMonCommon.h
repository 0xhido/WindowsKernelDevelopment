#pragma once

enum class ItemType : short {
	None,
	ProcessCreate,
	ProcessExit
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