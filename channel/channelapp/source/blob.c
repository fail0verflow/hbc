#include <ogcsys.h>
#include <ogc/machine/processor.h>

#include "../config.h"
#include "debug.h"
#include "panic.h"
#include "blob.h"

#define MAX_BLOBS 8
#define BLOB_MINSLACK (512*1024)

typedef struct {
	void *the_blob;
	size_t blob_size;
	void *old_arena2hi;
} blob_t;

blob_t blobs[MAX_BLOBS];
int num_blobs = 0;

// supports only stack-type allocs (free last alloced)
void *blob_alloc(size_t size)
{
	u32 level;
	u32 addr;
	void *old_arena2hi;

	_CPU_ISR_Disable(level);
	if (num_blobs >= MAX_BLOBS) {
		_CPU_ISR_Restore(level);
		gprintf("too many blobs\n");
		panic();
	}

	old_arena2hi = SYS_GetArena2Hi();
	addr = (((u32)old_arena2hi) - size) & (~0x1f);

	if (addr < (BLOB_MINSLACK + (u32)SYS_GetArena2Lo())) {
		_CPU_ISR_Restore(level);
		return NULL;
	}

	blobs[num_blobs].old_arena2hi = old_arena2hi;
	blobs[num_blobs].the_blob = (void*)addr;
	blobs[num_blobs].blob_size = size;
	num_blobs++;

	SYS_SetArena2Hi((void*)addr);
	_CPU_ISR_Restore(level);
	gprintf("allocated blob size %d at 0x%08lx\n", size, addr);
	return (void*)addr;
}

void blob_free(void *p)
{
	u32 level;
	if (!p)
		return;

	_CPU_ISR_Disable(level);

	if (num_blobs == 0) {
		_CPU_ISR_Restore(level);
		gprintf("blob_free with no blobs\n");
		panic();
	}

	num_blobs--;
	if (p != blobs[num_blobs].the_blob) {
		_CPU_ISR_Restore(level);
		gprintf("mismatched blob_free (%p != %p)\n", p, blobs[num_blobs].the_blob);
		panic();
	}
	if (SYS_GetArena2Hi() != p) {
		_CPU_ISR_Restore(level);
		gprintf("someone else used MEM2 (%p != %p)\n", p, SYS_GetArena2Hi());
		panic();
	}

	SYS_SetArena2Hi(blobs[num_blobs].old_arena2hi);
	_CPU_ISR_Restore(level);
	gprintf("freed blob size %d at %p\n", blobs[num_blobs].blob_size, p);
}

