#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <zlib.h>
#include <png.h>

#include "../config.h"
#include "tex.h"
#include "panic.h"

#define SCREENSHOT_FILENAME "/hbc-%03u.png"

static u32 screenshot_index = 0;

void tex_free (gfx_entity *entity) {
	if (!entity)
		return;

	free (entity->texture.pixels);
	free (entity);
}

static void pngcb_error (png_structp png_ptr, png_const_charp error_msg) {
	gprintf ("ERROR: Couldn't load PNG image: %s\n", error_msg);
}

static void pngcb_read (png_structp png_ptr, png_bytep data,
						png_size_t length) {
	u8 **p = (u8 **) png_get_io_ptr (png_ptr);
	memcpy (data, *p, length);
	*p += length;
}

gfx_entity * tex_from_png(const u8 *data, u32 size, u16 width, u16 height) {
	int res;
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 w, h;
	png_bytepp rows;

	u8 *pixels;
	u8 *s, *d;
	u32 x, y;
	u8 r;

	gfx_entity *entity;

	res = png_sig_cmp ((u8 *) data, 0, 4);
	if (res) {
		gprintf ("png_sig_cmp failed: %d\n", res);

		return NULL;
	}

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, pngcb_error,
										NULL);

	if (!png_ptr) {
		gprintf ("png_create_read_struct failed\n");

		return NULL;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		gprintf ("png_create_info_struct failed\n");
		png_destroy_read_struct (&png_ptr, (png_infopp)NULL, (png_infopp)NULL);

		return NULL;
	}

	if (setjmp (png_jmpbuf (png_ptr))) {
		gprintf ("setjmp failed\n");
		png_destroy_read_struct (&png_ptr, &info_ptr, (png_infopp)NULL);

		return NULL;
	}

	png_set_read_fn (png_ptr, &data, pngcb_read);
	png_set_user_limits (png_ptr, width, height);
	png_set_add_alpha (png_ptr, 0xff, PNG_FILLER_BEFORE);
	png_read_png (png_ptr, info_ptr, PNG_TRANSFORM_PACKING |
					PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_SWAP_ALPHA,
					(png_voidp)NULL);

	w = png_get_image_width (png_ptr, info_ptr);
	h = png_get_image_height (png_ptr, info_ptr);

	if ((w != width) || (h != height)) {
		gprintf ("invalid png dimension!\n");
		png_destroy_read_struct (&png_ptr, &info_ptr, (png_infopp)NULL);

		return NULL;
	}

	rows = png_get_rows (png_ptr, info_ptr);

	pixels = (u8 *) pmemalign (32, w * h * 4);
	d = pixels;

	for (y = 0; y < h; y += 4) {
		for (x = 0; x < w; x += 4) {
			for (r = 0; r < 4; ++r) {
				s = &rows[y + r][x << 2];

				*d++ = s[0];
				*d++ = s[1];
				*d++ = s[4];
				*d++ = s[5];
				*d++ = s[8];
				*d++ = s[9];
				*d++ = s[12];
				*d++ = s[13];
			}

			for (r = 0; r < 4; ++r) {
				s = &rows[y + r][x << 2];

				*d++ = s[2];
				*d++ = s[3];
				*d++ = s[6];
				*d++ = s[7];
				*d++ = s[10];
				*d++ = s[11];
				*d++ = s[14];
				*d++ = s[15];
			}

			CHKBUFACC(d - 1, pixels, w * h * 4);
		}
	}

	png_destroy_read_struct (&png_ptr, &info_ptr, (png_infopp)NULL);

	entity = (gfx_entity *) pmalloc (sizeof (gfx_entity));
	gfx_gen_tex (entity, w, h, pixels, GFXT_RGBA8);

	return entity;
}

gfx_entity * tex_from_png_file(const char *fn, const struct stat *st,
							   u16 width, u16 height) {
	u8 *buf;
	int fd;
	gfx_entity *entity;

	fd = open (fn, O_RDONLY);

	if (fd == -1)
		return NULL;

	buf = (u8 *) pmalloc (st->st_size);

	if (st->st_size != read (fd, buf, st->st_size)) {
		free (buf);
		close (fd);
		return NULL;
	}

	entity = tex_from_png (buf, st->st_size, width, height);

	close (fd);
	free (buf);

	return entity;
}

gfx_entity * tex_from_tex_vsplit(gfx_entity *ge, u16 hstart, u16 hend)
{
	gfx_entity *entity;
	u16 h = hend - hstart;

	entity = (gfx_entity *) pmalloc (sizeof (gfx_entity));
	gfx_gen_tex (entity, ge->w, h, ge->texture.pixels + ge->w * hstart * 4, GFXT_RGBA8);
	return entity;
}

void save_rgba_png(u32 *buffer, u16 x, u16 y) {
	char fn[16];
	struct stat st;
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr;
	png_bytep *row_pointers;
	u16 i;

	while (true) {
		if (screenshot_index > 999)
			return;

		sprintf(fn, SCREENSHOT_FILENAME, screenshot_index);
		if (!stat(fn, &st)) {
			screenshot_index++;
			continue;
		}

		if (errno == ENOENT)
			break;

		gprintf("error looking for a screenshot spot %d\n", errno);
		return;
	}

	row_pointers = (png_bytep *) pmalloc(y * sizeof(png_bytep));

	fp = fopen(fn, "wb");
	if (!fp) {
		gprintf("couldnt open %s for writing\n", fn);
		goto exit;
	}

	setbuf(fp, NULL);

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png_ptr) {
		gprintf ("png_create_write_struct failed\n");
		goto exit;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		gprintf ("png_create_info_struct failed\n");
		goto exit;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		gprintf ("setjmp failed\n");
		goto exit;
	}

	png_init_io(png_ptr, fp);

	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_IHDR(png_ptr, info_ptr, x, y, 8,
				PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

	for (i = 0; i < y; ++i)
		row_pointers[i] = (png_bytep) (buffer + i * x);

	png_set_swap_alpha(png_ptr);

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	gprintf("saved %s\n", fn);
	screenshot_index++;

exit:
	if (png_ptr)
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

	free(row_pointers);
	fclose(fp);
}

