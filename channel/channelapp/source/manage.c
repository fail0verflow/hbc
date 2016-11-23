#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>

#include <ogcsys.h>

#include "../config.h"
#include "appentry.h"
#include "theme.h"
#include "unzip.h"
#include "dialogs.h"
#include "i18n.h"
#include "panic.h"

#include "manage.h"

s32 dir_exists(char *dirname) {
	struct stat st;

	if (stat(dirname, &st) != 0)
		return 0;

	if (S_ISDIR(st.st_mode))
		return 1;

	gprintf("'%s' exists, but is no directory\n", dirname);

	return -1;
}

static s32 mkdir_hier(char *dirname) {
	char dir[MAXPATHLEN];
	size_t i;
	s32 res;

	strcpy(dir, dirname);

	if (dir[strlen(dir) - 1] != '/')
		strcat(dir, "/");

	i = 1;
	while (dir[i]) {
		if (dir[i] == '/') {
			dir[i] = 0;

			res = dir_exists(dir);

			if (res < 0)
				return res;

			if (!res) {
				gprintf("mkdir '%s'\n", dir);
				if (mkdir(dir, 0755)) {
					gprintf("mkdir failed: %d\n", errno);
					return -1;
				}
			}

			dir[i] = '/';
		}

		i++;
	}

	return 0;
}

static s32 rmdir_hier_iter(const char *dirname) {
	s32 res = -1;
	DIR* d;
	struct dirent *de;

	gprintf("rmdir_hier_iter '%s'\n", dirname);

	d = opendir(dirname);
	if (!d) {
		gprintf("opendir '%s' failed\n", dirname);
		return -1;
	}

	char newpath[MAXPATHLEN];

	while ((de = readdir(d))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;

		strcpy(newpath, dirname);
		if (newpath[strlen(newpath) - 1] != '/')
			strcat(newpath, "/");
		strcat(newpath, de->d_name);

		if (de->d_type == DT_DIR) {
			res = rmdir_hier_iter(newpath);
			if (res)
				goto exit;
		}

		gprintf("unlinking '%s'\n", newpath);
		res = unlink(newpath);
		if (res) {
			gprintf("error unlinking '%s'\n", newpath);
			goto exit;
		}
	}

	res = 0;

exit:
	closedir(d);

	return res;
}

static s32 rmdir_hier(const char *dirname) {
	char buf[MAXPATHLEN];

	sprintf(buf, "%s/%s", app_path, dirname);

	gprintf("rmdir_hier '%s'\n", buf);
	s32 res = rmdir_hier_iter(buf);
	if (!res) {
		gprintf("unlinking dir '%s'\n", (buf));
		res = unlink(buf);
		if (res)
			gprintf("error unlinking '%s'\n", buf);
	}

	return res;
}

bool manage_is_zip(const u8 *data) {
	return memcmp(data, "PK\x03\x04", 4) == 0;
}

bool manage_check_zip_app(u8 *data, u32 data_len, char *dirname, u32 *bytes) {
	unzFile uf;
	int res, i;
	bool ret = false;
	unz_global_info gi;

	uf = unzOpen(data, data_len);
	if (!uf) {
		gprintf("unzOpen failed\n");
		return false;
	}

	res = unzGetGlobalInfo (uf, &gi);
	if (res != UNZ_OK) {
		gprintf("unzGetGlobalInfo failed: %d\n", res);
		goto error;
	}

	if ((gi.number_entry < 2) || (gi.number_entry > 1024)) {
		gprintf("invalid file count\n");
		goto error;
	}

	char filename[256];
	unz_file_info fi;
	u8 got_elf = 0;
	u8 got_dol = 0;
	u8 got_theme = 0;

	dirname[0] = 0;
	*bytes = 0;

	for (i = 0; i < gi.number_entry; ++i) {
		res = unzGetCurrentFileInfo(uf, &fi, filename, sizeof(filename) ,NULL, 0, NULL, 0);
		if (res != UNZ_OK) {
			gprintf("unzGetCurrentFileInfo failed: %d\n", res);
			goto error;
		}

		gprintf("found '%s' %lu -> %lu\n", filename, fi.compressed_size, fi.uncompressed_size);

		if (filename[0] == '/' || strchr(filename, '\\') || strchr(filename, ':')) {
			gprintf("invalid char in filename\n");
			goto error;
		}

		if (fi.flag & 1) {
			gprintf("encrypted entry\n");
			goto error;
		}

		if (fi.uncompressed_size > 0) {
			*bytes += fi.uncompressed_size;

			if (!dirname[0]) {
				char *p = strchr(filename, '/');
				if (p) {
					strncpy(dirname, filename, p - filename + 1);
					dirname[p - filename + 1] = 0;

					gprintf("dirname='%s'\n", dirname);
				} else {
					gprintf("missing pathname\n");
					goto error;
				}
			} else {
				if (strncmp(filename, dirname, strlen(dirname))) {
					gprintf("additional pathname\n");
					goto error;
				}
			}

			if (!strcasecmp(filename + strlen(dirname), app_fn_boot_elf))
				got_elf = 1;
			else if (!strcasecmp(filename + strlen(dirname), app_fn_boot_dol))
				got_dol = 1;
			else if (!strcasecmp(filename + strlen(dirname), app_fn_theme))
				got_theme = 1;
		}

		if (i != gi.number_entry - 1) {
			res = unzGoToNextFile(uf);
			if (res != UNZ_OK) {
				gprintf("unzGoToNextFile failed: %d\n", res);
				goto error;
			}
		}
	}

	if (strlen(dirname) && ((got_elf + got_dol + got_theme) == 1)) {
		ret = true;
		dirname[strlen(dirname) - 1] = 0;
	}

error:
	res = unzClose(uf);
	if (res)
		gprintf("unzClose failed: %d\n", res);

	return ret;
}

bool manage_check_zip_theme(u8 *data, u32 data_len) {
	unzFile uf;
	int res, i;
	bool ret = false;
	unz_global_info gi;

	if (data_len > MAX_THEME_ZIP_SIZE) {
		gprintf("theme size too big: %lu\n", data_len);
		return false;
	}

	uf = unzOpen(data, data_len);
	if (!uf) {
		gprintf("unzOpen failed\n");
		return false;
	}

	res = unzGetGlobalInfo (uf, &gi);
	if (res != UNZ_OK) {
		gprintf("unzGetGlobalInfo failed: %d\n", res);
		goto error;
	}

	if ((gi.number_entry < 1) || (gi.number_entry > 64)) {
		gprintf("invalid file count\n");
		goto error;
	}

	char filename[256];
	unz_file_info fi;
	bool got_xml = false;

	for (i = 0; i < gi.number_entry; ++i) {
		res = unzGetCurrentFileInfo(uf, &fi, filename, sizeof(filename) ,NULL, 0, NULL, 0);
		if (res != UNZ_OK) {
			gprintf("unzGetCurrentFileInfo failed: %d\n", res);
			goto error;
		}

		gprintf("found '%s' %lu -> %lu\n", filename, fi.compressed_size, fi.uncompressed_size);

		if (filename[0] == '/' || strchr(filename, '\\') || strchr(filename, ':')) {
			gprintf("invalid char in filename\n");
			goto error;
		}

		if (fi.flag & 1) {
			gprintf("encrypted entry\n");
			goto error;
		}

		if (fi.uncompressed_size > 0) {
			char *p = strchr(filename, '/');
			if (p) {
				gprintf("directories not accepted\n");
				goto error;
			}

			if (!strcasecmp(filename, theme_fn_xml)) {
				got_xml = true;
			} else if (!theme_is_valid_fn(filename)) {
				gprintf("not a valid theme filename\n");
				goto error;
			}
		}

		if (i != gi.number_entry - 1) {
			res = unzGoToNextFile(uf);
			if (res != UNZ_OK) {
				gprintf("unzGoToNextFile failed: %d\n", res);
				goto error;
			}
		}
	}

	if (got_xml)
		ret = true;

error:
	res = unzClose(uf);
	if (res)
		gprintf("unzClose failed: %d\n", res);

	return ret;
}

static bool manage_extract_zip(u8 *data, u32 data_len,
								mutex_t *mutex, u32 *progress) {
	unzFile uf;
	int res, i;
	bool ret = false;
	unz_global_info gi;
	u8 *buf = NULL;

	uf = unzOpen(data, data_len);
	if (!uf) {
		gprintf("unzOpen failed\n");
		return false;
	}

	res = unzGetGlobalInfo (uf, &gi);
	if (res != UNZ_OK) {
		gprintf("unzGetGlobalInfo failed: %d\n", res);
		goto error;
	}

	unz_file_info fi;
	char filename[256];
	char sd_filename[MAXPATHLEN];
	char *p;
	int fd;
	
	buf = (u8 *) pmalloc(8 * 1024);

	for (i = 0; i < gi.number_entry; ++i) {
		res = unzGetCurrentFileInfo(uf, &fi, filename, sizeof(filename) ,NULL, 0, NULL, 0);
		if (res != UNZ_OK) {
			gprintf("unzGetCurrentFileInfo failed: %d\n", res);
			goto error;
		}

		gprintf("extracting '%s' %lu -> %lu\n", filename, fi.compressed_size, fi.uncompressed_size);

		if (fi.uncompressed_size > 0) {
			res = unzOpenCurrentFile(uf);
			if (res != UNZ_OK) {
				gprintf("unzOpenCurrentFile failed: %d\n", res);
				goto error;
			}

			sprintf(sd_filename, "%s/%s", app_path, filename);

			p = strrchr(sd_filename, '/');
			if (!p) {
				gprintf("invalid path: %s\n", sd_filename);
				goto error;
			}

			*p = 0;
			res = mkdir_hier(sd_filename);
			if (res) {
				gprintf("mkdir_hier failed: %d\n", res);
				goto error;
			}
			*p = '/';

			fd = open(sd_filename, O_CREAT|O_WRONLY|O_TRUNC);
			if (fd < 0) {
				gprintf("error opening file: %d\n", fd);
				goto error;
			}

			do {
				res = unzReadCurrentFile(uf, buf, 8 * 1024);

				if (res < 0) {
					gprintf("unzReadCurrentFile failed: %d\n", res);
					break;
				}

				if (res > 0) {
					if (res != write(fd, buf, res)) {
						gprintf("short write");
						res = -1;
						break;
					}

					LWP_MutexLock (*mutex);
					*progress += res;
					LWP_MutexUnlock (*mutex);
				}
			} while (res > 0);

			close(fd);
			unzCloseCurrentFile(uf);

			if (res < 0) {
				gprintf("error extracting file: %d\n", res);
				goto error;
			}
		}

		if (i != gi.number_entry - 1) {
			res = unzGoToNextFile(uf);
			if (res != UNZ_OK) {
				gprintf("unzGoToNextFile failed: %d\n", res);
				goto error;
			}
		}
	}

	ret = true;

error:
	free(buf);

	res = unzClose(uf);
	if (res)
		gprintf("unzClose failed: %d\n", res);

	return ret;
}

static lwpq_t manage_queue;
static lwp_t manage_thread;
static u8 manage_stack[MANAGE_THREAD_STACKSIZE] ATTRIBUTE_ALIGN (32);

typedef struct {
	bool running;

	bool extract;

	// extract
	u8 *data;
	u32 data_len;
	mutex_t mutex;
	u32 progress;

	// delete
	const char *dirname;

	bool success;
} manage_zip_arg;

static void *manage_func (void *arg) {
	manage_zip_arg *ta = (manage_zip_arg *) arg;

	if (ta->extract)
		ta->success = manage_extract_zip(ta->data, ta->data_len, &ta->mutex, &ta->progress);
	else
		ta->success = 0 == rmdir_hier(ta->dirname); 

	ta->running = false;

	return NULL;
}

bool manage_run(view *sub_view, const char *dirname,
				u8 *data, u32 data_len, u32 bytes) {
	s32 res;
	u32 progress = 0;
	char caption[MAXPATHLEN];

	view *v;

	manage_zip_arg ta;

	res = LWP_MutexInit(&ta.mutex, false);
	if (res) {
		gprintf ("error creating mutex: %ld\n", res);
		return false;
	}

	ta.running = true;
	ta.data = data;
	ta.data_len = data_len;
	ta.progress = 0;
	ta.dirname = dirname;

	ta.extract = data != NULL;

	memset(&manage_stack, 0, MANAGE_THREAD_STACKSIZE);
	LWP_InitQueue(&manage_queue);
	res = LWP_CreateThread(&manage_thread, manage_func, &ta, manage_stack,
							MANAGE_THREAD_STACKSIZE, MANAGE_THREAD_PRIO);

	if (res) {
		gprintf ("error creating thread: %ld\n", res);
		LWP_CloseQueue(manage_queue);
		return false;
	}

	const char *text;
	
	if (data) {
		text = _("Extracting");
		sprintf(caption, "%s '%s'...", text, dirname);
		v = dialog_progress(sub_view, caption, bytes);
		dialog_fade(v, true);
	} else {
		v = sub_view;
		view_show_throbber(true);
	}

	while (true) {
		LWP_MutexLock(ta.mutex);
		if (!ta.running) {
			LWP_MutexUnlock(ta.mutex);
			break;
		}

		progress = ta.progress;

		LWP_MutexUnlock (ta.mutex);

		if (data)
			dialog_set_progress(v, progress);
		else
			view_throbber_tickle();

		view_plot(v, DIALOG_MASK_COLOR, NULL, NULL, NULL);
	}

	if (data) {
		dialog_fade(v, false);
		view_free(v);
	} else {
		view_show_throbber(false);
	}

	//LWP_SuspendThread(manage_thread);
	LWP_CloseQueue(manage_queue);

	LWP_MutexDestroy(ta.mutex);

	return ta.success;
}


