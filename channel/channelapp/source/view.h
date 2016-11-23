#ifndef _VIEW_H_
#define _VIEW_H_

#include <gctypes.h>

#include "widgets.h"

typedef struct _view {
	gfx_coordinates coords;
	gfx_queue_entry qe_coords_push, qe_coords_pop;

	u8 widget_count;
	widget *widgets;

	const struct _view *sub_view;

	s8 focus;
	s8 cursor;

	bool drag;
	s8 drag_widget;
	u32 drag_btn;
	s32 drag_start_x, drag_start_y, drag_x, drag_y;
} view;

extern bool view_bubbles;
extern bool allow_screenshot;

void view_init ();
void view_deinit ();
void view_theme_reinit(void);

view * view_new (u8 widget_count, const view *sub_view, s16 x, s16 y, s16 z,
				 u32 drag_btn);
void view_free (view *v);

void view_plot (view *v, u32 alpha, u32 *down, u32 *held, u32 *up);
void view_fade (view *v, s16 z, u32 c1, u32 c2, u32 c3, u32 c4, u8 steps,
				s8 modifier);

void view_set_focus (view *v, s8 new_focus);
u8 view_set_focus_prev (view *v);
u8 view_set_focus_next (view *v);
u8 view_move_focus(view *v, s8 mod);

void view_enable_cursor (bool enable);
s8 view_widget_at_xy (const view *v, s32 x, s32 y);
s8 view_widget_at_ir (const view *v);

void view_show_throbber(bool show);
void view_throbber_tickle(void);

#endif

