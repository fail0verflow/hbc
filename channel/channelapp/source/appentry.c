#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/dir.h>

#include <ogcsys.h>
#include <ogc/cond.h>
#include <sdcard/gcsd.h>
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include <fat.h>

#include "../config.h"
#include "tex.h"
#include "panic.h"

#include "appentry.h"

typedef struct {
	const char *name;
	const DISC_INTERFACE *device;
	bool last_status;
} device_t;

static app_entry *entries_all[MAX_ENTRIES];
static u32 entry_count_all = 0;

app_entry *entries[MAX_ENTRIES];
u32 entry_count = 0;

const char *app_path = "/apps";
const char *app_fn_boot_elf = "boot.elf";
const char *app_fn_boot_dol = "boot.dol";
const char *app_fn_theme = "theme.zip";
const char *app_fn_meta = "meta.xml";
const char *app_fn_icon = "icon.png";

static device_t devices[DEVICE_COUNT] = {
	{ "sd", &__io_wiisd, false },
	{ "usb", &__io_usbstorage, false },
	{ "carda", &__io_gcsda, false },
	{ "cardb", &__io_gcsdb, false }
};

static int device_active = -1;
static int device_prefered = -1;
static app_filter current_filter = APP_FILTER_ALL;
static app_sort current_sort = APP_SORT_NAME;
static bool cmp_descending = false;
static bool cmp_release_date = false;

static int cmp_app_entry (const void *p1, const void *p2) {
	const app_entry *a1;
	const app_entry *a2;

	if (cmp_descending) {
		a1 = *((const app_entry **) p2);
		a2 = *((const app_entry **) p1);
	} else {
		a1 = *((const app_entry **) p1);
		a2 = *((const app_entry **) p2);
	}

	if (!a1->meta && !a2->meta)
		return strcasecmp (a1->dirname, a2->dirname);

	if (!a1->meta && a2->meta)
		return 1;

	if (a1->meta && !a2->meta)
		return -1;

	if (cmp_release_date) {
		if (!a1->meta->release_date && !a2->meta->release_date)
			return 0;

		if (!a1->meta->release_date && a2->meta->release_date)
			return -1;

		if (a1->meta->release_date && !a2->meta->release_date)
			return 1;

		if (a1->meta->release_date > a2->meta->release_date)
			return 1;

		return -1;
	}

	if (!a1->meta->name && !a2->meta->name)
		return strcasecmp (a1->dirname, a2->dirname);

	if (!a1->meta->name && a2->meta->name)
		return 1;

	if (a1->meta->name && !a2->meta->name)
		return -1;

	return strcasecmp (a1->meta->name, a2->meta->name);
}

static void app_entry_filter(app_filter filter) {
	u32 i;

	for (i = 0; i < MAX_ENTRIES; ++i)
		entries[i] = NULL;

	entry_count = 0;

	for (i = 0; i < MAX_ENTRIES; ++i) {
		if (!entries_all[i])
			continue;

		switch (filter) {
		case APP_FILTER_ICONSONLY:
			if (!entries_all[i]->icon)
				continue;
			break;

		case APP_FILTER_DATEONLY:
			if (!entries_all[i]->meta) //|| !entries_all[i]->meta->release_date)
				continue;
			break;

		default:
			break;
		}

		entries[entry_count] = entries_all[i];
		entry_count++;
	}

	current_filter = filter;

	if (entry_count)
		qsort(entries, entry_count, sizeof(app_entry *), cmp_app_entry);
}

app_sort app_entry_get_sort(void) {
	return current_sort;
}

void app_entry_set_sort(app_sort sort) {
	switch (sort) {
	case APP_SORT_DATE:
		cmp_descending = true;
		cmp_release_date = true;
		current_filter = APP_FILTER_DATEONLY;
		current_sort = APP_SORT_DATE;
		break;

	default:
		cmp_descending = false;
		cmp_release_date = false;
		current_filter = APP_FILTER_ALL;
		current_sort = APP_SORT_NAME;
		break;
	}

	if (settings.sort_order != current_sort)
		settings.sort_order = current_sort;
}

static app_entry *app_entry_load_single (const char *dirname) {
	app_entry *entry;
	app_entry_type type;
	char tmp[PATH_MAX + 32];
	struct stat st;

	type = AET_BOOT_ELF;
	sprintf(tmp, "%s/%s/%s", app_path, dirname, app_fn_boot_elf);

	if (stat(tmp, &st)) {
		type = AET_BOOT_DOL;
		sprintf(tmp, "%s/%s/%s", app_path, dirname, app_fn_boot_dol);

		if (stat(tmp, &st)) {
			type = AET_THEME;
			sprintf(tmp, "%s/%s/%s", app_path, dirname, app_fn_theme);

			if (stat(tmp, &st))
				return NULL;

			if (st.st_size > MAX_THEME_ZIP_SIZE)
				return NULL;
		}
	}

	if (!st.st_size || st.st_size > LD_MAX_SIZE)
		return NULL;

	entry = (app_entry *) pmalloc(sizeof(app_entry));

	entry->type = type;
	entry->size = st.st_size;
	entry->dirname = pstrdup(dirname);
	entry->icon = NULL;
	entry->meta = NULL;

	sprintf(tmp, "%s/%s/%s", app_path, dirname, app_fn_meta);
	if (stat(tmp, &st) == 0)
		entry->meta = meta_parse(tmp);

	sprintf(tmp, "%s/%s/%s", app_path, dirname, app_fn_icon);
	if (stat(tmp, &st) == 0)
		entry->icon = tex_from_png_file(tmp, &st, APP_ENTRY_ICON_WIDTH,
										APP_ENTRY_ICON_HEIGHT);

	return entry;
}

void app_entries_free(void) {
	u32 i;

	for (i = 0; i < MAX_ENTRIES; ++i) {
		entries[i] = NULL;

		if (!entries_all[i])
			continue;

		free (entries_all[i]->dirname);
		tex_free (entries_all[i]->icon);
		meta_free (entries_all[i]->meta);

		free(entries_all[i]);
		entries_all[i] = NULL;
	}

	entry_count_all = 0;
	entry_count = 0;
}

static void app_entry_load_all (void) {
	app_entry *entry;
	DIR *d;
	struct dirent *de;

	app_entries_free ();

	d = opendir(app_path);
	if (!d)
		return;

	while ((de = readdir(d))) {
		// ignore dotfiles / dotdirs
		if (de->d_name[0] == '\0' || de->d_name[0] == '.')
			continue;

		// All apps have their own dir
		if (de->d_type != DT_DIR)
			continue;

		entry = app_entry_load_single(de->d_name);

		if (!entry)
			continue;

		entries_all[entry_count_all] = entry;

		entry_count_all++;

		if (entry_count_all >= MAX_ENTRIES)
			break;
	}

	closedir(d);

	app_entry_filter(current_filter);
}

bool app_entry_remove(app_entry *app) {
	u32 i;
	bool res = false;

	for (i = 0; i < MAX_ENTRIES; ++i)
		if (entries_all[i] == app) {
			free(entries_all[i]->dirname);
			tex_free(entries_all[i]->icon);
			meta_free(entries_all[i]->meta);

			free(entries_all[i]);
			entries_all[i] = NULL;
			entry_count_all--;

			res = true;

			break;
		}

	if (res)
		app_entry_filter(current_filter);

	return res;
}

app_entry *app_entry_add(const char *dirname) {
	app_entry *entry = NULL;
	u32 i;
	bool filter = false;

	gprintf("adding entry '%s'\n", dirname);

	for (i = 0; i < MAX_ENTRIES; ++i) {
		if (entries_all[i]) {
			if (!strcasecmp(entries_all[i]->dirname, dirname)) {
				gprintf("removing old entry '%s'\n", entries_all[i]->dirname);

				free(entries_all[i]->dirname);
				tex_free(entries_all[i]->icon);
				meta_free(entries_all[i]->meta);

				free(entries_all[i]);
				entries_all[i] = NULL;
				entry_count_all--;

				filter = true;

				break;
			}
		}
	}

	if (entry_count_all >= MAX_ENTRIES)
		goto exit;

	entry = app_entry_load_single(dirname);

	if (!entry)
		goto exit;

	for (i = 0; i < MAX_ENTRIES; ++i)
		if (!entries_all[i]) {
			entries_all[i] = entry;
			entry_count_all++;

			filter = true;

			break;
		}

exit:
	if (filter)
		app_entry_filter(current_filter);

	return entry;
}

static bool _mount(int index) {
	devices[index].last_status = fatMountSimple(devices[index].name,
											devices[index].device);

	if (!devices[index].last_status) {
		devices[index].device->shutdown();

		return false;
	}

	return true;
}

typedef enum {
	AE_CMD_IDLE = 0,
	AE_CMD_EXIT,
	AE_CMD_SCAN,
	AE_CMD_POLLSTATUS
} ae_cmd;

typedef struct {
	bool running;
	ae_cmd cmd;
	ae_action action;
	bool status[DEVICE_COUNT];
	bool umount;
	int mount;
	bool loading;
	mutex_t cmutex;
	cond_t cond;
} ae_args;

static lwp_t ae_thread;
static u8 ae_stack[APPENTRY_THREAD_STACKSIZE] ATTRIBUTE_ALIGN (32);
static ae_args ta_ae;

static void *ae_func (void *arg) {
	ae_args *ta = (ae_args *) arg;
	int i;
	char cwd[16];

	ta->running = true;

	for (i = 0; i < DEVICE_COUNT; ++i)
		ta_ae.status[i] = false;

	LWP_MutexLock (ta->cmutex);

	while (true) {

		ta->cmd = AE_CMD_IDLE;
		LWP_CondWait(ta->cond, ta->cmutex);

		if (ta->cmd == AE_CMD_EXIT)
			break;

		switch (ta->cmd) {
		case AE_CMD_SCAN:
			
			if (device_active >= 0) {
				if (!ta->umount && devices[device_active].device->isInserted())
					continue;

				gprintf("device lost: %s\n", devices[device_active].name);

				ta->umount = false;

				fatUnmount(devices[device_active].name);
				devices[device_active].device->shutdown();
				devices[device_active].last_status = false;
				device_active = -1;

				ta->action = AE_ACT_REMOVE;

				continue;
			}

			if (ta->mount >= 0) {
				if (_mount(ta->mount))
					device_active = ta->mount;
				ta->mount = -1;
			}

			if ((device_active < 0) && (device_prefered >= 0)) {
				if (_mount(device_prefered))
					device_active = device_prefered;
			}

			if (device_active < 0) {
				for (i = 0; i < DEVICE_COUNT; ++i)
					if (_mount(i)) {
						device_active = i;
						break;
					}
			}

			if (device_active >= 0) {
				gprintf("device mounted: %s\n", devices[device_active].name);

				ta->loading = true;

				strcpy(cwd, devices[device_active].name);
				strcat(cwd, ":/");

				gprintf("chdir to '%s'\n", cwd);
				if (chdir(cwd))
					gprintf("chdir failed: %d\n", errno);

				app_entry_load_all();

				ta->loading = false;

				ta->action = AE_ACT_ADD;
			}

			continue;

		case AE_CMD_POLLSTATUS:
			for (i = 0; i < DEVICE_COUNT; ++i) {
				if (i == device_active) {
					ta->status[i] = devices[i].last_status =
						devices[i].device->isInserted();

					if (!ta->umount && !devices[i].last_status)
						ta->umount = true;

					continue;
				}

				if (devices[i].last_status) {
					if (!devices[i].device->isInserted()) {
						ta->status[i] = devices[i].last_status = false;
						devices[i].device->shutdown();
					}

					continue;
				}

				if (!devices[i].device->startup()) {
					ta->status[i] = devices[i].last_status = false;
					continue;
				}

				ta->status[i] = devices[i].last_status =
					devices[i].device->isInserted();

				if (!devices[i].last_status)
					devices[i].device->shutdown();
			}

			continue;

		default:
			break;
		}
	}

	LWP_MutexUnlock (ta->cmutex);

	gprintf("app entry thread done\n");

	ta->running = false;

	return NULL;
}

void app_entry_init(void) {
	s32 res;

	memset(&entries_all, 0, sizeof(app_entry *) * MAX_ENTRIES);
	memset(&entries, 0, sizeof(app_entry *) * MAX_ENTRIES);

	gprintf("starting app entry thread\n");

	ta_ae.cmd = AE_CMD_IDLE;
	ta_ae.action = AE_ACT_NONE;
	ta_ae.umount = false;
	ta_ae.mount = -1;
	ta_ae.loading = false;

	res = LWP_MutexInit (&ta_ae.cmutex, false);

	if (res) {
		gprintf ("error creating cmutex: %d\n", res);
		return;
	}

	res = LWP_CondInit (&ta_ae.cond);

	if (res) {
		gprintf ("error creating cond: %d\n", res);
		return;
	}

	memset(&ae_stack, 0, APPENTRY_THREAD_STACKSIZE);
	res = LWP_CreateThread(&ae_thread, ae_func, &ta_ae, ae_stack,
							APPENTRY_THREAD_STACKSIZE, APPENTRY_THREAD_PRIO);

	if (res) {
		gprintf("error creating thread: %d\n", res);
	}
}

void app_entry_deinit (void) {
	u8 i;

	if (ta_ae.running) {
		gprintf ("stopping app entry thread\n");

		for (i = 0; i < 25; ++i) {
			if (LWP_MutexTryLock (ta_ae.cmutex) == 0) {
				break;
			}
			usleep (20 * 1000);
		}
		if (i < 25) {
			gprintf ("sending app entry thread the exit cmd\n");
			ta_ae.cmd = AE_CMD_EXIT;
			LWP_SetThreadPriority (ae_thread, LWP_PRIO_HIGHEST);
			LWP_CondBroadcast (ta_ae.cond);
			LWP_MutexUnlock (ta_ae.cmutex);
			LWP_JoinThread(ae_thread, NULL);
			LWP_CondDestroy (ta_ae.cond);
			LWP_MutexDestroy (ta_ae.cmutex);
			app_entries_free ();
		} else {
			gprintf("app entry thread didn't shutdown gracefully!\n");
		}

		if (device_active >= 0) {
			fatUnmount(devices[device_active].name);
			devices[device_active].device->shutdown();
		}

		USB_Deinitialize();
	}

}

void app_entry_scan(void) {
	if (!ta_ae.running)
		return;

	if (LWP_MutexTryLock(ta_ae.cmutex) != 0)
		return;

	ta_ae.cmd = AE_CMD_SCAN;
	LWP_CondBroadcast(ta_ae.cond);
	LWP_MutexUnlock(ta_ae.cmutex);
}

ae_action app_entry_action(void) {
	ae_action res;

	if (!ta_ae.running)
		return AE_ACT_NONE;

	if (LWP_MutexTryLock(ta_ae.cmutex) != 0)
		return AE_ACT_NONE;

	res = ta_ae.action;
	ta_ae.action = AE_ACT_NONE;
	LWP_MutexUnlock(ta_ae.cmutex);

	return res;
}

void app_entry_poll_status(bool reset) {
	if (!ta_ae.running)
		return;

	if (LWP_MutexTryLock(ta_ae.cmutex) != 0)
		return;

	ta_ae.cmd = AE_CMD_POLLSTATUS;
	LWP_CondBroadcast(ta_ae.cond);
	LWP_MutexUnlock(ta_ae.cmutex);
}

int app_entry_get_status(bool *status) {
	u8 i;

	if (status)
		for (i = 0; i < DEVICE_COUNT; ++i)
			status[i] = ta_ae.status[i];

	return device_active;
}

void app_entry_set_prefered(int device) {
	if (device < 0) {
		device_prefered = -1;
		return;
	}

	if (device > DEVICE_COUNT - 1) {
		device_prefered = -1;
		return;
	}

	device_prefered = device;

	if (settings.device != device_prefered)
		settings.device = device_prefered;
}

void app_entry_set_device(int device) {
	if (device < 0)
		return;

	if (device > DEVICE_COUNT - 1)
		return;

	ta_ae.umount = true;
	ta_ae.mount = device;
}

bool app_entry_get_path(char *buf) {
	if (device_active < 0)
		return false;

	strcpy(buf, devices[device_active].name);
	return true;
}

bool app_entry_get_filename(char *buf, app_entry *app) {
	if (device_active < 0)
		return false;

	sprintf(buf, "%s:%s/%s/", devices[device_active].name,
			app_path, app->dirname);

	switch(app->type) {
	case AET_BOOT_ELF:
		strcat(buf, app_fn_boot_elf);
		return true;
	case AET_BOOT_DOL:
		strcat(buf, app_fn_boot_dol);
		return true;
	case AET_THEME:
		strcat(buf, app_fn_theme);
		return true;
	}

	return false;
}

app_entry *app_entry_find(char *dirname) {
	u32 i;

	for (i = 0; i < entry_count; ++i)
		if (!strcasecmp(entries[i]->dirname, dirname))
			return entries[i];

	return NULL;
}

bool app_entry_is_loading(void) {
	if (!ta_ae.running)
		return false;

	return ta_ae.loading;
}

