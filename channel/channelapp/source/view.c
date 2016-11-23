#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include <ogcsys.h>

#include "../config.h"
#include "controls.h"
#include "theme.h"
#include "cursors.h"
#include "widgets.h"
#include "bubbles.h"
#include "panic.h"
#include "view.h"

#define FOCUS_FLAGS (WF_VISIBLE | WF_ENABLED | WF_FOCUSABLE)
#define CURSOR_FLAGS (WF_VISIBLE | WF_ENABLED)

#define RUMBLE_TIME 3
#define RUMBLE_DELAY 12

u8 *cursor_data;
u32 cursor_data_size;
u8 *cursor_shade_data;
u32 cursor_shade_data_size;

static gfx_queue_entry entry_bg;
static gfx_queue_entry entry_logo;
static gfx_queue_entry entry_throbber;

static gfx_entity gradient_subview;
static gfx_queue_entry entry_subview;

static bool cursor_enabled;
static bool rumble_enabled;
static bool throbber_enabled;

static gfx_queue_entry cur[2];

static s8 rumble_timeout = -RUMBLE_DELAY;

bool view_bubbles = false;

void view_init (void) {
	cursor_enabled = false;
	rumble_enabled = (CONF_GetPadMotorMode() == 0) ? 0 : 1;
	throbber_enabled = false;

	view_theme_reinit();
	bubbles_init();
}

void view_deinit (void) {
	cursor_enabled = false;
	bubbles_deinit();
}

void view_theme_reinit(void) {
	gfx_qe_entity(&entry_bg, theme_gfx[THEME_BACKGROUND],
					0, 0, -3, COL_DEFAULT);
	gfx_qe_entity(&entry_logo, theme_gfx[THEME_LOGO], 0, 416, 0, COL_DEFAULT);
	gfx_qe_entity(&entry_throbber, theme_gfx[THEME_THROBBER],
					(view_width - theme_gfx[THEME_THROBBER]->w) / 2,
					(view_height - theme_gfx[THEME_THROBBER]->h) / 2,
					0, COL_DEFAULT);
}

view * view_new (u8 widget_count, const view *sub_view, s16 x, s16 y, s16 z,
				 u32 drag_btn) {
	view *v;

	v = (view *) pmalloc (sizeof (view));
	memset (v, 0, sizeof (view));

	v->coords.x = x;
	v->coords.y = y;
	v->coords.z = z;
	gfx_qe_origin_push (&v->qe_coords_push, &v->coords);
	gfx_qe_origin_pop (&v->qe_coords_pop);

	v->widget_count = widget_count;

	v->widgets = (widget *) pmalloc (widget_count * sizeof (widget));
	memset (v->widgets, 0, widget_count * sizeof (widget));

	v->sub_view = sub_view;

	v->focus = -1;
	v->cursor = -1;

	v->drag = false;
	v->drag_btn = drag_btn;

	return v;
}

void view_free (view *v) {
	u8 i;

	if (!v)
		return;

	for (i = 0; i < v->widget_count; ++i)
		widget_free(&v->widgets[i]);

	free(v->widgets);
	free(v);
}

static void view_push_view (const view *v, u32 subview_alpha) {
	int i, j;
	widget *w;
	widget_layer *l;
	u32 f;

	if (v->sub_view) {
		view_push_view (v->sub_view, false);
		gfx_gen_gradient (&gradient_subview, view_width + 10,
							view_height + 10, subview_alpha,
							subview_alpha, subview_alpha, subview_alpha);

		gfx_qe_entity (&entry_subview, &gradient_subview, -5, -5,
						v->coords.z - 1, COL_DEFAULT);

		gfx_frame_push (&entry_subview, 1);
	}

	gfx_frame_push (&v->qe_coords_push, 1);

	for (i = 0; i < v->widget_count; ++i) {
		w = &v->widgets[i];

		if (w->flags & WF_VISIBLE) {
			gfx_frame_push (&w->qe_coords_push, 1);
			for (j = 0; j < w->layer_count; ++j) {
				l = &w->layers[j];

				if (l->flags) {
					f = w->flags ^ l->flags_invert;
					f = f & l->flags & WF_FLAGS_MASK;

					if (!f)
						continue;

					if (l->flags & WF_FLAGS_AND &&
							f != (l->flags & WF_FLAGS_MASK))
						continue;
				}

				gfx_frame_push (w->layers[j].gfx_entries,
								w->layers[j].gfx_entry_count);
			}

			gfx_frame_push (&w->qe_coords_pop, 1);
		}
	}

	gfx_frame_push (&v->qe_coords_pop, 1);
}

void view_plot (view *v, u32 alpha, u32 *down, u32 *held, u32 *up) {
	u32 bd, bh, bu;
	bool wm;
	s32 x, y;
	f32 roll;
	s8 w, c;
	cursor_type ct;
#ifdef ENABLE_SCREENSHOTS
	u16 ss_x, ss_y;
	u32 *ss_buf = NULL;
#endif

	controls_scan (&bd, &bh, &bu);
	wm = controls_ir (&x, &y, &roll);
	w = view_widget_at_xy (v, x - v->coords.x, y - v->coords.y);
	ct = CUR_STD;

	if (v->drag_btn) {
		if (bu & v->drag_btn)
			v->drag = false;

		if ((w != -1) && (bd & v->drag_btn)) {
			v->drag = true;

			v->drag_widget = w;

			v->drag_start_x = x;
			v->drag_start_y = y;
		}

		if (v->drag && (bh & v->drag_btn)) {
			v->drag_x = x - v->drag_start_x;
			v->drag_y = y - v->drag_start_y;

			ct = v->widgets[v->drag_widget].cur;
		}
	}

	c = v->cursor;

	if (v->cursor != -1) {
		widget_set_flag (&v->widgets[v->cursor], WF_CURSOR, false);
		v->cursor = -1;
	}

	wm = wm && cursor_enabled;

	if (wm) {
		view_set_focus (v, -1);

		if (w != -1) {
			if ((v->widgets[w].flags & CURSOR_FLAGS) == CURSOR_FLAGS) {
				widget_set_flag (&v->widgets[w], WF_CURSOR, true);
				v->cursor = w;

				if (rumble_enabled && (c != w) && (ct != CUR_DRAG) &&
						(v->widgets[w].flags & WF_RUMBLE) &&
						(rumble_timeout == -RUMBLE_DELAY)) {
					rumble_timeout = RUMBLE_TIME;
					controls_rumble(1);
				} 
			}

			if ((v->widgets[w].flags & FOCUS_FLAGS) == FOCUS_FLAGS)
				view_set_focus (v, w);
		}
	}

	gfx_frame_start ();

	gfx_frame_push (&entry_bg, 1);

	if (view_bubbles)
		bubble_update(wm, x, y);

	gfx_frame_push (&entry_logo, 1);

	view_push_view (v, alpha);

	if (throbber_enabled)
		gfx_frame_push(&entry_throbber, 1);

	if (wm) {
		if ((w != -1) && (ct != CUR_DRAG))
			ct = v->widgets[w].cur;

		cursors_queue (cur, ct, x, y, roll);

		gfx_frame_push (cur, 2);
	}

#ifdef ENABLE_SCREENSHOTS
	// ZR + ZL on Classic Controller
	if ((bh & PADS_NET_INIT) && (bd & PADS_SCREENSHOT)) {
		gfx_get_efb_size(&ss_x, &ss_y);
		ss_buf = (u32 *) pmalloc(ss_x * ss_y * sizeof(u32));

		gfx_set_efb_buffer(ss_buf);
	}
#endif

	gfx_frame_end ();

#ifdef ENABLE_SCREENSHOTS
	if (ss_buf) {
		save_rgba_png(ss_buf, ss_x, ss_y);
		free(ss_buf);
	}
#endif

	if (down)
		*down = bd;

	if (held)
		*held = bh;

	if (up)
		*up = bu;

	if (rumble_timeout > -RUMBLE_DELAY)
		rumble_timeout--;

	if (rumble_timeout < 1)
		controls_rumble(0);
}

void view_fade (view *v, s16 z, u32 c1, u32 c2, u32 c3, u32 c4, u8 steps,
				s8 modifier) {
	view *vf;
	u8 i;

	vf = view_new (1, v, 0, 0, 0, 0);

	widget_gradient (&vf->widgets[0], -32, -32, z, view_width + 64,
						view_height + 64, c1, c2, c3, c4);

	for (i = 0; i < steps; ++i) {
		widget_fade_gradient (&vf->widgets[0], modifier);

		view_plot (vf, 0, NULL, NULL, NULL);
	}
}

void view_set_focus (view *v, s8 new_focus) {
	if (v->focus != -1)
		v->widgets[v->focus].flags &= ~((u32) WF_FOCUSED);

	v->focus = new_focus;

	if (v->focus != -1)
		v->widgets[v->focus].flags |= WF_FOCUSED;
}

u8 view_set_focus_prev (view *v) {
	s16 i;

	if (v->focus < 1)
		return v->focus;

	for (i = v->focus - 1; i >= 0; --i)
		if ((v->widgets[i].flags & FOCUS_FLAGS) == FOCUS_FLAGS) {
			view_set_focus (v, i);

			break;
		}

	return v->focus;
}

u8 view_set_focus_next (view *v) {
	u8 i;

	if (v->focus == v->widget_count - 1)
		return v->focus;

	for (i = v->focus + 1; i < v->widget_count; ++i)
		if ((v->widgets[i].flags & FOCUS_FLAGS) == FOCUS_FLAGS) {
			view_set_focus (v, i);

			break;
		}

	return v->focus;
}

u8 view_move_focus(view *v, s8 mod) {
	if (v->focus + mod < 1)
		return v->focus;

	if (v->focus + mod > v->widget_count - 1)
		return v->focus;

	if ((v->widgets[v->focus + mod].flags & FOCUS_FLAGS) == FOCUS_FLAGS)
		view_set_focus (v, v->focus + mod);

	return v->focus;
}

void view_enable_cursor (bool enable) {
	cursor_enabled = enable;
}

s8 view_widget_at_xy (const view *v, s32 x, s32 y) {
	s16 i;
	widget *w;
	s16 wx, wy;

	for (i = v->widget_count - 1; i >= 0; --i) {
		w = &v->widgets[i];

		if (w->type == WT_MEMO) {
			wx = w->coords.x;
			wy = w->memo.y_max;
		} else {
			wx = w->coords.x;
			wy = w->coords.y;
		}

		if (w->width && w->height && (x >= wx) && (y >= wy) &&
				(x <= wx + w->width) && (y <= wy + w->height))
			return i;
	}

	return -1;
}

s8 view_widget_at_ir (const view *v) {
	s32 x, y;

	if (!cursor_enabled || !controls_ir (&x, &y, NULL))
		return -1;

	return view_widget_at_xy (v, x - v->coords.x, y - v->coords.y);
}

void view_show_throbber(bool show) {
	entry_throbber.entity.rad = 0;
	throbber_enabled = show;
}

void view_throbber_tickle(void) {
	entry_throbber.entity.rad -= 0.1f;
}

