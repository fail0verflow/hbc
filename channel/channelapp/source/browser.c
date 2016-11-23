#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>

#include "../config.h"
#include "theme.h"
#include "view.h"
#include "controls.h"
#include "i18n.h"

#include "browser.h"

#define AE_OFFSET 4
#define TRANS_STEPS 20
#define MAX_COLUMNS 4
#define ROWS 5

static bool first_set = true;
static u16 top_offset = 0;

static view *v_browser = NULL;

static int columns_current = 1;
static int columns_new = 1;

static bool inited_widgets = false;

view * browser_init(void) {
	v_browser = view_new (AE_OFFSET + (MAX_COLUMNS * ROWS * 2), NULL,
							0, 0, 0, 0);
	browser_theme_reinit();

	return v_browser;
}

void browser_deinit(void) {
	view_free(v_browser);
	inited_widgets = false;
	v_browser = NULL;
}

void browser_theme_reinit(void) {
	int i;
	if (inited_widgets)
		for (i = 0; i < v_browser->widget_count; ++i)
			widget_free(&v_browser->widgets[i]);

	widget_image (&v_browser->widgets[0], 24, 192, 0,
					theme_gfx[THEME_ARROW_LEFT], NULL, true,
					theme_gfx[THEME_ARROW_LEFT_FOCUS]);
	widget_image (&v_browser->widgets[1],
					view_width - 24 - theme_gfx[THEME_ARROW_RIGHT]->w, 192, 0,
					theme_gfx[THEME_ARROW_RIGHT], NULL, true,
					theme_gfx[THEME_ARROW_RIGHT_FOCUS]);
	widget_image (&v_browser->widgets[2],
					view_width - 32 - theme_gfx[THEME_GECKO_ACTIVE]->w -
					theme_gfx[THEME_LAN_ACTIVE]->w, 412, 0,
					theme_gfx[THEME_GECKO_ACTIVE], theme_gfx[THEME_GECKO],
					false, NULL);
	widget_image (&v_browser->widgets[3],
					view_width - 32 - theme_gfx[THEME_GECKO_ACTIVE]->w, 412, 0,
					theme_gfx[THEME_LAN_ACTIVE], theme_gfx[THEME_LAN],
					false, NULL);
	widget_set_flag (&v_browser->widgets[2], WF_ENABLED, false);
	widget_set_flag (&v_browser->widgets[3], WF_ENABLED, false);

	widget_set_flag (&v_browser->widgets[0], WF_VISIBLE, false);
	widget_set_flag (&v_browser->widgets[1], WF_VISIBLE, false);
	
	inited_widgets = true;
}

static void browser_set_top_offset(const app_entry *app) {
	u32 i;

	if (!app || (entry_count < 1)) {
		top_offset = 0;
		return;
	}

	for (i = 0; i < entry_count; ++i)
		if (entries[i] == app) {
			top_offset = i;
			break;
		}

	top_offset /= columns_new * ROWS;
	top_offset *= columns_new * ROWS;

	if (top_offset > entry_count - 1)
		top_offset -= columns_new * ROWS;
}
void browser_gen_view(browser_action action, const app_entry *app) {
	bool less, more;
	app_entry *entry;
	s8 focus;

	u32 i, j;
	u16 y;

	u8 o1, o2;
	s16 xal, xar, x1, x2;
	float xm;
	float vala = 0;
	float val1[MAX_COLUMNS * ROWS];
	float val2[MAX_COLUMNS * ROWS];
	float stepa = M_TWOPI / TRANS_STEPS;
	float step = M_PI / (TRANS_STEPS - 6);
	s16 s;
	float f1, f2;

	switch (action) {
	case BA_REMOVE:
		break;

	case BA_ADD:
	case BA_REFRESH:
		browser_set_top_offset(app);
		break;

	case BA_NEXT:
		if (entry_count <= top_offset + (columns_new * ROWS))
			return;
		top_offset += columns_current * ROWS;
		break;

	case BA_PREV:
		if (top_offset < 1)
			return;
		if (top_offset < columns_current * ROWS)
			return;
		top_offset -= columns_current * ROWS;
		break;
	}

	if (action == BA_REMOVE) {
		less = false;
		more = false;
	} else {
		less = top_offset > 0;
		more = entry_count > top_offset + (columns_new * ROWS);
	}

	memset(val1, 0, sizeof(float) * MAX_COLUMNS * ROWS);
	memset(val2, 0, sizeof(float) * MAX_COLUMNS * ROWS);

	if (first_set) {
		o1 = AE_OFFSET;
		o2 = AE_OFFSET + (MAX_COLUMNS * ROWS);
	} else {
		o1 = AE_OFFSET + (MAX_COLUMNS * ROWS);
		o2 = AE_OFFSET;
	}

	first_set = !first_set;
	focus = o2;

	xal = v_browser->widgets[0].coords.x;
	xar = v_browser->widgets[1].coords.x;

	if (columns_current == 1)
		x1 = (view_width - theme_gfx[THEME_APP_ENTRY]->w) / 2;
	else
		x1 = (view_width - (theme_gfx[THEME_GRID_APP_ENTRY]->w *
							columns_current)) / 2;

	if (columns_new == 1)
		x2 = (view_width - theme_gfx[THEME_APP_ENTRY]->w) / 2;
	else
		x2 = (view_width - (theme_gfx[THEME_GRID_APP_ENTRY]->w *
							columns_new)) / 2;

	if (action == BA_PREV) {
		xm = view_width / 2;
		x2 = -view_width + x2;
	} else {
		xm = -view_width / 2;
		x2 = view_width + x2;
	}

	y = 64;

	for (i = 0; i < (MAX_COLUMNS * ROWS); ++i)
		widget_free(&v_browser->widgets[o2 + i]);

	if (action != BA_REMOVE)
		for (i = 0; i < (columns_new * ROWS); ++i) {
			if (entry_count > top_offset + i) {
				entry = entries[top_offset + i];

				if (entry && (entry == app))
					focus += i;

				if (columns_new == 1)
					widget_app_entry(&v_browser->widgets[o2 + i],
									x2, y, 0, entry);
				else
					widget_grid_app_entry(&v_browser->widgets[o2 + i],
											x2 + ((i % columns_new) *
											theme_gfx[THEME_GRID_APP_ENTRY]->w),
											y, 0, entry);
			}

			if (((i+1) % columns_new) == 0)
				y += theme_gfx[THEME_APP_ENTRY]->h;
		}

	for (i = 0; i < TRANS_STEPS; ++i) {
		vala += stepa;
		s = roundf (156.0 * (cosf (vala) - 1));

		// adjust L/R button positions
		v_browser->widgets[0].coords.x = xal + s;
		v_browser->widgets[1].coords.x = xar - s;

		for (j = 0; j < MAX_COLUMNS * ROWS; ++j) {
			if ((i > j / columns_current) &&
					(i < TRANS_STEPS - (ROWS - j / columns_current)))
				val1[j] += step;
			if ((i > j / columns_new) &&
					(i < TRANS_STEPS - (ROWS - j / columns_new)))
				val2[j] += step;

			f1 = roundf (xm * (cosf (val1[j]) - 1));
			f2 = roundf (xm * (cosf (val2[j]) - 1));

			v_browser->widgets[o1 + j].coords.x = x1 - f1 +
					((j % columns_current) * theme_gfx[THEME_GRID_APP_ENTRY]->w);
			v_browser->widgets[o2 + j].coords.x = x2 - f2 +
					((j % columns_new) * theme_gfx[THEME_GRID_APP_ENTRY]->w);
		}

		view_plot (v_browser, 0, NULL, NULL, NULL);

		if (i == TRANS_STEPS / 2) {
			widget_set_flag (&v_browser->widgets[0], WF_VISIBLE, true);
			widget_set_flag (&v_browser->widgets[1], WF_VISIBLE, true);
			widget_set_flag (&v_browser->widgets[0], WF_VISIBLE, less);
			widget_set_flag (&v_browser->widgets[1], WF_VISIBLE, more);

			view_set_focus (v_browser, focus);
		}
	}

	for (i = 0; i < (MAX_COLUMNS * ROWS); ++i)
		widget_free(&v_browser->widgets[o1 + i]);

	columns_current = columns_new;

	if (action == BA_REMOVE)
		top_offset = 0;
}

void browser_set_focus(u32 bd) {
	if (columns_current == 1) {
		if (bd & PADS_UP) {
			view_set_focus_prev(v_browser);
			return;
		}

		if (bd & PADS_DOWN) {
			view_set_focus_next(v_browser);
			return;
		}

		return;
	} else {
		if (bd & PADS_LEFT) {
			view_set_focus_prev(v_browser);
			return;
		}

		if (bd & PADS_RIGHT) {
			view_set_focus_next(v_browser);
			return;
		}

		if (bd & PADS_UP) {
			view_move_focus(v_browser, -columns_current);
			return;
		}

		if (bd & PADS_DOWN) {
			view_move_focus(v_browser, columns_current);
			return;
		}

		return;
	}
}

app_entry *browser_sel(void) {
	if ((entry_count < 1) || (v_browser->focus < AE_OFFSET))
		return NULL;

	u32 i;
	if (first_set)
		i = top_offset + v_browser->focus - AE_OFFSET;
	else
		i = top_offset + v_browser->focus - (MAX_COLUMNS * ROWS) - AE_OFFSET;

	return entries[i];
}

void browser_switch_mode(void) {
	const app_entry *app = browser_sel();
	int mode = 0;

	if (columns_current == 1) {
		if (widescreen)
			columns_new = 4;
		else
			columns_new = 3;

		mode = 1;
	} else {
		columns_new = 1;
	}

	if (v_browser)
		browser_gen_view(BA_REFRESH, app);

	if (settings.browse_mode != mode)
		settings.browse_mode = mode;
}

