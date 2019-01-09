#include <string.h>
#include <ogcsys.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "../config.h"
#include "panic.h"
#include "theme.h"
#include "font.h"

#include "droid_ttf.h"
#include "droidbold_ttf.h"

#define ROUNDUP4B(x) ((x + 4 - 1) & ~(4 - 1))

#define X_RATIO (widescreen?WIDESCREEN_RATIO:1.0)

//#define FONT_CHECKER

FT_Library library;

typedef struct {
	int codepoint;
	int next_idx;
	int glyph_index;
	int valid;
	gfx_entity *texture;
	int x, y;
	int w, h;
	FT_Pos dx, dy;
} font_glyph;

typedef struct {
	const void *data;
	u32 data_len;
	FT_Face face;
	int max_glyphs;
	int num_glyphs;
	int em_w, em_h;
	int em_height;
	int ascender;
	int descender;
	int height;
	font_glyph *glyphs;
} font_face;

static font_face *fonts[FONT_MAX];

typedef struct {
	int size;
	const void *data;
	const u32 *data_len; // bin2s stupidity
} font_default;

const font_default default_fonts[FONT_MAX] =
{
	[FONT_LABEL]		= { 16, droidbold_ttf, &droidbold_ttf_size},
	[FONT_DLGTITLE]		= { 20, droidbold_ttf, &droidbold_ttf_size},
	[FONT_MEMO]			= { 16, droid_ttf, &droid_ttf_size},
	[FONT_APPNAME]		= { 20, droidbold_ttf, &droidbold_ttf_size},
	[FONT_APPDESC]		= { 16, droidbold_ttf, &droidbold_ttf_size},
	[FONT_BUTTON]		= { 20, droidbold_ttf, &droidbold_ttf_size},
	[FONT_BUTTON_DESEL]	= { 20, droid_ttf, &droid_ttf_size},
};

#define GLYPH_CACHE_ROOT 64
#define GLYPH_CACHE_INIT 32
#define GLYPH_CACHE_GROW 32

font_glyph *font_get_char(font_id id, int codepoint);

#if 0
static const char *utf8(u32 codepoint)
{
	static char buf[5];
	if (codepoint < 0x80) {
		buf[0] = codepoint;
		buf[1] = 0;
	} else if (codepoint < 0x800) {
		buf[0] = 0xC0 | (codepoint>>6);
		buf[1] = 0x80 | (codepoint&0x3F);
		buf[2] = 0;
	} else if (codepoint < 0x10000) {
		buf[0] = 0xE0 | (codepoint>>12);
		buf[1] = 0x80 | ((codepoint>>6)&0x3F);
		buf[2] = 0x80 | (codepoint&0x3F);
		buf[3] = 0;
	} else if (codepoint < 0x110000) {
		buf[0] = 0xF0 | (codepoint>>18);
		buf[1] = 0x80 | ((codepoint>>12)&0x3F);
		buf[2] = 0x80 | ((codepoint>>6)&0x3F);
		buf[3] = 0x80 | (codepoint&0x3F);
		buf[4] = 0;
	} else {
		buf[0] = '?';
		buf[1] = 0;
	}
	return buf;
}
#endif

int font_load(font_id id, bool use_theme)
{
	int ret;
	int i;

	const void *data;
	u32 data_len;
	int em;

	font_face *font;

	if (fonts[id])
		return 0;

	gprintf("Loading font ID %d\n", id);

	data = default_fonts[id].data;
	data_len = *default_fonts[id].data_len;
	em = default_fonts[id].size;

	if (use_theme && theme_fonts[id].data) {
		data = theme_fonts[id].data;
		data_len = theme_fonts[id].data_len;
	}

	if (use_theme && theme_fonts[id].size)
		em = theme_fonts[id].size;

	// maybe we can reuse the entire font, look for other fonts
	for (i=0; i<FONT_MAX; i++) {
		if (fonts[i] && fonts[i]->data == data && fonts[i]->em_h == em) {
			gprintf("Cloned font object from font ID %d\n", i);
			fonts[id] = fonts[i];
			return 0;
		}
	}

	font = pmalloc(sizeof(*font));
	memset(font, 0, sizeof(*font));

	font->data = data;
	font->data_len = data_len;
	font->em_h = font->em_w = em;

	font->face = NULL;

	// maybe we can reuse the FT_Font object, look for other fonts
	for (i=0; i<FONT_MAX; i++) {
		if (fonts[i] && fonts[i]->data == font->data) {
			gprintf("Reused FreeType font subobject from font ID %d\n", i);
			font->face = fonts[i]->face;
			break;
		}
	}

	if (!font->face) {
		ret = FT_New_Memory_Face(library, font->data, font->data_len, 0, &font->face);
		if (ret != 0) {
			gprintf("Failed to load font\n");
			// try to fall back
			if (use_theme && theme_fonts[id].data) {
				free(font);
				theme_fonts[id].data = NULL;
				return font_load(id, false);
			}
			return ret;
		} else {
			gprintf("Loaded font at %p\n", font->data);
		}
	}
	gprintf("Font contains %ld faces\n", font->face->num_faces);
	gprintf("Face contains %ld glyphs\n", font->face->num_glyphs);

	if (widescreen)
		font->em_w = (int)(font->em_w / WIDESCREEN_RATIO + 0.5);

	ret = FT_Set_Pixel_Sizes(font->face, font->em_w, font->em_h);
	if (ret) {
		gprintf("FT_Set_Pixel_Sizes failed: %d\n", ret);
		free(font);
		return ret;
	}

	if (!FT_HAS_KERNING(font->face))
		gprintf("Font has no usable kerning data\n");
	else
		gprintf("Font has kerning data\n");

	font->max_glyphs = GLYPH_CACHE_ROOT+GLYPH_CACHE_INIT;
	font->num_glyphs = GLYPH_CACHE_ROOT; //base "hashtable" set
	font->glyphs = pmalloc(sizeof(font_glyph) * font->max_glyphs);
	memset(font->glyphs, 0, sizeof(font_glyph) * font->max_glyphs);
	for(i=0; i<font->max_glyphs; i++) {
		font->glyphs[i].codepoint = -1;
		font->glyphs[i].next_idx = -1;
	}

	fonts[id] = font;

	font_glyph *glyph = font_get_char(id, 'M');

	font->em_height = glyph->h; // height of capital M
	font->ascender = (font->face->size->metrics.ascender+32)>>6;
	font->descender = (font->face->size->metrics.descender+32)>>6;
	font->height = (font->face->size->metrics.height+32)>>6;

	gprintf("Ascender is %d\n", font->ascender);
	gprintf("Descender is %d\n", font->descender);
	gprintf("Height is %d\n", font->height);

	gprintf("Font ID %d loaded\n", id);
	return 0;
}

font_glyph *font_get_char(font_id id, int codepoint)
{
	int ret;
	font_glyph *glyph;

	font_load(id,1);
	font_face *font = fonts[id];

	glyph = &font->glyphs[codepoint&(GLYPH_CACHE_ROOT-1)];

	if (glyph->codepoint != -1) {
		while(1) {
			if (glyph->codepoint == codepoint) {
				//gprintf("FONT: Glyph %d (%s) is cached\n", codepoint, utf8(codepoint));
				return glyph;
			}
			if (glyph->next_idx == -1) {
				glyph->next_idx = font->num_glyphs;
				if (font->num_glyphs == font->max_glyphs) {
					font->glyphs = prealloc(font->glyphs, sizeof(font_glyph) * (font->max_glyphs + GLYPH_CACHE_GROW));
					memset(&font->glyphs[font->max_glyphs], 0, GLYPH_CACHE_GROW*sizeof(font_glyph));
					font->max_glyphs += GLYPH_CACHE_GROW;
					gprintf("FONT: expanded glyph cache size to %d\n", font->max_glyphs);
				}
				glyph = &font->glyphs[font->num_glyphs];
				font->num_glyphs++;
				break;
			}
			glyph = &font->glyphs[glyph->next_idx];
		}
	}

	memset(glyph, 0, sizeof(*glyph));
	glyph->codepoint = codepoint;
	glyph->next_idx = -1;
	glyph->valid = 0;
	glyph->texture = pmalloc(sizeof(*glyph->texture));
	glyph->dx = 0;
	glyph->dy = 0;

	FT_GlyphSlot slot = font->face->glyph;
	FT_UInt glyph_index;

	ret = FT_Set_Pixel_Sizes(font->face, font->em_w, font->em_h);
	if (ret)
		return glyph;

	glyph_index = FT_Get_Char_Index(font->face, codepoint);
	ret = FT_Load_Glyph(font->face, glyph_index, FT_LOAD_DEFAULT);
	if (ret)
		return glyph;

	ret = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	if (ret)
		return glyph;

	glyph->dx = slot->advance.x;
	glyph->dy = slot->advance.y;
	glyph->glyph_index = glyph_index;

	int cw = slot->bitmap.width;
	int ch = slot->bitmap.rows;

	glyph->w = cw;
	glyph->h = ch;

	if (!cw || !ch)
		return glyph;

	int tw = (cw+7)/8;
	int th = (ch+3)/4;

	int tpitch = tw * 32;

	u8 *pix = pmemalign(32, tw*th*32);
	memset(pix, 0, tw*th*32);

	int x,y;
	u8 *p = slot->bitmap.buffer;
	for(y=0; y<ch; y++) {
		u8 *lp = p;
		int ty = y/4;
		int py = y%4;
		u8 *lpix = pix + ty*tpitch + py*8;
		for(x=0; x<cw; x++) {
			int tx = x/8;
			int px = x%8;
#ifndef FONT_CHECKER
			lpix[32*tx + px] = *lp++;
#else
			if ((x+y)&1)
				lpix[32*tx + px] = 0xff;
			else
				lpix[32*tx + px] = 0;
#endif
		}
		p += slot->bitmap.pitch;
	}

	gfx_gen_tex (glyph->texture, 8*tw, 4*th, pix, GFXT_A8);

	glyph->x = slot->bitmap_left;
	glyph->y = slot->bitmap_top;
	glyph->valid = 1;

	//gprintf("FONT: Rendered and cached glyph %d (%s) at pos %d size %dx%d\n", codepoint, utf8(codepoint), glyph-font->glyphs, cw, ch);
	//gprintf("Free MEM1: %d MEM2: %d\n", SYS_GetArena1Size(), SYS_GetArena2Size());
	return glyph;
}

void font_kern(font_id id, int left, int right, FT_Pos *dx, FT_Pos *dy)
{
	FT_Vector delta;
	if (!FT_HAS_KERNING(fonts[id]->face)) {
		return;
	}
	if (FT_Get_Kerning(fonts[id]->face, left, right, FT_KERNING_UNFITTED, &delta))
		return;
	//gprintf("Kern font %d for glyphs %d,%d is %d,%d\n", id, left, right, delta.x, delta.y);
	*dx += delta.x;
	*dy += delta.y;
}


void font_init(void)
{
	int ret;

	memset(fonts, 0, sizeof(fonts));

	ret = FT_Init_FreeType(&library);
	if (ret != 0) {
		gprintf("FreeType init failed (%d)\n", ret);
		return;
	}
	gprintf("FreeType initialized\n");
}

void font_clear (void) {
	int id,id2;
	for (id=0; id<FONT_MAX; id++) {
		int g;
		if (!fonts[id])
			continue;

		// kill all clones first
		for (id2=0; id2<FONT_MAX; id2++) {
			if (id2 != id && fonts[id2] && fonts[id2] == fonts[id])
				fonts[id2] = NULL;
		}

		for (g=0; g<fonts[id]->num_glyphs; g++) {
			if (fonts[id]->glyphs[g].texture)
				free(fonts[id]->glyphs[g].texture);
			fonts[id]->glyphs[g].texture = NULL;
		}
		free(fonts[id]->glyphs);
		fonts[id]->glyphs = NULL;

		if (fonts[id]->face) {
			FT_Face face = fonts[id]->face;
			FT_Done_Face(face);
			// other fonts can share this face object, so nuke all
			for (id2=0; id2<FONT_MAX; id2++) {
				if (fonts[id2] && fonts[id2]->face == face)
					fonts[id2]->face = NULL;
			}
		}

		free(fonts[id]);
		fonts[id] = NULL;
	}
}

void font_deinit (void) {
	font_clear();
	FT_Done_FreeType(library);
}

static u32 utf8_get_char(const char **s)
{
	const char *c = *s;
	u32 mbc = '?';

	if (!c[0])
		return 0;

	if (c[0] <= 0x7f) {
		mbc = c[0];
		c++;
	} else if (c[0] >= 0xc0 && c[0] <= 0xdf) {
		if ((c[1]&0xc0) != 0x80) {
			c++;
			if (c[0])
				c++;
		} else {
			mbc = (c[1]&0x3f) | ((c[0]&0x1f)<<6);
			c+=2;
		}
	} else if (c[0] >= 0xe0 && c[0] <= 0xef) {
		if (((c[1]&0xc0) != 0x80) || ((c[2]&0xc0) != 0x80)) {
			c++;
			if (c[0])
				c++;
			if (c[0])
				c++;
		} else {
			mbc = (c[2]&0x3f) | ((c[1]&0x3f)<<6) | ((c[0]&0x1f)<<12);
			c+=3;
		}
	} else if (c[0] >= 0xf0 && c[0] <= 0xf7) {
		if (((c[1]&0xc0) != 0x80) || ((c[2]&0xc0) != 0x80) || ((c[3]&0xc0) != 0x80)) {
			c++;
			if (c[0])
				c++;
			if (c[0])
				c++;
			if (c[0])
				c++;
		} else {
			mbc = (c[3]&0x3f) | ((c[2]&0x3f)<<6) | ((c[1]&0x3f)<<12) | ((c[0]&0x1f)<<18);
			c+=4;
		}
	} else {
		c++;
	}
	*s = c;
	return mbc;
}

int font_get_ascender(font_id id) {
	font_load(id,1);

	return fonts[id]->ascender;
}

int font_get_height(font_id id) {
	font_load(id,1);

	return fonts[id]->ascender - fonts[id]->descender;
}

int font_get_em_height(font_id id) {
	font_load(id,1);

	return fonts[id]->em_height;
}

int font_get_y_spacing(font_id id) {
	font_load(id,1);

	return fonts[id]->height;
}

int font_get_min_y(font_id id) {
	font_load(id,1);

	return -fonts[id]->descender;
}

u16 font_get_string_width (font_id id, const char *s, int count) {
	int i = 0;
	u32 mbc;
	int cx = 0;
	int cy = 0;

	FT_Pos cdx = 0, cdy = 0;
	u32 previous = 0;

	font_load(id,1);

	while((mbc = utf8_get_char(&s))) {
		if (mbc == '\n') {
			if (*s)
				mbc = ' ';
			else
				continue;
		}
		if (mbc == '\r')
			continue;
		font_glyph *glyph = font_get_char(id, mbc);

		if (previous)
			font_kern(id, previous, glyph->glyph_index, &cdx, &cdy);

		cx += (cdx+32) >> 6;
		cy += (cdy+32) >> 6;

		cdx = glyph->dx;
		cdy = glyph->dy;
		previous = glyph->glyph_index;
		i++;
		if (i == count)
			break;
	}

	cx += (cdx+32) >> 6;
	cy += (cdy+32) >> 6;

	return cx;
}

int font_get_char_count (font_id id, const char *s, u16 max_width) {
	//u16 res = 0;
	int i = 0;
	u32 mbc;
	int cx = 0;
	int cy = 0;

	FT_Pos cdx = 0, cdy = 0;
	u32 previous = 0;

	max_width /= X_RATIO;

	font_load(id,1);

	while((mbc = utf8_get_char(&s))) {
		if (mbc == '\n') {
			if (*s)
				mbc = ' ';
			else
				continue;
		}
		if (mbc == '\r')
			continue;
		font_glyph *glyph = font_get_char(id, mbc);

		if (previous)
			font_kern(id, previous, glyph->glyph_index, &cdx, &cdy);

		cx += (cdx+32) >> 6;
		cy += (cdy+32) >> 6;

		if (max_width && (cx >= max_width))
			return i;

		cdx = glyph->dx;
		cdy = glyph->dy;
		previous = glyph->glyph_index;
		i++;
	}

	return i;
}

int font_wrap_string (char ***lines, font_id id, const char *s,
						u16 max_width) {
	const char *p = s;
	char **res = NULL;
	int line = 0;
	int start = 0;
	int end = 0;
	int ls = -1;
	bool lb;

	int cx = 0;
	int cy = 0;

	int i = 0;
	u32 mbc;

	FT_Pos cdx = 0, cdy = 0;
	u32 previous = 0;

	max_width /= X_RATIO;

	font_load(id,1);

	while (true) {
		i = p - s;
		mbc = utf8_get_char(&p);
		lb = false;

		if (mbc == ' ')
			ls = p - s;

		if ((mbc == '\n') || mbc == 0) {
			lb = true;
			end = i;
			i = p - s;
		} else if (mbc == '\r') {
			continue;
		} else {
			font_glyph *glyph = font_get_char(id, mbc);

			if (previous)
				font_kern(id, previous, glyph->glyph_index, &cdx, &cdy);

			cx += (cdx+32) >> 6;
			cy += (cdy+32) >> 6;

			int w = (glyph->dx+32) >> 6;
			if ((glyph->w + glyph->x) > w)
				w = glyph->w + glyph->x;

			if ((cx + w) >= max_width) {
				lb = true;
				if (ls <= start) {
					if (i == start)
						i++;
					end = i;
					p = &s[i];
				} else {
					end = ls;
					i = ls;
					p = &s[i];
				}
			}

			cdx = glyph->dx;
			cdy = glyph->dy;
			previous = glyph->glyph_index;
		}

		if (lb) {
			res = prealloc (res, (line + 1) * sizeof (char **));
			if (end <= start)
				res[line] = NULL;
			else {
				res[line] = strndup (&s[start], end - start);
			}

			line++;
			start = i;
			cx = 0;
			cy = 0;
			cdx = 0;
			cdy = 0;
			previous = 0;
		}

		if (mbc == 0)
			break;
	}

	*lines = res;
	return line;
}

void font_free_lines (char **lines, u32 count) {
	int i;

	for (i = 0; i < count; ++i)
		free (lines[i]);

	free (lines);
}

void font_plot_string (gfx_queue_entry *entries, int count, font_id id,
					   const char *s, u16 x, u16 y, u16 layer, u16 width,
					   font_xalign xalign, font_yalign yalign) {
	int cx;
	int cy;


	if (!count)
		return;

	cx = x;
	cy = y;

	cx /= X_RATIO;

	switch (xalign) {
	case FA_LEFT:
		break;

	case FA_CENTERED:
		cx += (width/X_RATIO - font_get_string_width (id, s, count)) / 2;
		break;

	case FA_RIGHT:
		cx += width/X_RATIO - font_get_string_width (id, s, count);
		break;
	}

	switch (yalign) {
	case FA_ASCENDER:
		cy += fonts[id]->ascender;
		break;
	case FA_EM_TOP:
		cy += fonts[id]->em_height;
		break;
	case FA_EM_CENTERED:
		cy += fonts[id]->em_height/2;
		break;
	case FA_BASELINE:
		break;
	case FA_DESCENDER:
		cy += fonts[id]->descender;
		break;
	}

	FT_Pos cdx = 0, cdy = 0;
	u32 previous = 0;
	int first = 1;
	u32 mbc;

	while ((mbc = utf8_get_char(&s))) {
		if (mbc == '\n') {
			if (*s)
				mbc = ' ';
			else
				break;
		}
		if (mbc == '\r')
			continue;
		font_glyph *glyph = font_get_char(id, mbc);

		if (previous)
			font_kern(id, previous, glyph->glyph_index, &cdx, &cdy);

		cx += (cdx+32) >> 6;
		cy += (cdy+32) >> 6;

		if (!glyph->valid) {
			entries->type = GFXQ_NULL;
			goto next_glyph;
		}

		// fudgity fudge, helps make text left-align slightly better/nicer,
		// especially with differing fonts.
		// do this only for chars with glyph->x > 0. Those with glyph->x < 0
		// are usually kerned together and look better a bit to the left anyway.
		if(xalign == FA_LEFT && first && glyph->x > 0)
			cx -= glyph->x;

		gfx_qe_entity (entries, glyph->texture,
			((float)(cx + glyph->x)) * X_RATIO,
			cy - glyph->y,
			layer, theme_fonts[id].color);

		//gprintf("Render %d (%s) at %d %d -> %d %d size %d %d\n", mbc, utf8(mbc), cx, cy, cx + glyph->x, cy - glyph->y + fonts[id]->height, glyph->texture->w, glyph->texture->h);

		first = 0;

next_glyph:
		count--;
		entries++;

		if (!count)
			break;

		cdx = glyph->dx;
		cdy = glyph->dy;
		previous = glyph->glyph_index;
	}

	if (count) {
		gprintf("BUG: %d queue entries empty, padding with NULLs\n", count);
		while (count--) {
			entries->type = GFXQ_NULL;
			// this superfluous statement carefully chosen for optimal crashiness
			entries++->entity.coords.z = 0;
		}
	}
}

