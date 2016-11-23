#include "asm.h"
#include "cache.h"
#include "system.h"
#include "processor.h"

extern char systemcallhandler_start[], systemcallhandler_end[];

void __SyscallInit(void)
{
	memcpy((void*)0x80000c00, systemcallhandler_start,
			systemcallhandler_end - systemcallhandler_start);
	DCFlushRangeNoSync((void*)0x80000c00, 0x100);
	ICInvalidateRange((void*)0x80000c00, 0x100);
	_sync();
}

void *memcpy(void *ptr, const void *src, int size) {
	char* ptr2 = ptr;
	const char* src2 = src;
	while(size--) *ptr2++ = *src2++;
	return ptr;
}

int strlen(const char *ptr) {
	int i=0;
	while(*ptr++) i++;
	return i;
}
