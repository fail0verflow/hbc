#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <gccore.h>

#include "../config.h"
#include "blob.h"
#include "panic.h"
#include "isfs.h"

#define ROUNDUP32B(x) ((x + 32 - 1) & ~(32 - 1))

static fstats __st ATTRIBUTE_ALIGN(32);

s32 isfs_get(const char *path, u8 **buf, u32 expected, u32 maxsize, bool use_blob) {
	s32 ret;
	s32 fd;
	u8 *__buf;
	
	gprintf("reading %s (expecting %u bytes)\n", path, expected);
	ret = ISFS_Open(path, ISFS_OPEN_READ);
	if (ret < 0) {
		gprintf("ISFS_Open failed (%d)\n", ret);
		return ret;
	}

	fd = ret;

	ret = ISFS_GetFileStats(fd, &__st);
	if (ret < 0) {
		gprintf("ISFS_GetFileStats failed (%d)\n", ret);
		return ret;
	}
	DCInvalidateRange(&__st, sizeof(__st));

	if ((__st.file_length == 0) ||
			(maxsize && (__st.file_length > maxsize)) ||
			(expected && (__st.file_length != expected))) {
		gprintf("invalid size: %u\n", __st.file_length);
		ISFS_Close(fd);
		return -1;
	}

	if (use_blob) {
		__buf = blob_alloc(ROUNDUP32B(__st.file_length + 1));
		if (!__buf)
			panic();
	} else {
		__buf = pmemalign(32, ROUNDUP32B(__st.file_length + 1));
	}

	gprintf("reading %u bytes\n", __st.file_length);
	ret = ISFS_Read(fd, __buf, __st.file_length);
	if (ret < 0) {
		gprintf("ISFS_Read failed (%d)\n", ret);
		free(__buf);
		ISFS_Close(fd);
		return ret;
	}
	DCInvalidateRange(__buf, __st.file_length + 1);
	__buf[__st.file_length] = 0;

	if (ret != __st.file_length) {
		gprintf("ISFS_Read short read (%d)\n", ret);
		free(__buf);
		ISFS_Close(fd);
		return -1;
	}

	ret = ISFS_Close(fd);
	if (ret < 0) {
		gprintf("ISFS_Close failed (%d)\n", ret);
		free(__buf);
		return ret;
	}

	*buf = __buf;
	return __st.file_length;
}

s32 isfs_put(const char *path, const void *buf, u32 len) {
	s32 fd, ret;
	
	ISFS_Delete(path);

	gprintf("writing %s (%u bytes)\n", path, len);
	ret = ISFS_CreateFile(path, 0, 3, 3, 0);
	if (ret < 0) {
		gprintf("ISFS_CreateFile failed (%d)\n", ret);
		return ret;
	}

	ret = ISFS_Open(path, 3);
	if (ret < 0) {
		gprintf("ISFS_Open failed (%d)\n", ret);
		ISFS_Delete(path);
		return ret;
	}

	fd = ret;

	DCFlushRange((void *) buf, len);

	ret = ISFS_Write(fd, buf, len);
	if (ret < 0) {
		gprintf("ISFS_Write failed (%d)\n", ret);
		ISFS_Close(fd);
		ISFS_Delete(path);
		return ret;
	}

	if (ret != len) {
		gprintf("ISFS_Write short write (%d)\n", ret);
		ISFS_Close(fd);
		ISFS_Delete(path);
		return -1;
	}

	ret = ISFS_Close(fd);
	if (ret < 0) {
		gprintf("ISFS_Close failed (%d)\n", ret);
		ISFS_Delete(path);
		return ret;
	}

	return 0;
}

