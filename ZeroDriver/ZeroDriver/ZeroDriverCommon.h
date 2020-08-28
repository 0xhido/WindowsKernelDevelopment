#pragma once

#define ZERO_DEVICE 0x8001

#define IOCTL_ZERO_QUERY_BYTES CTL_CODE(ZERO_DEVICE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct Bytes {
	long long Read;
	long long Write;
};
