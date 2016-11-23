// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ERROR(s) do { fprintf(stderr, s "\n"); exit(1); } while (0)

// basic data types
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

void wbe16(u8 *p, u16 x) {
	p[0] = x >> 8;
	p[1] = x;
}

void wbe32(u8 *p, u32 x) {
	wbe16(p, x >> 16);
	wbe16(p + 2, x);
}

static int read_image(u8 *data, u32 w, u32 h, const char *name) {
	FILE *fp;
	u32 x, y;
	u32 ww, hh;

	fp = fopen(name, "rb");
	if (!fp)
		return -1;

	if (fscanf(fp, "P6 %d %d 255\n", &ww, &hh) != 2)
		ERROR("bad ppm");
	if (ww != w || hh != h)
		ERROR("wrong size ppm");

	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			u8 pix[3];
			u16 raw;
			u32 x0, x1, y0, y1, off;

			x0 = x & 3;
			x1 = x >> 2;
			y0 = y & 3;
			y1 = y >> 2;
			off = x0 + 4 * y0 + 16 * x1 + 4 * w * y1;

			if (fread(pix, 3, 1, fp) != 1)
				ERROR("read");

			raw = (pix[0] & 0xf8) << 7;
			raw |= (pix[1] & 0xf8) << 2;
			raw |= (pix[2] & 0xf8) >> 3;
			raw |= 0x8000;

			wbe16(data + 2*off, raw);
		}

	fclose(fp);
	return 0;
}

int main(int argc, char **argv) {
	FILE *in, *fp;
	u8 header[0x72a0];

	fp = fopen("banner.bin", "wb+");
	if (!fp)
		ERROR("open banner.bin");

	memset(header, 0, sizeof header);

	memcpy(header, "WIBN", 4);
	header[9] = 2; // wtf

	in = fopen("title", "rb");
	if (!in)
		ERROR("open title");
	if (fread(header + 0x20, 0x80, 1, in) != 1)
		ERROR("read title");
	fclose(in);

	if(read_image(header + 0xa0, 192, 64, "banner.ppm"))
		ERROR("open banner.ppm"); 

	if(read_image(header + 0x60a0, 48, 48, "icon.ppm"))
		ERROR("open icon.ppm"); 

	if (fwrite(header, 0x72a0, 1, fp) != 1)
		ERROR("write header");

	fclose(fp);

	return 0;
}
