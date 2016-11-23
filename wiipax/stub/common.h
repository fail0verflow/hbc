#ifndef _LOADER_H_
#define _LOADER_H_

#include <stddef.h>

// Basic types.

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short int u16;
typedef signed short int s16;
typedef unsigned int u32;
typedef signed int s32;

// Basic I/O.

static inline u32 read32(u32 addr) {
	u32 x;

	asm volatile("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline void write32(u32 addr, u32 x) {
	asm("stw %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}

static inline u16 read16(u32 addr) {
	u16 x;

	asm volatile("lhz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline u8 read8(u32 addr) {
	u8 x;

	asm volatile("lbz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline void write16(u32 addr, u16 x) {
	asm("sth %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}

static inline void write8(u32 addr, u8 x) {
	asm("stb %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}

static inline void mask32(u32 addr, u32 clear, u32 set)
{
	write32(addr, (read32(addr)&(~clear)) | set);
}

// Address mapping.

static inline u32 virt_to_phys(const void *p) {
	return (u32)p & 0x7fffffff;
}

static inline void *phys_to_virt(u32 x) {
	return (void *)(x | 0x80000000);
}


// Cache synchronisation.

void sync_before_read(void *p, u32 len);
void sync_after_write(const void *p, u32 len);
void sync_before_exec(const void *p, u32 len);


// Time.

void udelay(u32 us);


// Special purpose registers.

#define mtspr(n, x) do { asm("mtspr %1,%0" : : "r"(x), "i"(n)); } while (0)
#define mfspr(n) ({ \
	u32 x; asm volatile("mfspr %0,%1" : "=r"(x) : "i"(n)); x; \
})


// Exceptions.

#ifdef NDEBUG
#define printf(...) { }
#else
void exception_init(void);
void gecko_init(void);
int printf(const char *fmt, ...);
#endif


// Debug: blink the tray led.

static inline void blink(void) {
	write32(0x0d8000c0, read32(0x0d8000c0) ^ 0x20);
}

#endif

