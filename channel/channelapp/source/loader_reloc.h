#ifndef _LOADER_RELOC_H_
#define _LOADER_RELOC_H_

#include <gctypes.h>

typedef void (*entry_point) (void);

bool loader_reloc (entry_point *ep, const u8 *addr, u32 size, const char *args,
				   u16 arg_len, bool check_overlap);
void loader_exec (entry_point ep);

#endif

