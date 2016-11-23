#ifndef _TEX_H_
#define _TEX_H_

#include <sys/stat.h>
#include <gctypes.h>

#include "gfx.h"

void tex_free (gfx_entity *entity);

gfx_entity * tex_from_png(const u8 *data, u32 size, u16 width, u16 height);
gfx_entity * tex_from_png_file(const char *fn, const struct stat *st,
							   u16 width, u16 height);
gfx_entity * tex_from_tex_vsplit (gfx_entity *ge, u16 hstart, u16 hend);

void save_rgba_png(u32 *buffer, u16 x, u16 y);

#endif

