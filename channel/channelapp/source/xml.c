#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <mxml.h>

#include "../config.h"
#include "title.h"
#include "isfs.h"
#include "xml.h"
#include "panic.h"

#include "banner_bin.h"

#define FN_BANNER "banner.bin"
#define FN_SETTINGS "settings.xml"
#define MAX_SETTINGS_XML_SIZE (16 * 1024)

settings_t settings;
theme_t theme;

static settings_t _loaded_settings;
static bool write_banner = true;

static inline char *_xmldup(const char *str) {
	if (!str)
		return NULL;

	return pstrdup(str);
}

static char *_get_cdata(mxml_node_t *node) {
	if (!node)
		return NULL;

	mxml_node_t *n = node->child;

	while (n) {
		if (n->type == MXML_OPAQUE)
			return n->value.opaque;

		n = mxmlWalkNext(n, node, MXML_NO_DESCEND);
	}

	return NULL;
}

static char *_get_elem_cdata(mxml_node_t *node, const char *element) {
	if (!node)
		return NULL;

	return _get_cdata(mxmlFindElement(node, node, element,
										NULL, NULL, MXML_DESCEND_FIRST));
}

static int _get_elem_int(mxml_node_t *node, const char *element, int std) {
	char *cdata;

	if (!node)
		return std;

	cdata = _get_cdata(mxmlFindElement(node, node, element,
										NULL, NULL, MXML_DESCEND_FIRST));

	if (!cdata)
		return std;

	return atoi(cdata);
}

static bool _get_color(mxml_node_t *node, const char *element, u32 *color) {
	u32 res, i;
	int c;
	const char *const channel[] = { "red", "green", "blue", "alpha" };

	if (!node)
		return false;

	node = mxmlFindElement(node, node, element, NULL, NULL, MXML_DESCEND_FIRST);

	if (!node)
		return false;

	res = 0;
	for (i = 0; i < 4; ++i) {
		res <<= 8;
		c = _get_elem_int(node, channel[i], -1);
		if ((c < 0) || (c > 255))
			return false;
		res |= c & 0xff;
	}

	*color = res;
	return true;
}

static bool _get_gradient(mxml_node_t *node, const char *element,
							theme_gradient_t *gradient) {
	u32 i, c[4];
	const char *const corner[] = {
		"upper_left", "upper_right", "lower_right", "lower_left"
	};

	if (!node)
		return false;

	node = mxmlFindElement(node, node, element, NULL, NULL, MXML_DESCEND_FIRST);

	if (!node)
		return false;

	for (i = 0; i < 4; ++i)
		if (!_get_color(node, corner[i], &c[i]))
			return false;

	gradient->ul = c[0];
	gradient->ur = c[1];
	gradient->ll = c[2];
	gradient->lr = c[3];

	return true;
}

// sync with font.h
static const char *targets[FONT_MAX] = {
	"label",
	"dlgtitle",
	"memo",
	"appname",
	"appdesc",
	"button",
	"button_desel"
};

static void _get_font(mxml_node_t *node) {
	theme_font_t f;
	f.file = _xmldup(_get_elem_cdata(node, "file"));
	f.size = _get_elem_int(node, "size", 0);
	f.color = NO_COLOR;
	_get_color(node, "color", &f.color);

	const char *t = mxmlElementGetAttr(node, "target");
	if (t && t[0]) {
		char *trg = pstrdup(t);
		char *tok;
		tok = strtok(trg, ", ");
		while (tok) {
			int i;
			for (i=0; i<FONT_MAX; i++) {
				if (!strcmp(tok, targets[i])) {
					if (f.file) {
						if (theme.fonts[i].file)
							free(theme.fonts[i].file);
						theme.fonts[i].file = f.file;
					}
					if (f.size)
						theme.fonts[i].size = f.size;
					if (f.color != NO_COLOR)
						theme.fonts[i].color = f.color;
				}
			}
			tok = strtok(NULL, ",");
		}
		free(trg);
	} else {
		if (f.file) {
			if (theme.default_font.file)
				free(theme.default_font.file);
			theme.default_font.file = f.file;
		}
		if (f.size)
			theme.default_font.size = f.size;
		if (f.color != NO_COLOR)
			theme.default_font.color = f.color;
	}
}

static char *_get_args(u16 *length, mxml_node_t *node, const char *element) {
	*length = 0;

	if (!node)
		return NULL;

	node = mxmlFindElement(node, node, element, NULL, NULL, MXML_DESCEND_FIRST);

	if (!node)
		return NULL;

	mxml_node_t *n;
	u16 len = 0;
	char *arg;

	for (n = mxmlFindElement(node, node, "arg", NULL, NULL, MXML_DESCEND_FIRST);
			n != NULL; n = mxmlFindElement(n, node, "arg", NULL, NULL,
			MXML_NO_DESCEND)) {
		arg = _get_cdata(n);

		if (arg) {
			if (len)
				len++;
			len += strlen(arg);
		}
	}

	if (!len)
		return NULL;

	len++;

	if (len > ARGS_MAX_LEN)
		return NULL;

	char *ret;

	ret = pmalloc(len);

	len = 0;

	for (n = mxmlFindElement(node, node, "arg", NULL, NULL, MXML_DESCEND_FIRST);
			n != NULL; n = mxmlFindElement(n, node, "arg", NULL, NULL,
			MXML_NO_DESCEND)) {
		arg = _get_cdata(n);

		if (arg) {
			if (len) {
				ret[len] = 0;
				len++;
			}

			strcpy(ret + len, arg);
			len += strlen(arg);
		}
	}

	ret[len] = 0;

	*length = len;

	return ret;
}

meta_info *meta_parse(char *fn) {
	int fd;
	mxml_node_t *root, *node;
	meta_info *res;
	char *s;
	struct tm t;

	fd = open(fn, O_RDONLY);
	if (fd < 0) {
		gprintf("error opening '%s'\n", fn);
		return NULL;
	}

	root = mxmlLoadFd(NULL, fd, MXML_OPAQUE_CALLBACK);

	close(fd);

	if (!root) {
		gprintf("error parsing '%s'\n", fn);
		return NULL;
	}

	node = mxmlFindElement(root, root, "app", NULL, NULL, MXML_DESCEND_FIRST);

	if (!node) {
		gprintf("no app node '%s'\n", fn);
		mxmlDelete(root);
		return NULL;
	}

	res = (meta_info *) pmalloc(sizeof(meta_info));
	memset(res, 0, sizeof(meta_info));

	res->name = _xmldup(_get_elem_cdata(node, "name"));
	res->coder = _xmldup(_get_elem_cdata(node, "author"));
	if (!res->coder)
		res->coder = _xmldup(_get_elem_cdata(node, "coder"));
	res->version = _xmldup(_get_elem_cdata(node, "version"));
	res->short_description = _xmldup(_get_elem_cdata(node, "short_description"));
	res->long_description = _xmldup(_get_elem_cdata(node, "long_description"));

	s = _get_elem_cdata(node, "release_date");

	if (s) {
		memset(&t, 0, sizeof(struct tm));
		if (sscanf(s, "%4d%2d%2d%2d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday,
					&t.tm_hour, &t.tm_min, &t.tm_sec) == 6) {
			t.tm_year -= 1900;

			res->release_date = mktime (&t);
		} else if (sscanf(s, "%4d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday) == 3) {
			t.tm_hour = 1;
			t.tm_min = 0;
			t.tm_sec = 0;
			t.tm_year -= 1900;

			res->release_date = mktime (&t);
		}
	}

	res->args = _get_args(&res->argslen, node, "arguments");

	if (mxmlFindElement(node, node, "ahb_access", NULL, NULL,
			MXML_DESCEND_FIRST))
		res->ahb_access = true;
	if (mxmlFindElement(node, node, "no_ios_reload", NULL, NULL,
			MXML_DESCEND_FIRST))
		res->ahb_access = true;

	mxmlDelete(root);

	return res;
}

void meta_free(meta_info *info) {
	if (!info)
		return;

	free(info->name);
	free(info->coder);
	free(info->version);
	free(info->short_description);
	free(info->long_description);
	free(info->args);

	free(info);
}

update_info *update_parse(char *buf) {
	mxml_node_t *root, *node;
	char *s;
	update_info *res;

	root = mxmlLoadString(NULL, buf, MXML_OPAQUE_CALLBACK);

	if (!root) {
		gprintf("error parsing update.xml!\n");
		return NULL;
	}

	node = mxmlFindElement(root, root, "app", NULL, NULL, MXML_DESCEND_FIRST);

	if (!node) {
		gprintf("no app node for update.xml!'\n");
		mxmlDelete(root);
		return NULL;
	}

	res = (update_info *) pmalloc(sizeof(update_info));
	memset(res, 0, sizeof(update_info));

	res->version = _xmldup(_get_elem_cdata(node, "version"));

	s = _get_elem_cdata(node, "date");
	if (s) {
		if (sscanf(s, "%llu", &res->date) != 1)
			res->date = 0;
	}

	res->notes = _xmldup(_get_elem_cdata(node, "notes"));
	res->uri = _xmldup(_get_elem_cdata(node, "uri"));
	res->hash = _xmldup(_get_elem_cdata(node, "hash"));

	mxmlDelete(root);

	return res;
}

void update_free(update_info *info) {
	if (!info)
		return;

	free(info->version);
	free(info->notes);
	free(info->uri);

	free(info);
}

bool settings_load(void) {
	s32 res;
	u8 *buf;
	mxml_node_t *root, *node;
	char *s;
	const char *titlepath;
	STACK_ALIGN(char, fn, ISFS_MAXPATH, 32);

	settings.device = -1;
	settings.sort_order = 0;
	settings.browse_mode = 0;
	memset(settings.app_sel, 0, sizeof(settings.app_sel));

	memcpy(&_loaded_settings, &settings, sizeof(settings_t));

	titlepath = title_get_path();
	if (!titlepath[0])
		return false;

	sprintf(fn, "%s/" FN_BANNER, titlepath);

	buf = NULL;
	res = isfs_get(fn, &buf, banner_bin_size, 0, false);
	if (res < 0) {
		gprintf("deleting banner: %ld\n", res);
		ISFS_Delete(fn);
	} else if (memcmp(buf, banner_bin, banner_bin_size)) {
		gprintf("banner.bin mismatch, deleting\n");
		ISFS_Delete(fn);
	} else {
		write_banner = false;
	}

	if (buf) {
		free(buf);
		buf = NULL;
	}

	sprintf(fn, "%s/" FN_SETTINGS, titlepath);
	res = isfs_get(fn, &buf, 0, MAX_SETTINGS_XML_SIZE, false);
	if (res < 0) {
		gprintf("isfs_get failed: %ld\n", res);
		return false;
	}

	root = mxmlLoadString(NULL, (char *) buf, MXML_OPAQUE_CALLBACK);
	free(buf);
	buf = NULL;

	if (!root) {
		gprintf("error parsing settings.xml!\n");
		return false;
	}

	node = mxmlFindElement(root, root, "hbc", NULL, NULL, MXML_DESCEND_FIRST);

	if (!node) {
		gprintf("no hbc node for settings.xml!'\n");
		mxmlDelete(root);
		return false;
	}

	settings.device = _get_elem_int(node, "device", -1);
	settings.sort_order = _get_elem_int(node, "sort_order", 0);
	settings.browse_mode = _get_elem_int(node, "browse_mode", 0);
	s = _get_elem_cdata(node, "app_sel");
	if (s) {
		strncpy(settings.app_sel, s, sizeof(settings.app_sel));
		settings.app_sel[sizeof(settings.app_sel) - 1] = 0;
	}

	mxmlDelete(root);

	memcpy(&_loaded_settings, &settings, sizeof(settings_t));
	//hexdump(&_loaded_settings,sizeof(settings_t));

	return true;
}

bool settings_save(void) {
	s32 res;
	char *x;
	const char *titlepath;
	int size;

	STACK_ALIGN(char, fn, ISFS_MAXPATH, 32);

	titlepath = title_get_path();
	if (!titlepath[0])
		return false;

	if (!memcmp(&_loaded_settings, &settings, sizeof(settings_t)))
		return false;

	//hexdump(&settings,sizeof(settings_t));
	x = (char *) pmemalign(32, MAX_SETTINGS_XML_SIZE);

	mxml_node_t *xml;
	mxml_node_t *data;
	mxml_node_t *node;

	xml = mxmlNewXML("1.0");

	data = mxmlNewElement(xml, "hbc");

	node = mxmlNewElement(data, "device");
	mxmlNewInteger(node, settings.device);

	node = mxmlNewElement(data, "sort_order");
	mxmlNewInteger(node, settings.sort_order);

	node = mxmlNewElement(data, "browse_mode");
	mxmlNewInteger(node, settings.browse_mode);

	if (strlen(settings.app_sel) > 0) {
		node = mxmlNewElement(data, "app_sel");
		mxmlNewText(node, 0, settings.app_sel);
	}

	size = mxmlSaveString (xml, x, MAX_SETTINGS_XML_SIZE, MXML_NO_CALLBACK);
	x[MAX_SETTINGS_XML_SIZE - 1] = 0;

	if ((size < 16) || (size > MAX_SETTINGS_XML_SIZE - 1)) {
		gprintf("mxmlSaveString failed!\n");
		free(x);
		return false;
	}

	if (write_banner) {
		sprintf(fn, "%s/" FN_BANNER, titlepath);
		res = isfs_put(fn, banner_bin, banner_bin_size);
		if ((res < 0) && (res != -105)) {
			gprintf("isfs_put banner failed: %ld\n", res);
			free(x);
			return false;
		}
	}

	sprintf(fn, "%s/" FN_SETTINGS, titlepath);

	res = isfs_put(fn, x, strlen(x));
	if (res < 0) {
		gprintf("isfs_put xml failed: %ld\n", res);
		free(x);
		sprintf(fn, "%s/" FN_BANNER, titlepath);
		ISFS_Delete(fn);
		return false;
	}

	free(x);
	return true;
}

void theme_xml_init(void) {
	memset(&theme, 0, sizeof(theme));
}

bool load_theme_xml(char *buf) {
	int i;
	mxml_node_t *root, *node, *fnode;

	// free prior theme
	if (theme.description)
		free(theme.description);
	if (theme.default_font.file)
		free(theme.default_font.file);
	for (i=0; i<FONT_MAX; i++)
		if (theme.fonts[i].file)
			free(theme.fonts[i].file);

	root = mxmlLoadString(NULL, buf, MXML_OPAQUE_CALLBACK);

	if (!root) {
		gprintf("error parsing theme.xml!\n");
		return false;
	}

	node = mxmlFindElement(root, root, "theme", NULL, NULL, MXML_DESCEND_FIRST);

	if (!node) {
		gprintf("no theme node for theme.xml!'\n");
		mxmlDelete(root);
		return false;
	}

	theme.langs = 0;
	const char *l = mxmlElementGetAttr(node, "langs");
	if (l && l[0]) {
		char *lng = pstrdup(l);
		char *tok;
		tok = strtok(lng, ", ");
		while (tok) {
			if (!strcmp(tok, "ja"))
				theme.langs |= TLANG_JA;
			if (!strcmp(tok, "ko"))
				theme.langs |= TLANG_KO;
			if (!strcmp(tok, "zh"))
				theme.langs |= TLANG_ZH;
			tok = strtok(NULL, ",");
		}
		free(lng);
	}

	theme.description = _xmldup(_get_elem_cdata(node, "description"));
	if (theme.description && (strlen(theme.description) > 64))
		theme.description[64] = 0;

	theme.default_font.color = NO_COLOR;
	theme.default_font.file = NULL;
	theme.default_font.size = 0;
	for (i=0; i<FONT_MAX; i++) {
		theme.fonts[i].color = NO_COLOR;
		theme.fonts[i].file = NULL;
		theme.fonts[i].size = 0;
	}

	_get_color(node, "font_color", &theme.default_font.color);
	_get_gradient(node, "progress_gradient", &theme.progress);

	fnode = mxmlFindElement(node, root, "font", NULL, NULL, MXML_DESCEND_FIRST);
	while (fnode != NULL) {
		_get_font(fnode);
		fnode = mxmlFindElement(fnode, root, "font", NULL, NULL, MXML_NO_DESCEND);
	}

	mxmlDelete(root);

	return true;
}
