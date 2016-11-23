#include <string.h>
#include <ogc/machine/processor.h>

#include "panic.h"

// steal from libogc
extern unsigned char console_font_8x16[];

#define BLACK 0x00800080
#define RED 0x4C544CFF

static vu32 *fb;

static void pchar(u32 cx, u32 cy, char c)
{
	int x,y;
	unsigned char *p = &console_font_8x16[16*c];
	
	for (y=0; y<16; y++) {
		char v = *p++;
		for (x=0; x<4; x++) {
			u32 c = RED;
			switch (v&0xc0) {
			case 0x00:
				c = BLACK;
				break;
			case 0x40:
				c = (RED & 0x0000FF00) | (BLACK & 0xFFFF00FF);
				break;
			case 0x80:
				c = (RED & 0xFFFF00FF) | (BLACK & 0x0000FF00);
				break;
			case 0xc0:
				c = RED;
				break;
			}
			fb[320*(cy+y)+cx+x] = c;
			v<<=2;
		}
	}
}

static void putsc(u32 y, char *s) {
	u32 x = (320-strlen(s)*4)/2;
	while(*s) {
		pchar(x, y, *s++);
		x += 4;
	}
}

static void hex(u32 v, char *p) {
	int i;
	for (i=0; i<8; i++) {
		if ((v>>28) >= 10)
			*p++ = 'A' + (v>>28) - 10;
		else
			*p++ = '0' + (v>>28);
		v <<= 4;
	}
}

#define		HW_REG_BASE		0xd800000
#define		HW_RESETS		(HW_REG_BASE + 0x194)

// written to be as generic and reliable as possible
void _panic(u32 file, u32 line)
{
	u32 level;
	u32 fbr;
	int count = 30;
	int x,y;
	u32 bcolor = BLACK;
	u16 vtr;
	int lines;
	char guru[] = "Guru Meditation #00000000.00000000";
	_CPU_ISR_Disable(level);

	fbr = read32(0xc00201c) & 0x1fffffff;
	if (fbr&0x10000000)
		fbr <<= 5;

	fb = (vu32*)(fbr | 0xC0000000);

	vtr = read16(0xc002000);
	if ((vtr & 0xf) > 10)
		lines = vtr >> 4;
	else
		lines = 2 * (vtr >> 4);

	for(y=(lines-1); y>=116; y--)
		for(x=0; x<320; x++)
			fb[y*320+x] = fb[(y-116)*320+x];
	for(y=0; y<116; y++)
		for(x=0; x<320; x++)
			fb[y*320+x] = BLACK;

	hex(file, &guru[17]);
	hex(line, &guru[26]);
		
	putsc(42, "Software Failure.   Press reset button to continue.");
	putsc(62, guru);
		
	// wait for reset button
	while(read32(0xcc003000)&0x10000) {
		// blink
		if (count >= 25) {
			if (bcolor == RED)
				bcolor = BLACK;
			else
				bcolor = RED;

			for(y=28; y<34; y++)
				for(x=14; x<306; x++)
					fb[y*320+x] = bcolor;
			for(y=34; y<84; y++) {
				for(x=14; x<17; x++)
					fb[y*320+x] = bcolor;
				for(x=303; x<306; x++)
					fb[y*320+x] = bcolor;
			}
			for(y=84; y<90; y++)
				for(x=14; x<306; x++)
					fb[y*320+x] = bcolor;

			count = 0;
		}
		
		// hacky waitvsync
		while(read16(0xc00202c) >= 200);
		while(read16(0xc00202c) <  200);
		count++;
	}

	// needs AHBPROT, is evil. so sue me.
	write32(HW_RESETS, read32(HW_RESETS)&(~1));
	while(1);
}
