#ifndef _APPENTRY_H_
#define _APPENTRY_H_

#include <gctypes.h>

#include "gfx.h"
#include "xml.h"

#define DEVICE_COUNT 4
#define MAX_THEME_ZIP_SIZE (20 * 1024 * 1024)

typedef enum {
	AET_BOOT_ELF = 0,
	AET_BOOT_DOL,
	AET_THEME
} app_entry_type;

typedef struct {
	app_entry_type type;
	u32 size;
	char *dirname;
	gfx_entity *icon;
	meta_info *meta;
} app_entry;

typedef enum {
	AE_ACT_NONE = 0,
	AE_ACT_REMOVE,
	AE_ACT_ADD
} ae_action;

typedef enum {
	APP_FILTER_ALL = 0,
	APP_FILTER_ICONSONLY,
	APP_FILTER_DATEONLY
} app_filter;

typedef enum {
	APP_SORT_NAME = 0,
	APP_SORT_DATE
} app_sort;

extern const char *app_path;
extern const char *app_fn_boot_elf;
extern const char *app_fn_boot_dol;
extern const char *app_fn_theme;
extern const char *app_fn_meta;
extern const char *app_fn_icon;

extern app_entry *entries[MAX_ENTRIES];
extern u32 entry_count;

void app_entry_init (void);
void app_entry_deinit (void);

void app_entries_free(void);

void app_entry_scan(void);
ae_action app_entry_action(void);

void app_entry_poll_status(bool reset);
int app_entry_get_status(bool *status);

void app_entry_set_prefered(int device);
void app_entry_set_device(int device);

bool app_entry_get_path(char *buf);
bool app_entry_get_filename(char *buf, app_entry *app);
app_entry *app_entry_find(char *dirname);

app_sort app_entry_get_sort(void);
void app_entry_set_sort(app_sort sort);

app_entry *app_entry_add(const char *dirname);
bool app_entry_remove(app_entry *app);

bool app_entry_is_loading(void);

#endif

