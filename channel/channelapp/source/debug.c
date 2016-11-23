#include <stdio.h>
#include <stdarg.h>

#include <ogcsys.h>
#include <ogc/machine/processor.h>

#include "../config.h"

#ifdef DEBUG_APP

//#define DEBUG_MEMSTATS

static int gprintf_enabled = 1;

void gprintf_enable(int enable) {
	gprintf_enabled = enable;
}

int gprintf(const char *format, ...)
{
	va_list ap;
	u32 level;
	int ret;

	if (!gprintf_enabled)
		return 0;

	level = IRQ_Disable();
	va_start(ap, format);
	ret = vprintf(format, ap);
	va_end(ap);
	IRQ_Restore(level);

	return ret;
}

/********* you know you love it **********/
static char ascii(char s) {
  if(s < 0x20) return '.';
  if(s > 0x7E) return '.';
  return s;
}

void hexdump(const void *d, int len) {
  u8 *data;
  int i, off;
  data = (u8*)d;
  for (off=0; off<len; off += 16) {
    gprintf("%08x  ",off);
    for(i=0; i<16; i++)
      if((i+off)>=len) gprintf("   ");
      else gprintf("%02x ",data[off+i]);

    gprintf(" ");
    for(i=0; i<16; i++)
      if((i+off)>=len) gprintf(" ");
      else gprintf("%c",ascii(data[off+i]));
    gprintf("\n");
  }
}
/********* you know you love it **********/

#ifndef UINT_MAX
#define UINT_MAX ((u32)((s32)-1))
#endif
void memstats(int reset) {
#ifdef DEBUG_MEMSTATS
	static u32 min_free = UINT_MAX;
	static u32 temp_free;
	static u32 level;

	if (reset)
		min_free = UINT_MAX;

	_CPU_ISR_Disable(level);

	temp_free = (u32) SYS_GetArena2Hi() - (u32) SYS_GetArena2Lo();

	_CPU_ISR_Restore(level);

	if (temp_free < min_free) {
		min_free = temp_free;
		gprintf("MEM2 free: %8u\n", min_free);
	}
#endif
}

#endif
