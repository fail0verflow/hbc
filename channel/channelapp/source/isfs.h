#ifndef ___ISFS_H__
#define ___ISFS_H__

#include <gctypes.h>

s32 isfs_get(const char *path, u8 **buf, u32 expected, u32 maxsize, bool use_blob);
s32 isfs_put(const char *path, const void *buf, u32 len);

#endif
