#ifndef _PLAYTIME_H_
#define _PLAYTIME_H_

#include <gctypes.h>

typedef union {
	struct {
		u32 checksum;
		u16 name[0x28];
		u32 _pad1;
		u64 ticks_boot;
		u64 ticks_last;
		u32 title_id;
		u32 _pad2[5];
	};
	struct {
		u32 _checksum;
		u32 data[0x1f];
	};
} __attribute__((packed)) playtime_t;

void playtime_destroy(void);

#endif

