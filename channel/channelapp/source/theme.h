#ifndef _THEME_H_
#define _THEME_H_

#include <gctypes.h>

#include "gfx.h"
#include "xml.h"

typedef enum {
	// view.c
	THEME_BACKGROUND = 0,
	THEME_LOGO,

	// bubbles.c
	THEME_BUBBLE1,
	THEME_BUBBLE2,
	THEME_BUBBLE3,

	// browser.c
	THEME_ARROW_LEFT,
	THEME_ARROW_LEFT_FOCUS,
	THEME_ARROW_RIGHT,
	THEME_ARROW_RIGHT_FOCUS,
	THEME_GECKO,
	THEME_GECKO_ACTIVE,
	THEME_LAN,
	THEME_LAN_ACTIVE,
	THEME_THROBBER,

	// dialogc.c
	THEME_ABOUT,
	THEME_DIALOG,
	THEME_DLG_INFO,
	THEME_DLG_CONFIRM,
	THEME_DLG_WARNING,
	THEME_DLG_ERROR,

	// widgets.c
	THEME_BUTTON,
	THEME_BUTTON_FOCUS,
	THEME_BUTTON_SMALL,
	THEME_BUTTON_SMALL_FOCUS,
	THEME_BUTTON_TINY,
	THEME_BUTTON_TINY_FOCUS,
	THEME_APP_ENTRY,
	THEME_APP_ENTRY_FOCUS,
	THEME_GRID_APP_ENTRY,
	THEME_GRID_APP_ENTRY_FOCUS,
	THEME_PROGRESS,
	THEME_CONTENT_ARROW_UP,
	THEME_CONTENT_ARROW_DOWN,

	THEME_LAST
} theme_entry;

typedef struct {
	const char *file;
	void *data;
	u32 data_len;
	int size;
	u32 color;
} theme_font;

extern gfx_entity *theme_gfx[THEME_LAST];

extern theme_font theme_fonts[FONT_MAX];

extern const char *theme_fn_xml;

void theme_init(u8 *data, u32 data_len);
void theme_deinit(void);

bool theme_is_valid_fn(const char *filename);

#endif

