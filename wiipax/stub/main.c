#include "common.h"

#include "string.h"
#include "elf.h"

#include "LzmaDec.h"

extern const u8 __self_start[], __self_end[];
extern const u8 __payload[];

extern void _plunge(u32 entry);

typedef struct {
	u8 * const data;
	const u32 len_in;
	const u32 len_out;
	const u8 props[LZMA_PROPS_SIZE];
} __attribute__((packed)) _payload;

const _payload * const payload = (void *) __payload;

u8 * const code_buffer = (u8 *) 0x90100000;

static void *lz_malloc(void *p, size_t size) {
	(void) p;
	(void) size;
	printf("lz_malloc %u\n", size);
	return (void *) 0x90081000;
}

static void lz_free(void *p, void *address) {
	(void) p;
	(void) address;
	printf("lz_free %p\n", address);
}

static const ISzAlloc lz_alloc = { lz_malloc, lz_free };

static inline __attribute__((always_inline)) int decode(void) {
	SizeT len_in;
	SizeT len_out;
	ELzmaStatus status;
	SRes res;

	len_in = payload->len_in;
	len_out = payload->len_out;

	printf("in: %d out: %d\n", len_in, len_out);

	res = LzmaDecode(code_buffer, &len_out, payload->data, &len_in,
					payload->props, LZMA_PROPS_SIZE, LZMA_FINISH_END,
					&status, (ISzAlloc*)&lz_alloc);

	if (res != SZ_OK) {
		printf("decoding error %d (%u)\n", res, status);
		return res;
	}

	return 0;
}

void stubmain(void) {
#ifndef NDEBUG
	exception_init();
#endif

	// clear interrupt mask
	write32(0x0c003004, 0);

#ifndef NDEBUG
#ifndef DEVKITFAIL
	udelay(500 * 1000); // wait for mini - avoid EXI battle
#endif
	gecko_init();
#endif
	printf("hello world\n");
	printf("payload @%p blob @%p\n", payload, payload->data);

	if (decode() == 0 && valid_elf_image(code_buffer)) {
		printf("Valid ELF image detected.\n");
		u32 entry = load_elf_image(code_buffer);

		if (entry) {
			// Disable all IRQs; ack all pending IRQs.
			write32(0x0c003004, 0);
			write32(0x0c003000, 0xffffffff);

			printf("Branching to 0x%08x\n", entry);
#ifndef DEVKITFAIL
			// detect failkit apps packed with the mini stub
			if (entry & 0xc0000000) {
				void (*ep)() = (void (*)()) entry;
				ep();
			} else {
				_plunge(entry);
			}
#else
			void (*ep)() = (void (*)()) entry;
			ep();
#endif
		}
	}

error:
	blink();
	udelay(500000);
	goto error;
}

