#ifndef _FONT_H_
#define _FONT_H_

#include <gctypes.h>

#include "gfx.h"

typedef enum {
	FONT_LABEL = 0,
	FONT_DLGTITLE,
	FONT_MEMO,
	FONT_APPNAME,
	FONT_APPDESC,
	FONT_BUTTON,
	FONT_BUTTON_DESEL,

	FONT_MAX
} font_id;

typedef enum {
	FA_LEFT = 0,
	FA_CENTERED,
	FA_RIGHT
} font_xalign;

typedef enum {
	FA_ASCENDER = 0,
	FA_EM_TOP,
	FA_EM_CENTERED,
	FA_BASELINE,
	FA_DESCENDER,
} font_yalign;


void font_init (void);
void font_clear (void);
void font_deinit (void);

int font_get_height (font_id id);
int font_get_em_height (font_id id);
int font_get_ascender (font_id id);
int font_get_y_spacing (font_id id);
int font_get_min_y(font_id id);

int font_get_char_count (font_id id, const char *s, u16 max_width);
int font_wrap_string (char ***lines, font_id id, const char *s, u16 max_width);
void font_free_lines (char **lines, u32 count);

void font_plot_string (gfx_queue_entry *entries, int count, font_id id,
					   const char *s, u16 x, u16 y, u16 layer, u16 width,
					   font_xalign xalign, font_yalign yalign);

#endif

