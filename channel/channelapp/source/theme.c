#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <ogcsys.h>
#include <ogc/machine/processor.h>

#include "../config.h"
#include "tex.h"
#include "unzip.h"
#include "title.h"
#include "appentry.h"
#include "isfs.h"
#include "blob.h"
#include "panic.h"

#include "theme.h"

// view.c
#include "background_png.h"
#include "background_wide_png.h"
#include "logo_png.h"
// bubbles.c
#include "bubble1_png.h"
#include "bubble2_png.h"
#include "bubble3_png.h"
// browser.c
#include "apps_previous_png.h"
#include "apps_previous_hover_png.h"
#include "apps_next_png.h"
#include "apps_next_hover_png.h"
#include "icon_usbgecko_png.h"
#include "icon_usbgecko_active_png.h"
#include "icon_network_png.h"
#include "icon_network_active_png.h"
#include "throbber_png.h"
// dialogs.c
#include "about_png.h"
#include "dialog_background_png.h"
#include "dlg_info_png.h"
#include "dlg_confirm_png.h"
#include "dlg_warning_png.h"
#include "dlg_error_png.h"
// widgets.c
#include "button_png.h"
#include "button_focus_png.h"
#include "button_small_png.h"
#include "button_small_focus_png.h"
#include "button_tiny_png.h"
#include "button_tiny_focus_png.h"
#include "apps_list_png.h"
#include "apps_list_hover_png.h"
#include "apps_grid_png.h"
#include "apps_grid_hover_png.h"
#include "progress_png.h"
#include "content_arrow_up_png.h"
#include "content_arrow_down_png.h"

#define _ENTRY(n, w, h, fn) \
	{ \
		n##_png, &n##_png_size, w, h, \
		NULL, NULL, 0,  \
		fn \
	}

#define _ENTRY_WS(n, w, h, w_ws, fn) \
	{ \
		n##_png, &n##_png_size, w, h, \
		n##_wide_png, &n##_wide_png_size, w_ws, \
		fn \
	}

#define ENTRY(n, w, h) _ENTRY(n, w, h, #n)
#define ENTRY_RO(n, w, h) _ENTRY(n, w, h, NULL)
#define ENTRY_WS(n, w, h, w_ws) _ENTRY_WS(n, w, h, w_ws, #n)
#define ENTRY_WS_RO(n, w, h, w_ws) _ENTRY_WS(n, w, h, w_ws, NULL)

gfx_entity *theme_gfx[THEME_LAST];

theme_font theme_fonts[FONT_MAX];

const char *theme_fn_xml = "theme.xml";

static const struct {
	const u8 *data;
	const u32 *size;
	const u16 width;
	const u16 height;

	const u8 *data_ws;
	const u32 *size_ws;
	const u16 width_ws;

	const char *filename;
} theme_data[THEME_LAST] = {
	ENTRY_WS(background, 640, 480, 852),
	ENTRY_RO(logo, 320, 64),

	ENTRY(bubble1, 64, 64),
	ENTRY(bubble2, 64, 64),
	ENTRY(bubble3, 64, 64),

	ENTRY(apps_previous, 64, 64),
	ENTRY(apps_previous_hover, 64, 64),
	ENTRY(apps_next, 64, 64),
	ENTRY(apps_next_hover, 64, 64),
	ENTRY(icon_usbgecko, 32, 32),
	ENTRY(icon_usbgecko_active, 32, 32),
	ENTRY(icon_network, 32, 32),
	ENTRY(icon_network_active, 32, 32),
	ENTRY(throbber, 64, 64),

	ENTRY_RO(about, 236, 104),
	ENTRY(dialog_background, 520, 360),
	ENTRY(dlg_info, 32, 32),
	ENTRY(dlg_confirm, 32, 32),
	ENTRY(dlg_warning, 32, 32),
	ENTRY(dlg_error, 32, 32),

	ENTRY(button, 340, 48),
	ENTRY(button_focus, 340, 48),
	ENTRY(button_small, 200, 48),
	ENTRY(button_small_focus, 200, 48),
	ENTRY(button_tiny, 132, 48),
	ENTRY(button_tiny_focus, 132, 48),
	ENTRY(apps_list, 432, 64),
	ENTRY(apps_list_hover, 432, 64),
	ENTRY(apps_grid, 144, 64),
	ENTRY(apps_grid_hover, 144, 64),
	ENTRY(progress, 400, 112),
	ENTRY(content_arrow_up, 32, 8),
	ENTRY(content_arrow_down, 32, 8)
};

static bool theme_get_index(u32 *index, bool *ws, const char *filename) {
	u32 i;
	char buf[64];

	for (i = 0; i < THEME_LAST; ++i) {
		if (theme_data[i].filename) {
			strcpy(buf, theme_data[i].filename);
			strcat(buf, ".png");

			if (!strcasecmp(buf, filename)) {
				*index = i;
				*ws = false;
				return true;
			}

			if (theme_data[i].data_ws) {
				strcpy(buf, theme_data[i].filename);
				strcat(buf, "_wide.png");

				if (!strcasecmp(buf, filename)) {
					*index = i;
					*ws = true;
					return true;
				}
			}
		}
	}

	return false;
}

static void theme_load_fonts(unzFile uf) {
	int i,j,res;
	for (i=0; i<FONT_MAX; i++) {
		if (theme.fonts[i].color != NO_COLOR)
			theme_fonts[i].color = theme.fonts[i].color;
		else if (theme.default_font.color != NO_COLOR)
			theme_fonts[i].color = theme.default_font.color;

		if (theme.fonts[i].size)
			theme_fonts[i].size = theme.fonts[i].size;
		else if (theme.default_font.size)
			theme_fonts[i].size = theme.default_font.size;

		const char *file = NULL;
		if (theme.fonts[i].file)
			file = theme.fonts[i].file;
		else if (theme.default_font.file)
			file = theme.default_font.file;

		if (file) {
			// maybe we've already loaded this
			for (j=0; j<FONT_MAX; j++) {
				if (theme_fonts[j].file && !strcasecmp(theme_fonts[j].file,file) && theme_fonts[j].data) {
					theme_fonts[i].data = theme_fonts[j].data;
					theme_fonts[i].data_len = theme_fonts[j].data_len;
					break;
				}
			}

			if (!theme_fonts[i].data) {
				unz_file_info fi;
				gprintf("Loading font file %s\n", file);

				res = unzLocateFile(uf, file, 2);
				if (res != UNZ_OK) {
					gprintf("Could not locate font file %s\n", file);
					continue;
				}

				res = unzGetCurrentFileInfo(uf, &fi, NULL, 0, NULL, 0, NULL, 0);
				if (res != UNZ_OK) {
					gprintf("unzGetCurrentFileInfo failed: %d\n", res);
					continue;
				}
				if (fi.uncompressed_size == 0) {
					gprintf("Font file is empty\n");
					continue;
				}

				void *buf;
				res = unzOpenCurrentFile(uf);
				if (res != UNZ_OK) {
					gprintf("unzOpenCurrentFile failed: %d\n", res);
					continue;
				}

				buf = (u8 *) pmalloc(fi.uncompressed_size);

				res = unzReadCurrentFile(uf, buf, fi.uncompressed_size);
				if (res < 0) {
					gprintf("unzReadCurrentFile failed: %d\n", res);
					unzCloseCurrentFile(uf);
					free(buf);
					continue;
				}
				unzCloseCurrentFile(uf);

				theme_fonts[i].file = file;
				theme_fonts[i].data = buf;
				theme_fonts[i].data_len = fi.uncompressed_size;
			}
		}
	}
}

static void theme_load(u8 *data, u32 data_len) {
	unzFile uf;
	int res, i;
	unz_global_info gi;
	u8 *buf = NULL;

	uf = unzOpen(data, data_len);
	if (!uf) {
		gprintf("unzOpen failed\n");
		return;
	}

	res = unzGetGlobalInfo (uf, &gi);
	if (res != UNZ_OK) {
		gprintf("unzGetGlobalInfo failed: %d\n", res);
		unzClose(uf);
		return;
	}

	unz_file_info fi;
	char filename[256];
	u32 index;
	bool ws;

	for (i = 0; i < gi.number_entry; ++i) {
		res = unzGetCurrentFileInfo(uf, &fi, filename, sizeof(filename),
									NULL, 0, NULL, 0);
		if (res != UNZ_OK) {
			gprintf("unzGetCurrentFileInfo failed: %d\n", res);
			goto error;
		}

		if ((fi.uncompressed_size > 0) &&
				((!strcasecmp(filename, theme_fn_xml)) ||
				((theme_get_index(&index, &ws, filename)) &&
				(!theme_data[index].data_ws || (widescreen == ws))))) {
			res = unzOpenCurrentFile(uf);
			if (res != UNZ_OK) {
				gprintf("unzOpenCurrentFile failed: %d\n", res);
				goto error;
			}

			buf = (u8 *) pmalloc(fi.uncompressed_size + 1);

			res = unzReadCurrentFile(uf, buf, fi.uncompressed_size);
			if (res < 0) {
				gprintf("unzReadCurrentFile failed: %d\n", res);
				unzCloseCurrentFile(uf);
				free(buf);
				goto error;
			}

			if (!strcasecmp(filename, theme_fn_xml)) {
				gprintf("parsing theme.xml\n");
				buf[fi.uncompressed_size] = 0;
				load_theme_xml((char *) buf);
			} else {
				u16 w, h;

				gprintf("loading from theme: %s (%u @%u)\n", filename,
						(u32) fi.uncompressed_size, index);

				if (ws) {
					w = theme_data[index].width_ws;
					h = theme_data[index].height;
				} else {
					w = theme_data[index].width;
					h = theme_data[index].height;
				}

				theme_gfx[index] = tex_from_png(buf, fi.uncompressed_size, w, h);
			}

			free(buf);
		}

		if (i != gi.number_entry - 1) {
			res = unzGoToNextFile(uf);
			if (res != UNZ_OK) {
				gprintf("unzGoToNextFile failed: %d\n", res);
				goto error;
			}
		}
	}

	theme_load_fonts(uf);

error:
	res = unzClose(uf);
	if (res)
		gprintf("unzClose failed: %d\n", res);
}

void theme_init(u8 *data, u32 data_len) {
	s32 res;
	u32 i;
	const char *titlepath;
	STACK_ALIGN(char, fn, ISFS_MAXPATH, 32);

	memset(&theme, 0, sizeof(theme));

	for (i = 0; i < THEME_LAST; ++i)
		theme_gfx[i] = NULL;

	for (i = 0; i < FONT_MAX; ++i) {
		theme_fonts[i].file = NULL;
		theme_fonts[i].data = NULL;
		theme_fonts[i].data_len = 0;
		theme_fonts[i].size = 0;
		theme_fonts[i].color = 0xffffffff;
	}

	theme.progress.ul = 0xc8e1edff;
	theme.progress.ur = 0xc8e1edff;
	theme.progress.lr = 0x183848ff;
	theme.progress.ll = 0x183848ff;

	titlepath = title_get_path();
	if (titlepath[0])
		sprintf(fn, "%s/%s", titlepath, app_fn_theme);
	else
		fn[0] = 0;

	if (data) {
		theme_load(data, data_len);
		if (fn[0])
			isfs_put(fn, data, data_len);
	} else {
		res = -1;

		if (fn[0]) {
			res = isfs_get(fn, &data, 0, MAX_THEME_ZIP_SIZE, true);
			if (res > 0) {
				data_len = res;
				gprintf("theme loaded from nand (%u bytes)\n", data_len);
				theme_load(data, data_len);
			}
		}

		if (res <= 0) {
			data = NULL;
			data_len = 0;
			theme.description = pstrdup("Dark Water by drmr");
		}
	}

	gprintf("theme.description=%s\n", theme.description);
	gprintf("theme.progress ul=0x%08x\n", theme.progress.ul);
	gprintf("theme.progress ur=0x%08x\n", theme.progress.ur);
	gprintf("theme.progress lr=0x%08x\n", theme.progress.lr);
	gprintf("theme.progress ll=0x%08x\n", theme.progress.ll);
	gprintf("theme.langs=0x%x\n", theme.langs);

	for (i = 0; i < THEME_LAST; ++i) {
		if (!theme_gfx[i]) {
			if (data && theme_data[i].filename) {
				gprintf("%s unavailable, using default\n",
						theme_data[i].filename);
			}

			if (widescreen && theme_data[i].data_ws)
				theme_gfx[i] = tex_from_png(theme_data[i].data_ws,
											*(theme_data[i].size_ws),
											theme_data[i].width_ws,
											theme_data[i].height);
			else
				theme_gfx[i] = tex_from_png(theme_data[i].data,
											*(theme_data[i].size),
											theme_data[i].width,
											theme_data[i].height);
		}

		if (!theme_gfx[i])
			gprintf("WARNING: '%s' unavailable!\n", theme_data[i].filename);
	}

	if (data)
		blob_free(data);
}

void theme_deinit(void) {
	u32 i,j;

	if (theme.description) {
		free(theme.description);
		theme.description = NULL;
	}

	for (i = 0; i < FONT_MAX; ++i) {
		// data can be shared...
		if (theme_fonts[i].data) {
			for (j = 0; j < FONT_MAX; ++j) {
				if (i != j && theme_fonts[i].data == theme_fonts[j].data)
					theme_fonts[j].data = NULL;
			}
			free(theme_fonts[i].data);
			theme_fonts[i].data = NULL;
		}
		// filename is owned by xml.c, no need to free here
		memset(&theme_fonts[i], 0, sizeof(theme_fonts[i]));
	}

	for (i = 0; i < THEME_LAST; ++i) {
		tex_free(theme_gfx[i]);
		theme_gfx[i] = NULL;
	}
}

bool theme_is_valid_fn(const char *filename) {
	u32 index;
	bool ws;

	int l = strlen(filename);

	if (l >= 5 &&
		tolower((unsigned char)filename[l-1]) == 'f' &&
		tolower((unsigned char)filename[l-2]) == 't' &&
		tolower((unsigned char)filename[l-3]) == 't' &&
		filename[l-4] == '.')
		return true;

	// allow random txt files (e.g. README.txt)
	if (l >= 5 &&
		tolower((unsigned char)filename[l-1]) == 't' &&
		tolower((unsigned char)filename[l-2]) == 'x' &&
		tolower((unsigned char)filename[l-3]) == 't' &&
		filename[l-4] == '.')
		return true;

	return theme_get_index(&index, &ws, filename);
}

