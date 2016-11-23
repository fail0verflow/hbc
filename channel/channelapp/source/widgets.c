#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "../config.h"
#include "theme.h"
#include "i18n.h"
#include "panic.h"

#include "widgets.h"

static const char *app_entry_default_description;

void widgets_theme_reinit (void) {
	app_entry_default_description = _("<no description>");
}

void widgets_init (void) {
	widgets_theme_reinit();
}

void widgets_deinit (void) {
}

void widget_free (widget *w) {
	u32 i;

	for (i = 0; i < w->layer_count; ++i)
		free (w->layers[i].gfx_entries);

	switch (w->type) {
	case WT_LABEL:
	case WT_BUTTON:
	case WT_APP_ENTRY:
	case WT_MEMO:
	case WT_IMAGE:
		break;

	case WT_PROGRESS:
		free (w->progress.gradient);
		break;

	case WT_GRADIENT:
		free (w->gradient.gradient);
		break;
	}

	free (w->layers);

	memset(w, 0, sizeof(widget));
}

void widget_set_flag (widget *w, u32 flag, bool set) {
	if (set)
		w->flags |= flag;
	else
		w->flags &= ~((u32) flag);
}

void widget_toggle_flag (widget *w, u32 flag) {
	widget_set_flag (w, flag, (w->flags & flag) ? false : true);
}

void widget_set_progress (widget *w, u32 progress) {
	float m = 0;
	char p[5];

	if (progress > 0)
		m = (float) progress / w->progress.max;

	if (m > 1)
		m = 1;

	gfx_gen_gradient (w->progress.gradient, roundf ((400 - 36 * 2 + 2) * m),
						112 - 36 * 2 + 2, theme.progress.ul, theme.progress.ur,
						theme.progress.lr, theme.progress.ll);
	gfx_qe_entity (&w->layers[0].gfx_entries[0], w->progress.gradient, 35, 35,
					0, COL_DEFAULT);

	sprintf (p, "%3.f%%", roundf (100.0 * m));
	font_plot_string (&w->layers[2].gfx_entries[0], 4, FONT_LABEL, p,
						32, theme_gfx[THEME_PROGRESS]->h - 20, 2,
						theme_gfx[THEME_PROGRESS]->w - 70,
						FA_RIGHT, FA_EM_CENTERED);
}

#define CCLAMP(x) (((x)>255)?255:(((x)<0)?0:(x)))

void widget_fade_gradient (widget *w, s8 modifier) {
	w->layers[0].gfx_entries[0].entity.entity->gradient.c1 =
		CCLAMP(w->layers[0].gfx_entries[0].entity.entity->gradient.c1 + modifier);
	w->layers[0].gfx_entries[0].entity.entity->gradient.c2 =
		CCLAMP(w->layers[0].gfx_entries[0].entity.entity->gradient.c2 + modifier);
	w->layers[0].gfx_entries[0].entity.entity->gradient.c3 =
		CCLAMP(w->layers[0].gfx_entries[0].entity.entity->gradient.c3 + modifier);
	w->layers[0].gfx_entries[0].entity.entity->gradient.c4 =
		CCLAMP(w->layers[0].gfx_entries[0].entity.entity->gradient.c4 + modifier);
}

bool widget_scroll_memo (widget *w, s16 modifier) {
	s16 y;

	y = w->coords.y + modifier;

	if (y < w->memo.y_min)
		y = w->memo.y_min;

	if (y > w->memo.y_max)
		y = w->memo.y_max;

	if (w->coords.y == y)
		return false;

	w->coords.y = y;

	return true;
}

bool widget_scroll_memo_deco (widget *w, s16 modifier) {
	bool res;

	res = widget_scroll_memo (&w[1], modifier);

	widget_set_flag (&w[0], WF_VISIBLE, w[1].coords.y < w[1].memo.y_max);
	widget_set_flag (&w[2], WF_VISIBLE, w[1].coords.y > w[1].memo.y_min);

	return res;
}

static void widget_init (widget *w, widget_type type, u32 focusable,
						 s16 x, s16 y, s16 z, u16 width, u16 height,
						 int layer_count) {
	w->type = type;
	w->flags = WF_VISIBLE | WF_ENABLED;

	w->width = width;
	w->height = height;

	w->cur = CUR_STD;

	if (focusable)
		w->flags |= WF_FOCUSABLE;

	w->coords.x = x;
	w->coords.y = y;
	w->coords.z = TEX_LAYER_WIDGETS + z;
	gfx_qe_origin_push (&w->qe_coords_push, &w->coords);
	gfx_qe_origin_pop (&w->qe_coords_pop);

	w->layer_count = layer_count;
	w->layers = (widget_layer *)
				pmalloc (w->layer_count * sizeof (widget_layer));
}

void widget_label (widget *w, s16 x, s16 y, s16 z, const char *caption,
				   u16 width, font_xalign xalign, font_yalign yalign,
				   font_id font) {
	widget_layer *l;

	widget_init (w, WT_LABEL, false, x, y, z, 0, 0, 1);

	l = &w->layers[0];
	l->flags = 0;
	l->gfx_entry_count = font_get_char_count (font, caption, width);
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	font_plot_string (&l->gfx_entries[0], l->gfx_entry_count, font, caption,
						0, 0, 0, width, xalign, yalign);
}

void widget_image(widget *w, s16 x, s16 y, s16 z, gfx_entity *image,
				  gfx_entity *image_disabled, bool rumble,
				  gfx_entity *image_cursor) {
	int c;
	widget_layer *l;

	c = 1;

	if (image_disabled)
		c++;

	if (image_cursor)
		c++;

	widget_init (w, WT_IMAGE, false, x, y, z, image->w, image->h, c);

	if (rumble)
		w->flags |= WF_RUMBLE;

	c = 0;

	l = &w->layers[c];
	if(image_cursor) {
		l->flags = WF_ENABLED | WF_FLAGS_AND | WF_CURSOR;
		l->flags_invert = WF_CURSOR;
	} else {
		l->flags = WF_ENABLED;
		l->flags_invert = 0;
	}

	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	gfx_qe_entity (&l->gfx_entries[0], image, 0, 0, 0, COL_DEFAULT);

	c++;

	if (image_disabled) {
		l = &w->layers[c];
		l->flags = WF_ENABLED;
		l->flags_invert = WF_ENABLED;
		l->gfx_entry_count = 1;
		l->gfx_entries = (gfx_queue_entry *) pmalloc (l->gfx_entry_count *
							sizeof (gfx_queue_entry));
		gfx_qe_entity (&l->gfx_entries[0], image_disabled, 0, 0, 1, COL_DEFAULT);

		c++;
	}

	if (image_cursor) {
		l = &w->layers[c];
		l->flags = WF_ENABLED | WF_FLAGS_AND | WF_CURSOR;
		l->flags_invert = 0;
		l->gfx_entry_count = 1;
		l->gfx_entries = (gfx_queue_entry *) pmalloc (l->gfx_entry_count *
							sizeof (gfx_queue_entry));
		gfx_qe_entity (&l->gfx_entries[0], image_cursor, 0, 0, 2, COL_DEFAULT);
	}
}

void widget_button (widget *w, s16 x, s16 y, s16 z, button_size size,
					const char *caption) {
	gfx_entity *tex, *tex_focus;
	widget_layer *l;

	switch (size) {
	case BTN_SMALL:
		tex = theme_gfx[THEME_BUTTON_SMALL];
		tex_focus = theme_gfx[THEME_BUTTON_SMALL_FOCUS];
		break;

	case BTN_TINY:
		tex = theme_gfx[THEME_BUTTON_TINY];
		tex_focus = theme_gfx[THEME_BUTTON_TINY_FOCUS];
		break;

	default:
		tex = theme_gfx[THEME_BUTTON];
		tex_focus = theme_gfx[THEME_BUTTON_FOCUS];
		break;
	}

	widget_init (w, WT_BUTTON, true, x, y, z, tex->w, tex->h, 4);

	w->flags |= WF_RUMBLE;

	l = &w->layers[0];
	l->flags = WF_FOCUSED | WF_FLAGS_AND | WF_ENABLED;
	l->flags_invert = WF_FOCUSED;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	gfx_qe_entity (&l->gfx_entries[0], tex, 0, 0, 0, COL_DEFAULT);

	l = &w->layers[1];
	l->flags = WF_ENABLED;
	l->flags_invert = WF_ENABLED;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	gfx_qe_entity (&l->gfx_entries[0], tex, 0, 0, 0, WIDGET_DISABLED_COLOR);

	l = &w->layers[2];
	l->flags = WF_FOCUSED | WF_FLAGS_AND | WF_ENABLED;
	l->flags_invert = 0;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	gfx_qe_entity (&l->gfx_entries[0], tex_focus, 0, 0, 0, COL_DEFAULT);

	l = &w->layers[3];
	l->flags = 0;
	l->flags_invert = 0;
	l->gfx_entry_count = 0;
	l->gfx_entries = NULL;

	widget_button_set_caption(w, FONT_BUTTON, caption);
}

void widget_button_set_caption(widget *w, font_id font, const char *caption) {
	int c;
	u16 bw, bh, oy;
	widget_layer *l;

	l = &w->layers[3];
	free(l->gfx_entries);
	l->gfx_entry_count = 0;
	l->gfx_entries = NULL;

	if (!caption)
		return;

	bw = w->layers[0].gfx_entries[0].entity.entity->w;
	bh = w->layers[0].gfx_entries[0].entity.entity->h;

	c = font_get_char_count(font, caption, bw - 32);

	oy = bh/2;

	l->gfx_entry_count = c;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	font_plot_string (&l->gfx_entries[0], c, font, caption, 16, oy, 1,
						bw - 32, FA_CENTERED, FA_EM_CENTERED);
}

void widget_grid_app_entry(widget *w, s16 x, s16 y, s16 z,
						   const app_entry *entry) {

	widget_layer *l;

	if (entry->icon)
		widget_init(w, WT_APP_ENTRY, true, x, y, z,
					theme_gfx[THEME_GRID_APP_ENTRY]->w,
					theme_gfx[THEME_GRID_APP_ENTRY]->h, 4);
	else
		widget_init(w, WT_APP_ENTRY, true, x, y, z,
					theme_gfx[THEME_GRID_APP_ENTRY]->w,
					theme_gfx[THEME_GRID_APP_ENTRY]->h, 3);

	// Enable rumbling for this widget
	w->flags |= WF_RUMBLE;

	l = &w->layers[0];
	l->flags = WF_FOCUSED | WF_FLAGS_AND | WF_ENABLED;
	l->flags_invert = WF_FOCUSED;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	gfx_qe_entity (&l->gfx_entries[0], theme_gfx[THEME_GRID_APP_ENTRY],
					0, 0, 0, COL_DEFAULT);

	l = &w->layers[1];
	l->flags = WF_ENABLED;
	l->flags_invert = WF_ENABLED;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	gfx_qe_entity (&l->gfx_entries[0], theme_gfx[THEME_GRID_APP_ENTRY],
					0, 0, 0, WIDGET_DISABLED_COLOR);

	l = &w->layers[2];
	l->flags = WF_FOCUSED | WF_FLAGS_AND | WF_ENABLED;
	l->flags_invert = 0;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	gfx_qe_entity (&l->gfx_entries[0], theme_gfx[THEME_GRID_APP_ENTRY_FOCUS],
					0, 0, 0, COL_DEFAULT);

	if (entry->icon) {
		l = &w->layers[3];
		l->flags = 0;
		l->flags_invert = 0;
		l->gfx_entry_count = 1;

		l->gfx_entry_count = 1;
		l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

		gfx_qe_entity (&l->gfx_entries[0], entry->icon, GRID_APP_ENTRY_ICON_X,
						APP_ENTRY_ICON_Y, 1, COL_DEFAULT);
	}
}

void widget_app_entry (widget *w, s16 x, s16 y, s16 z, const app_entry *entry) {
	const char *line1, *line2;
	int l1, l2;

	widget_layer *l;

	widget_init (w, WT_APP_ENTRY, true, x, y, z, theme_gfx[THEME_APP_ENTRY]->w,
					theme_gfx[THEME_APP_ENTRY]->h, 4);

	w->flags |= WF_RUMBLE;

	l = &w->layers[0];
	l->flags = WF_FOCUSED | WF_FLAGS_AND | WF_ENABLED;
	l->flags_invert = WF_FOCUSED;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	gfx_qe_entity (&l->gfx_entries[0], theme_gfx[THEME_APP_ENTRY],
					0, 0, 0, COL_DEFAULT);

	l = &w->layers[1];
	l->flags = WF_ENABLED;
	l->flags_invert = WF_ENABLED;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	gfx_qe_entity (&l->gfx_entries[0], theme_gfx[THEME_APP_ENTRY],
					0, 0, 0, WIDGET_DISABLED_COLOR);

	l = &w->layers[2];
	l->flags = WF_FOCUSED | WF_FLAGS_AND | WF_ENABLED;
	l->flags_invert = 0;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	gfx_qe_entity (&l->gfx_entries[0], theme_gfx[THEME_APP_ENTRY_FOCUS],
					0, 0, 0, COL_DEFAULT);

	line1 = NULL;
	line2 = NULL;

	if (entry->meta) {
		line1 = entry->meta->name;
		line2 = entry->meta->short_description;
	}

	if (!line1)
		line1 = entry->dirname;

	if (!line2)
		line2 = app_entry_default_description;

	l1 = font_get_char_count(FONT_APPNAME, line1,
							theme_gfx[THEME_APP_ENTRY]->w -
							APP_ENTRY_TEXT1_X - 16);
	l2 = font_get_char_count(FONT_APPDESC, line2,
							theme_gfx[THEME_APP_ENTRY]->w -
							APP_ENTRY_TEXT2_X - 16);

	l = &w->layers[3];
	l->flags = 0;
	l->flags_invert = 0;
	l->gfx_entry_count = l1 + l2;

	if (entry->icon)
		l->gfx_entry_count++;

	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	font_plot_string (&l->gfx_entries[0], l1, FONT_APPNAME, line1,
						APP_ENTRY_TEXT1_X, APP_ENTRY_TEXT1_Y, 1,
						theme_gfx[THEME_APP_ENTRY]->w - APP_ENTRY_TEXT1_X -
						16, FA_LEFT, FA_ASCENDER);
	font_plot_string (&l->gfx_entries[l1], l2, FONT_APPDESC, line2,
						APP_ENTRY_TEXT2_X, APP_ENTRY_TEXT2_Y, 1,
						theme_gfx[THEME_APP_ENTRY]->w - APP_ENTRY_TEXT2_X -
						16, FA_LEFT, FA_DESCENDER);

	if (entry->icon)
		gfx_qe_entity (&l->gfx_entries[l1 + l2], entry->icon,
						APP_ENTRY_ICON_X, APP_ENTRY_ICON_Y, 1, COL_DEFAULT);
}

void widget_progress (widget *w, s16 x, s16 y, s16 z, const char *caption,
					  u32 max) {
	widget_layer *l;

	widget_init (w, WT_PROGRESS, false, x, y, z, 0, 0, 3);

	l = &w->layers[0];
	l->flags = 0;
	l->flags_invert = 0;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	l = &w->layers[1];
	l->flags = 0;
	l->flags_invert = 0;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	gfx_qe_entity (&l->gfx_entries[0], theme_gfx[THEME_PROGRESS],
					0, 0, 1, COL_DEFAULT);

	int chars = font_get_char_count(FONT_LABEL, caption,
									theme_gfx[THEME_PROGRESS]->w - 64);

	l = &w->layers[2];
	l->flags = 0;
	l->flags_invert = 0;
	l->gfx_entry_count = 4 + chars;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));
	font_plot_string (&l->gfx_entries[4], chars, FONT_LABEL, caption, 38, 19, 2,
						theme_gfx[THEME_PROGRESS]->w - 64, FA_LEFT, FA_EM_CENTERED);

	w->progress.max = max;
	w->progress.gradient = (gfx_entity *) pmalloc (sizeof (gfx_entity));
}

void widget_gradient (widget *w, s16 x, s16 y, s16 z,
						u16 width, u16 height,
						u32 c1, u32 c2, u32 c3,
						u32 c4) {
	widget_layer *l;

	widget_init (w, WT_GRADIENT, false, x, y, z, 0, 0, 1);

	w->gradient.gradient = (gfx_entity *) pmalloc (sizeof (gfx_entity));
	gfx_gen_gradient (w->gradient.gradient, width, height, c1, c2, c3, c4);

	l = &w->layers[0];
	l->flags = 0;
	l->flags_invert = 0;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	gfx_qe_entity (&l->gfx_entries[0], w->gradient.gradient, 0, 0, 0,
					COL_DEFAULT);
}

void widget_memo (widget *w, s16 x, s16 y, s16 z, u16 width, u16 height,
				  const char *text, font_xalign align) {
	widget_layer *l;
	int hf;
	int count, i, c;
	char **lines;
	int cl, ot;
	int oy;

	hf = font_get_y_spacing(FONT_MEMO);

	count = font_wrap_string (&lines, FONT_MEMO, text, width);

	c = 0;
	for (i = 0; i < count; ++i)
		if (lines[i])
			c++;

	ot = 0;
	cl = (height + hf - 1) / hf;
	if (cl > count)
		ot = ((cl - count) / 2) * hf;

	widget_init (w, WT_MEMO, false, x, y + ot, z, width, height, c + 2);

	if (c > cl)
		w->cur = CUR_DRAG;

	l = &w->layers[0];
	l->flags = 0;
	l->flags_invert = 0;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	gfx_qe_scissor (&l->gfx_entries[0], x, y, z, width, height);

	c = 1;
	oy = 0;

	w->memo.y_max = w->coords.y;

	for (i = 0; i < count; ++i) {
		if (!lines[i]) {
			oy += hf;
			continue;
		}

		l = &w->layers[c];
		l->flags = 0;
		l->flags_invert = 0;
		l->gfx_entry_count = font_get_char_count(FONT_MEMO, lines[i], 0);
		l->gfx_entries = (gfx_queue_entry *) pmalloc (l->gfx_entry_count *
							sizeof (gfx_queue_entry));
		font_plot_string (&l->gfx_entries[0], l->gfx_entry_count, FONT_MEMO,
							lines[i], 0, oy, 0, width, align, FA_ASCENDER);
		c++;
		oy += hf;
	}

	font_free_lines (lines, count);

	l = &w->layers[c];
	l->flags = 0;
	l->flags_invert = 0;
	l->gfx_entry_count = 1;
	l->gfx_entries = (gfx_queue_entry *)
						pmalloc (l->gfx_entry_count * sizeof (gfx_queue_entry));

	gfx_qe_scissor_reset (&l->gfx_entries[0]);

	w->memo.y_min = w->memo.y_max - oy + height;
}

void widget_memo_deco (widget *w, s16 x, s16 y, s16 z, u16 width, u16 height,
					   const char *text, font_xalign align) {
	widget_image (&w[0], x + (width - theme_gfx[THEME_CONTENT_ARROW_UP]->w) / 2,
					y, z, theme_gfx[THEME_CONTENT_ARROW_UP], NULL, false, NULL);
	widget_memo (&w[1], x, y + theme_gfx[THEME_CONTENT_ARROW_UP]->h + 8, z,
					width, height - theme_gfx[THEME_CONTENT_ARROW_UP]->h -
					8 - theme_gfx[THEME_CONTENT_ARROW_DOWN]->h - 8, text,
					align);
	widget_image (&w[2],
					x + (width - theme_gfx[THEME_CONTENT_ARROW_DOWN]->w) / 2,
					y + height - theme_gfx[THEME_CONTENT_ARROW_DOWN]->h, z,
					theme_gfx[THEME_CONTENT_ARROW_DOWN], NULL, false, NULL);
	widget_scroll_memo_deco (w, 0);
}

