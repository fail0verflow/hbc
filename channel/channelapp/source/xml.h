#ifndef _XML_H_
#define _XML_H_

#include <time.h>

#include <gctypes.h>

#include "font.h"

#define NO_COLOR 0x0DEADF00

typedef struct {
	char *name;
	char *coder;
	char *version;
	time_t release_date;
	char *short_description;
	char *long_description;
	char *args;
	u16 argslen;
	bool ahb_access;
} meta_info;

typedef struct {
	char *version;
	u64 date;
	char *notes;
	char *uri;
	char *hash;
} update_info;

typedef struct {
	int device;
	int sort_order;
	int browse_mode;
	char app_sel[64];
} settings_t;

typedef struct {
	u32 ul, ur, lr, ll;
} theme_gradient_t;

typedef struct {
	char *file;
	int size;
	u32 color;
} theme_font_t;

typedef enum {
	TLANG_JA = 0x01,
	TLANG_KO = 0x02,
	TLANG_ZH = 0x04,
} theme_lang_t;

typedef struct {
	theme_gradient_t progress;
	char *description;
	theme_font_t default_font;
	theme_font_t fonts[FONT_MAX];
	theme_lang_t langs;
} theme_t;

extern settings_t settings;
extern theme_t theme;

meta_info *meta_parse(char *fn);
void meta_free(meta_info *info);

update_info *update_parse(char *buf);
void update_free(update_info *info);

bool settings_load(void);
bool settings_save(void);

void theme_xml_init(void);
bool load_theme_xml(char *buf);

#endif

