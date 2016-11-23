#ifndef _DIALOGS_H_
#define _DIALOGS_H_

#include <gctypes.h>

#include "gfx.h"
#include "view.h"

typedef enum {
	DLGMT_INFO = 0,
	DLGMT_CONFIRM,
	DLGMT_WARNING,
	DLGMT_ERROR
} dialog_message_type;

typedef enum {
	DLGB_OK,
	DLGB_OKCANCEL,
	DLGB_YESNO
} dialog_message_buttons;

typedef struct {
	bool confirmed;
	int device;
	app_sort sort;
} dialog_options_result;

extern u16 width_dialog, height_dialog;

void dialogs_init (void);
void dialogs_theme_reinit (void);
void dialogs_deinit (void);

void dialog_fade (view *v, bool fade_in);

view * dialog_app (const app_entry *entry, const view *sub_view);
view * dialog_progress (const view *sub_view, const char *caption, u32 max);
void dialog_set_progress (const view *v, u32 progress);
view * dialog_about (const view *sub_view);

s8 show_message (const view *sub_view, dialog_message_type type,
					dialog_message_buttons buttons, const char *text, u8 focus);
dialog_options_result show_options_dialog(const view *sub_view);

#endif

