#ifndef _LZMA_H_
#define _LZMA_H_

#include "common.h"
#include "LzmaEnc.h"

typedef struct {
	u32 len_in;
	u32 len_out;
	u8 props[LZMA_PROPS_SIZE];
	u8 *data;
} lzma_t;

lzma_t *lzma_compress(const u8 *src, const u32 len);
void lzma_decode(const lzma_t *lzma, u8 *dst);
void lzma_write(const char *filename, const lzma_t *lzma);
void lzma_free(lzma_t *lzma);

#endif /* _LZMA_H_ */

