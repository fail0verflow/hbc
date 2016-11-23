#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"
#include "lzma.h"

#include "LzmaDec.h"

static void *lz_malloc(void *p, size_t size) {
	(void) p;
	return malloc(size);
}

static void lz_free(void *p, void *address) {
	(void) p;
	free(address);
}

static ISzAlloc lz_alloc = { lz_malloc, lz_free };

static lzma_t *lzma_alloc(const u32 len) {
	lzma_t *lzma;

	lzma = (lzma_t *) calloc(1, sizeof(lzma_t));

	if (!lzma)
		die("Failed to alloc 0x%x bytes", (u32) sizeof(lzma_t));

	lzma->data = calloc(1, len);
	if (!lzma->data)
		die("Failed to alloc 0x%x bytes", len);

	return lzma;
}

void lzma_free(lzma_t *lzma) {
	free(lzma->data);
	free(lzma);
}

lzma_t *lzma_compress(const u8 *src, const u32 len) {
	lzma_t *lzma;
	CLzmaEncProps props;
	size_t len_out;
	size_t len_props = LZMA_PROPS_SIZE;
	SRes res;

	printf("Compressing...");
	fflush(stdout);

	lzma = lzma_alloc(2*len);

	LzmaEncProps_Init(&props);
	props.level = 7;
	LzmaEncProps_Normalize(&props);

	len_out = 2*len;
	res = LzmaEncode(lzma->data, &len_out, src, len, &props, lzma->props,
					&len_props, 1, NULL, &lz_alloc, &lz_alloc);

	if (res != SZ_OK)
		die(" failed (%d)", res);

	if (len_props != LZMA_PROPS_SIZE)
		die(" failed: encoder propsize %u != %u", (u32) len_props,
			LZMA_PROPS_SIZE);

	printf(" %u -> %u = %3.2f%%\n", len, (u32) len_out,
			100.0 * (float) len_out / (float) len);

	lzma->len_in = len;
	lzma->len_out = len_out;

	return lzma;
}

void lzma_decode(const lzma_t *lzma, u8 *dst) {
	SizeT len_in;
	SizeT len_out;
	ELzmaStatus status;
	SRes res;

	len_in = lzma->len_out;
	len_out = lzma->len_in;

	res = LzmaDecode(dst, &len_out, lzma->data, &len_in, lzma->props,
					LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &lz_alloc);

	if (res != SZ_OK)
		die("Error decoding %d (%u)\n", res, status);

	*dst = len_out;
}

void lzma_write(const char *filename, const lzma_t *lzma) {
	int fd;
	int i;

	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd < 0)
		perrordie("Could not open file");

	if (write(fd, lzma->props, LZMA_PROPS_SIZE) != LZMA_PROPS_SIZE)
		perrordie("Could not write to file");

	u64 size = lzma->len_in;
	for (i = 0; i < 8; ++i) {
		u8 j = size & 0xff;
		size >>= 8;
		if (write(fd, &j, 1) != 1)
			perrordie("Could not write to file");
	}

	if (write(fd, lzma->data, lzma->len_out) != lzma->len_out)
		perrordie("Could not write to file");

	close(fd);
}

