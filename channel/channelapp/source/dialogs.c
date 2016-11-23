#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <math.h>

#include <ogcsys.h>

#include "../config.h"
#include "controls.h"
#include "theme.h"
#include "font.h"
#include "widgets.h"
#include "view.h"
#include "xml.h"
#include "panic.h"

#include "dialogs.h"

#include "i18n.h"

#define TRANS_STEPS 15

static const char *app_entry_desc_default;

static const char *caption_info;
static const char *caption_confirm;
static const char *caption_warning;
static const char *caption_error;
static const char *caption_ok;
static const char *caption_cancel;
static const char *caption_yes;
static const char *caption_no;
static const char *caption_delete;
static const char *caption_load;
static const char *caption_back;
static const char *caption_options;
static const char *caption_device;
static const char *caption_device_names[DEVICE_COUNT];
static const char *caption_sort_by;
static const char *caption_sort_name;
static const char *caption_sort_date;

static const char *l_version;
static const char *l_coder;

static const char *string_about_pre;
static const char *string_about_post;
static const char *string_about_trans;
static const char *string_about_theme;
static char *string_about_gen;

void dialogs_theme_reinit (void) {
	app_entry_desc_default = _("no description available");

	caption_info = _("Information");
	caption_confirm = _("Confirmation");
	caption_warning = _("Warning");
	caption_error = _("Error");
	caption_ok = _("Ok");
	caption_cancel = _("Cancel");
	caption_yes = _("Yes");
	caption_no = _("No");
	caption_delete = _("Delete");
	caption_load = _("Load");
	caption_back = _("Back");
	caption_options = _("Options");
	caption_device = _("Device:");
	caption_device_names[0] = _("Internal SD Slot");
	caption_device_names[1] = _("USB device");
	caption_device_names[2] = _("SDGecko Slot A");
	caption_device_names[3] = _("SDGecko Slot B");
	caption_sort_by = _("Sort applications by:");
	caption_sort_name = _("Name");
	caption_sort_date = _("Date");

	string_about_pre =
		"Credits\n\n"

		"The Homebrew Channel was made possible by the following people:\n\n"

		"dhewg (EWRONGCHAN) - application code, geckoloader code\n"
		"blasty (ESTONED) - application code\n"
		"marcan (EFAILURE) - reload stub, banner, installer, packaging\n"
		"bushing (EWANTPONY) - socket code, loader code\n"
		"segher (EBUGFOUND) - nandloader stub code\n"
		"souLLy (ENOTHERE) - banner graphics\n"
		"drmr (EMORECOWBELL) - banner audio, channel graphics\n"
		"mha (E404) - update server and hosting";

	string_about_post =
		"Powered by devkitPPC and libogc, by shagkur, WinterMute, "
		"and everyone else who contributed\n\n"

		"Thanks to all the beta testers\n\n"

		"Kind regards to the following people too:\n\n"

		"sepp256 - dropped some good GX hints\n"
		"chishm, svpe, rodries, hermes - libfat port\n"
		"alien - some graphics\n"
		"jodi - the lulz\n\n"

		"And last but not least, thanks to the authors of the following libraries:\n\n"

		"wiiuse - para's Wiimote library, now integrated with libogc\n"
		"libpng - the official PNG library\n"
		"Mini-XML - small and efficient XML parsing library\n"
		"FreeType - the free TrueType/OpenType font renderer\n";

	string_about_trans = _("<YourLanguageHere> translation by <YourNickNameHere>");

	if (!i18n_have_mo())
		string_about_trans = "";

	string_about_theme = _("Theme:");

	if (string_about_gen)
		free(string_about_gen);

	string_about_gen = pmalloc(strlen(string_about_pre) +
								strlen(string_about_post) +
								strlen(string_about_trans) +
								strlen(string_about_theme) + 128);

	l_version = _("Version: %s");
	l_coder = _("Author: %s");
}

void dialogs_init (void) {
	string_about_gen = NULL;
	dialogs_theme_reinit();
}

void dialogs_deinit (void) {
	free(string_about_gen);
}

void dialog_fade (view *v, bool fade_in) {
	float val;
	float step;
	s16 y;
	float yf;
	u32 c;
	u8 stepa;
	u8 i;

	if (fade_in) {
		val = 0;
		step = M_PI / (2 * TRANS_STEPS);
		y = v->coords.y + view_height;
		c = DIALOG_MASK_COLOR & 0xffffff00;
		stepa = (DIALOG_MASK_COLOR & 0xff) / TRANS_STEPS;
	} else {
		val = M_PI;
		step = M_PI / (2 * TRANS_STEPS);
		y = v->coords.y;
		c = DIALOG_MASK_COLOR;
		stepa = -(DIALOG_MASK_COLOR & 0xff) / TRANS_STEPS;
	}

	yf = view_height;

	for (i = 0; i < TRANS_STEPS; ++i) {
		v->coords.y = y - roundf (yf * sinf (val));
		val += step;
		c += stepa;

		view_plot (v, c, NULL, NULL, NULL);
	}
}

view * dialog_app (const app_entry *entry, const view *sub_view) {
	view *v;
	u16 x, gap;
	char *name;
	char coder[64];
	char version[64];
	const char *desc;
	u16 ym, hm, yb;

	if (entry->meta && entry->meta->name)
		name = entry->meta->name;
	else
		name = entry->dirname;

	if (entry->meta && entry->meta->coder)
		snprintf (coder, sizeof (coder), l_coder, entry->meta->coder);
	else
		*coder = 0;

	if (entry->meta && entry->meta->version)
		snprintf (version, sizeof (version), l_version, entry->meta->version);
	else
		*version = 0;

	if (entry->meta && entry->meta->long_description)
		desc = entry->meta->long_description;
	else
		desc = app_entry_desc_default;

	v = view_new (11, sub_view, (view_width - theme_gfx[THEME_DIALOG]->w) / 2,
					44, TEX_LAYER_DIALOGS, PADS_B);

	widget_image(&v->widgets[0], 0, 0, 0, theme_gfx[THEME_DIALOG],
					NULL, false, NULL);

	widget_label (&v->widgets[1], 32, 16, 1, name,
				  theme_gfx[THEME_DIALOG]->w - 64, FA_CENTERED, FA_ASCENDER,
				  FONT_DLGTITLE);

	if (entry->icon)
		widget_image(&v->widgets[2], 32, 48, 1, entry->icon, NULL, false, NULL);

	widget_label (&v->widgets[3], 48 + APP_ENTRY_ICON_WIDTH, 72, 1, version,
					theme_gfx[THEME_DIALOG]->w - 72 - APP_ENTRY_ICON_WIDTH,
					FA_LEFT, FA_DESCENDER, FONT_LABEL);

	widget_label (&v->widgets[4], 48 + APP_ENTRY_ICON_WIDTH, 72, 1, coder,
					theme_gfx[THEME_DIALOG]->w - 72 - APP_ENTRY_ICON_WIDTH,
					FA_LEFT, FA_ASCENDER, FONT_LABEL);

	yb = theme_gfx[THEME_DIALOG]->h - theme_gfx[THEME_BUTTON_TINY]->h - 16;
	ym = 48 + APP_ENTRY_ICON_HEIGHT + 8;
	hm = yb - ym - 8;

	widget_memo_deco (&v->widgets[5], 32, ym, 1,
						theme_gfx[THEME_DIALOG]->w - 64, hm, desc, FA_LEFT);

	gap = (theme_gfx[THEME_DIALOG]->w -
			theme_gfx[THEME_BUTTON_TINY]->w * 3) / 4;

	x = gap;
	widget_button (&v->widgets[8], x, yb, 1, BTN_TINY, caption_delete);

	x += gap + theme_gfx[THEME_BUTTON_TINY]->w;
	widget_button (&v->widgets[9], x, yb, 1, BTN_TINY, caption_load);

	x += gap + theme_gfx[THEME_BUTTON_TINY]->w;
	widget_button (&v->widgets[10], x, yb, 1, BTN_TINY, caption_back);

	view_set_focus (v, 10);

	return v;
}

view * dialog_progress (const view *sub_view, const char *caption, u32 max) {
	view *v;

	v = view_new (1, sub_view, (view_width - theme_gfx[THEME_PROGRESS]->w) / 2,
					(view_height - theme_gfx[THEME_PROGRESS]->h) / 2,
					TEX_LAYER_DIALOGS, 0);

	widget_progress (&v->widgets[0], 0, 0, 0, caption, max);
	widget_set_progress (&v->widgets[0], 0);

	return v;
}

void dialog_set_progress (const view *v, u32 progress) {
	widget_set_progress (&v->widgets[0], progress);
}

view * dialog_about (const view *sub_view) {
	view *v;
	u16 ym, hm, yb, hmn;
	u8 l, hf;

	strcpy(string_about_gen, string_about_pre);
	if (string_about_trans && strlen(string_about_trans)) {
		strcat(string_about_gen, "\n\n");
		strcat(string_about_gen, string_about_trans);
	}

	if (theme.description && strlen(theme.description)) {
		strcat(string_about_gen, "\n\n");
		strcat(string_about_gen, string_about_theme);
		strcat(string_about_gen, " ");
		strcat(string_about_gen, theme.description);
	}

	strcat(string_about_gen, "\n\n");
	strcat(string_about_gen, string_about_post);

	v = view_new (3, sub_view, (view_width - theme_gfx[THEME_DIALOG]->w) / 2,
					44, TEX_LAYER_DIALOGS, 0);

	widget_image (&v->widgets[0], 0, 0, 0, theme_gfx[THEME_DIALOG],
					NULL, false, NULL);

	widget_image (&v->widgets[1], (theme_gfx[THEME_DIALOG]->w -
					theme_gfx[THEME_ABOUT]->w) / 2, 16, 1,
					theme_gfx[THEME_ABOUT], NULL, false, NULL);

	yb = theme_gfx[THEME_DIALOG]->h - 16;
	ym = 16 + theme_gfx[THEME_ABOUT]->h + 8;
	hm = yb - ym - 8;
	hf = font_get_y_spacing(FONT_MEMO);
	l = hm / hf;
	hmn = l * hf;

	if (hmn < hm) {
		ym += (hm - hmn) / 2;
		hm = hmn;
	}

	widget_memo (&v->widgets[2], 32, ym, 1, theme_gfx[THEME_DIALOG]->w - 64,
					hm, string_about_gen, FA_CENTERED);

	v->widgets[2].cur = CUR_STD;

	return v;
}

static view *dialog_message(const view *sub_view, dialog_message_type type,
							dialog_message_buttons buttons, const char *text,
							u8 focus) {
	view *v;
	u8 c;
	u16 x, gap;
	const char *caption = NULL, *b1 = NULL, *b2 = NULL;
	u16 ym, hm, yb;
	u8 hf;
	gfx_entity *icon = NULL;

	c = 6;
	switch (buttons) {
	case DLGB_OK:
		c++;
		break;

	case DLGB_OKCANCEL:
	case DLGB_YESNO:
		c += 2;
		break;
	}

	v = view_new (c, sub_view, (view_width - theme_gfx[THEME_DIALOG]->w) / 2,
					44, TEX_LAYER_DIALOGS, PADS_B);

	widget_image (&v->widgets[0], 0, 0, 0, theme_gfx[THEME_DIALOG],
					NULL, false, NULL);

	switch (type) {
	case DLGMT_INFO:
		caption = caption_info;
		icon = theme_gfx[THEME_DLG_INFO];
		break;

	case DLGMT_CONFIRM:
		caption = caption_confirm;
		icon = theme_gfx[THEME_DLG_CONFIRM];
		break;

	case DLGMT_WARNING:
		caption = caption_warning;
		icon = theme_gfx[THEME_DLG_WARNING];
		break;

	case DLGMT_ERROR:
		caption = caption_error;
		icon = theme_gfx[THEME_DLG_ERROR];
		break;
	}

	widget_image (&v->widgets[1], 128, 24, 0, icon, NULL, false, NULL);
	widget_label (&v->widgets[2], 32, 32, 1, caption,
				  theme_gfx[THEME_DIALOG]->w - 64, FA_CENTERED, FA_ASCENDER,
				  FONT_DLGTITLE);

	hf = font_get_height (FONT_DLGTITLE);
	yb = theme_gfx[THEME_DIALOG]->h - theme_gfx[THEME_BUTTON_SMALL]->h - 32;
	ym = 32 + hf + 8;
	hm = yb - ym - 8;

	widget_memo_deco (&v->widgets[3], 32, ym, 1,
						theme_gfx[THEME_DIALOG]->w - 64, hm, text, FA_CENTERED);

	switch (buttons) {
	case DLGB_OK:
		b1 = caption_ok;
		b2 = NULL;
		break;

	case DLGB_OKCANCEL:
		b1 = caption_ok;
		b2 = caption_cancel;
		break;

	case DLGB_YESNO:
		b1 = caption_yes;
		b2 = caption_no;
		break;
	}

	if (b2) {
		gap = (theme_gfx[THEME_DIALOG]->w -
				theme_gfx[THEME_BUTTON_SMALL]->w * 2) / 3;

		x = gap;
		widget_button (&v->widgets[6], x, yb, 1, BTN_SMALL, b1);

		x += gap + theme_gfx[THEME_BUTTON_SMALL]->w;
		widget_button (&v->widgets[7], x, yb, 1, BTN_SMALL, b2);
	} else {
		gap = (theme_gfx[THEME_DIALOG]->w -
				theme_gfx[THEME_BUTTON_SMALL]->w) / 2;

		x = gap;
		widget_button (&v->widgets[6], x, yb, 1, BTN_SMALL, b1);
	}

	view_set_focus (v, 6 + focus);

	return v;
}

s8 show_message (const view *sub_view, dialog_message_type type,
					dialog_message_buttons buttons, const char *text, u8 focus) {
	view *v;
	u8 fhw;
	u32 bd;
	s8 res;
	s16 mm;

	v = dialog_message (sub_view, type, buttons, text, focus);

	dialog_fade (v, true);

	fhw = font_get_y_spacing(FONT_MEMO);

	while (true) {
		view_plot (v, DIALOG_MASK_COLOR, &bd, NULL, NULL);

		if (bd & PADS_LEFT)
			view_set_focus_prev (v);

		if (bd & PADS_RIGHT)
			view_set_focus_next (v);

		mm = 0;
		if (bd & PADS_UP)
			mm += fhw;

		if (bd & PADS_DOWN)
			mm -= fhw;

		mm += controls_sticky() / 8;

		if (v->drag && (v->drag_widget == 4))
			mm += -v->drag_y / 32;

		widget_scroll_memo_deco (&v->widgets[3], mm);

		if ((bd & PADS_A) && (v->focus != -1))
			break;
	}

	res = v->focus - 6;

	dialog_fade (v, false);

	view_free (v);

	return res;
}

#define DLG_DEV_FIRST 4

dialog_options_result show_options_dialog(const view *sub_view) {
	u32 frame = 0;
	view *v;
	dialog_options_result ret;
	int device;
	app_sort sort;
	bool status[DEVICE_COUNT];
	u32 i, bd;

	app_entry_poll_status(true);

	v = view_new (12, sub_view, (view_width - theme_gfx[THEME_DIALOG]->w) / 2,
					44, TEX_LAYER_DIALOGS, PADS_B);

	widget_image (&v->widgets[0], 0, 0, 0, theme_gfx[THEME_DIALOG],
					NULL, false, NULL);
	widget_label (&v->widgets[1], 32, 16, 1, caption_options,
					theme_gfx[THEME_DIALOG]->w - 64, FA_CENTERED, FA_ASCENDER, FONT_DLGTITLE);

	widget_label (&v->widgets[2], 32, 60, 1, caption_device,
					theme_gfx[THEME_DIALOG]->w - 64, FA_LEFT, FA_DESCENDER, FONT_LABEL);
	widget_label (&v->widgets[3], 32, 212, 1, caption_sort_by,
					theme_gfx[THEME_DIALOG]->w - 64, FA_LEFT, FA_DESCENDER, FONT_LABEL);

	widget_button (&v->widgets[4], 52, 64, 1, BTN_SMALL, NULL);
	widget_button (&v->widgets[5], 268, 64, 1, BTN_SMALL, NULL);
	widget_button (&v->widgets[6], 52, 128, 1, BTN_SMALL, NULL);
	widget_button (&v->widgets[7], 268, 128, 1, BTN_SMALL, NULL);

	widget_button (&v->widgets[8], 52, 216, 1, BTN_SMALL, NULL);
	widget_button (&v->widgets[9], 268, 216, 1, BTN_SMALL, NULL);

	widget_button (&v->widgets[10], 32,
					theme_gfx[THEME_DIALOG]->h -
					theme_gfx[THEME_BUTTON_SMALL]->h - 16 , 1, BTN_SMALL,
					caption_ok);
	widget_button (&v->widgets[11], theme_gfx[THEME_DIALOG]->w -
					theme_gfx[THEME_BUTTON_SMALL]->w - 32,
					theme_gfx[THEME_DIALOG]->h -
					theme_gfx[THEME_BUTTON_SMALL]->h - 16 , 1, BTN_SMALL,
					caption_back);

	device = app_entry_get_status(status);
	sort = app_entry_get_sort();

	ret.confirmed = false;
	ret.device = device;
	ret.sort = sort;

	for (i = 0; i < DEVICE_COUNT; ++i) {
		if (i == device)
			widget_button_set_caption(&v->widgets[DLG_DEV_FIRST + i],
										FONT_BUTTON,
										caption_device_names[i]);
		else
			widget_button_set_caption(&v->widgets[DLG_DEV_FIRST + i],
										FONT_BUTTON_DESEL,
										caption_device_names[i]);

		widget_set_flag (&v->widgets[DLG_DEV_FIRST + i], WF_ENABLED, status[i]);
	}

	if (ret.sort == APP_SORT_DATE) {
		widget_button_set_caption(&v->widgets[8],
									FONT_BUTTON_DESEL,
									caption_sort_name);
		widget_button_set_caption(&v->widgets[9],
									FONT_BUTTON,
									caption_sort_date);
	} else {
		widget_button_set_caption(&v->widgets[8],
									FONT_BUTTON,
									caption_sort_name);
		widget_button_set_caption(&v->widgets[9],
									FONT_BUTTON_DESEL,
									caption_sort_date);
	}

	view_set_focus (v, 11);

	dialog_fade (v, true);

	while (true) {
		app_entry_get_status(status);

		for (i = 0; i < DEVICE_COUNT; ++i)
			widget_set_flag (&v->widgets[DLG_DEV_FIRST + i], WF_ENABLED,
								status[i]);

		view_plot (v, DIALOG_MASK_COLOR, &bd, NULL, NULL);
		frame++;

		if (bd & PADS_LEFT)
			view_set_focus_prev (v);

		if (bd & PADS_RIGHT)
			view_set_focus_next (v);

		if (bd & PADS_UP)
			if (v->focus == view_move_focus(v, -2))
				view_move_focus(v, -4);

		if (bd & PADS_DOWN)
			if (v->focus == view_move_focus(v, 2))
				view_move_focus(v, 4);

		if (bd & (PADS_B | PADS_1))
			break;

		if ((bd & PADS_A) && (v->focus != -1)) {
			if ((v->focus >= DLG_DEV_FIRST) &&
					(v->focus < DLG_DEV_FIRST + DEVICE_COUNT)) {
				widget_button_set_caption(&v->widgets[DLG_DEV_FIRST + ret.device],
											FONT_BUTTON_DESEL,
											caption_device_names[ret.device]);
				ret.device = v->focus - DLG_DEV_FIRST;
				widget_button_set_caption(&v->widgets[DLG_DEV_FIRST + ret.device],
											FONT_BUTTON,
											caption_device_names[ret.device]);
			} else if (v->focus == 8) {
				ret.sort = APP_SORT_NAME;
				widget_button_set_caption(&v->widgets[8],
											FONT_BUTTON,
											caption_sort_name);
				widget_button_set_caption(&v->widgets[9],
											FONT_BUTTON_DESEL,
											caption_sort_date);
			} else if (v->focus == 9) {
				ret.sort = APP_SORT_DATE;
				widget_button_set_caption(&v->widgets[8],
											FONT_BUTTON_DESEL,
											caption_sort_name);
				widget_button_set_caption(&v->widgets[9],
											FONT_BUTTON,
											caption_sort_date);
			} else if ((v->focus == 10) || (v->focus == 11)) {
				break;
			}
		}

		if ((frame % 30) == 0)
			app_entry_poll_status(false);
	}

	if ((bd & PADS_A) && (v->focus == 10))
		ret.confirmed = true;

	dialog_fade (v, false);

	view_free (v);

	return ret;
}


