#ifndef _WIDGETS_H_
#define _WIDGETS_H_

#include <gctypes.h>

#include "gfx.h"
#include "cursors.h"
#include "appentry.h"
#include "font.h"

#define WF_VISIBLE 1
#define WF_ENABLED 2
#define WF_FOCUSABLE 4
#define WF_RUMBLE 8

#define WF_CURSOR 32
#define WF_FOCUSED 64

#define WF_FLAGS_MASK 0xff
#define WF_FLAGS_OR 0
#define WF_FLAGS_AND 256

typedef enum {
	WT_LABEL,
	WT_IMAGE,
	WT_BUTTON,
	WT_APP_ENTRY,
	WT_PROGRESS,
	WT_GRADIENT,
	WT_MEMO
} widget_type;

typedef struct {
	u32 flags;
	u32 flags_invert;
	u16 gfx_entry_count;
	gfx_queue_entry *gfx_entries;
} widget_layer;

typedef struct {
	widget_type type;
	u32 flags;
	u16 width, height;

	cursor_type cur;

	gfx_coordinates coords;
	gfx_queue_entry qe_coords_push, qe_coords_pop;

	int layer_count;
	widget_layer *layers;

	union {
		struct {
			gfx_entity *mask;
		} image;

		struct {
			u32 max;
			gfx_entity *gradient;
		} progress;

		struct {
			gfx_entity *gradient;
		} gradient;

		struct {
			s16 y_min, y_max;
		} memo;
	};
} widget;

typedef enum {
	BTN_NORMAL = 0,
	BTN_SMALL,
	BTN_TINY
} button_size;

void widgets_init (void);
void widgets_theme_reinit (void);
void widgets_deinit (void);

void widget_free (widget *w);

void widget_set_flag (widget *w, u32 flag, bool set);
void widget_toggle_flag (widget *w, u32 flag);
void widget_set_progress (widget *w, u32 progress);
void widget_fade_gradient (widget *w, s8 modifier);
bool widget_scroll_memo (widget *w, s16 modifier);
bool widget_scroll_memo_deco (widget *w, s16 modifier);

void widget_label (widget *w, s16 x, s16 y, s16 z, const char *caption,
				   u16 width, font_xalign xalign, font_yalign yalign,
				   font_id font);

void widget_image(widget *w, s16 x, s16 y, s16 z,
					gfx_entity *image, gfx_entity *image_disabled,
					bool rumble, gfx_entity *image_cursor);

void widget_button (widget *w, s16 x, s16 y, s16 z,
					button_size size, const char *caption);
void widget_button_set_caption(widget *w, font_id font, const char *caption);

void widget_app_entry (widget *w, s16 x, s16 y, s16 z,
						const app_entry *entry);
void widget_grid_app_entry (widget *w, s16 x, s16 y, s16 z,
						const app_entry *entry);

void widget_progress (widget *w, s16 x, s16 y, s16 z,
						const char *caption, u32 max);
void widget_gradient (widget *w, s16 x, s16 y, s16 z,
						u16 width, u16 height,
						u32 c1, u32 c2, u32 c3, u32 c4);
void widget_memo (widget *w, s16 x, s16 y, s16 z,
					u16 width, u16 height,
					const char *text, font_xalign align);
void widget_memo_deco (widget *w, s16 x, s16 y, s16 z,
						u16 width, u16 height,
						const char *text, font_xalign align);

#endif

