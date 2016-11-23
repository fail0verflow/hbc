#ifndef _PANIC_H_
#define _PANIC_H_

#include <string.h>
#include <ogcsys.h>
#include <malloc.h>

// to skip 'source/'
#define FOFF 7

#define S2I(s) (((s)[FOFF+0]<<24)|((s)[FOFF+1]<<16)|((s)[FOFF+2]<<8)|((s)[FOFF+3]))

#define panic() _panic(S2I(__FILE__)^0xDEADBEEF, __LINE__)
void _panic(u32 file, u32 line) __attribute__((noreturn));

#define pmalloc(s) _pmalloc(s, S2I(__FILE__)^0xDEADBEEF, __LINE__)
static inline void *_pmalloc(size_t s, u32 file, u32 line)
{
	void *p = malloc(s);
	if (!p)
		_panic(file, line);
	return p;
}

#define pmemalign(a,s) _pmemalign(a, s, S2I(__FILE__)^0xDEADBEEF, __LINE__)
static inline void *_pmemalign(size_t a, size_t s, u32 file, u32 line)
{
	void *p = memalign(a, s);
	if (!p)
		_panic(file, line);
	return p;
}

#define prealloc(p,s) _prealloc(p, s, S2I(__FILE__)^0xDEADBEEF, __LINE__)
static inline void *_prealloc(void *p, size_t s, u32 file, u32 line)
{
	p = realloc(p, s);
	if (!p)
		_panic(file, line);
	return p;
}

#define pstrdup(s) _pstrdup(s, S2I(__FILE__)^0xDEADBEEF, __LINE__)
static inline void *_pstrdup(const char *s, u32 file, u32 line)
{
	char *p;
	p = strdup(s);
	if (!p)
		_panic(file, line);
	return p;
}

#endif
