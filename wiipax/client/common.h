#ifndef _COMMON_H_
#define _COMMON_H_

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;

#define round_up(x,n) (-(-(x) & -(n)))

#define die(...) { \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); exit(1); \
}

#define perrordie(x) { perror(x); exit(1); }

#if BYTE_ORDER == BIG_ENDIAN

#define be32(x) (x)
#define be16(x) (x)

#else

static inline u32 be32(const u32 v) {
	return (v >> 24) |
		((v >> 8)  & 0x0000FF00) |
		((v << 8)  & 0x00FF0000) |
		(v << 24);
}

static inline u16 be16(const u16 v) {
	return (v >> 8) | (v << 8);
}

#endif /* BIG_ENDIAN */

#endif /* _COMMON_H_ */

