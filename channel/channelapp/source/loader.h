#ifndef _LOADER_H_
#define _LOADER_H_

#include <unistd.h>

#include <gctypes.h>

#include "loader_reloc.h"
#include "appentry.h"
#include "view.h"

typedef enum {
	LT_UNKNOWN = 0,
	LT_EXECUTABLE,
	LT_ZIP_APP,
	LT_ZIP_THEME
} loader_type;

typedef struct {
	loader_type type;

	u8 *data;
	u32 data_len;

	char args[ARGS_MAX_LEN];
	u32 args_len;

	char dirname[MAXPATHLEN];
	u32 bytes;
} loader_result;

void loader_init (void);
void loader_deinit (void);

void loader_tcp_init (void);
void loader_signal_threads (void);
bool loader_gecko_initialized (void);
bool loader_tcp_initializing (void);
bool loader_tcp_initialized (void);
bool loader_handshaked (void);

void loader_load(loader_result *result, view *sub_view, app_entry *entry);
bool loader_load_executable(entry_point *ep, loader_result *result,
							view *sub_view);
bool loader_handle_zip_app(loader_result *result, view *sub_view);

#endif

