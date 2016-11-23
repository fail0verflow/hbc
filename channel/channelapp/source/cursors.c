#include "../config.h"

#include <gctypes.h>

#include "cursor_drag_png.h"
#include "cursor_drag_shade_png.h"
#include "cursor_pic_png.h"
#include "cursor_shade_png.h"

#include "cursors.h"

static cursor cursors[CUR_MAX];

void cursors_init (void) {
	cursors[CUR_STD].tex[0] = tex_from_png (cursor_shade_png,
											cursor_shade_png_size, 96,96);
	cursors[CUR_STD].tex[1] = tex_from_png (cursor_pic_png, cursor_pic_png_size,
											96, 96);

	cursors[CUR_STD].hotspot_x = cursors[CUR_STD].tex[1]->w / 2;
	cursors[CUR_STD].hotspot_y = cursors[CUR_STD].tex[1]->h / 2;

	cursors[CUR_DRAG].tex[0] = tex_from_png (cursor_drag_shade_png,
												cursor_drag_shade_png_size,
												96, 96);
	cursors[CUR_DRAG].tex[1] = tex_from_png (cursor_drag_png,
												cursor_drag_png_size, 96, 96);
	cursors[CUR_DRAG].hotspot_x = cursors[CUR_DRAG].tex[1]->w / 2;
	cursors[CUR_DRAG].hotspot_y = cursors[CUR_DRAG].tex[1]->h / 2;
}

void cursors_deinit (void) {
	u8 i;

	for (i = 0; i < CUR_MAX; ++i) {
		tex_free (cursors[i].tex[0]);
		tex_free (cursors[i].tex[1]);
	}
}

void cursors_queue (gfx_queue_entry *queue, cursor_type type, s16 x, s16 y,
					f32 roll) {
	gfx_qe_entity (&queue[0], cursors[type].tex[0],
					x - cursors[type].hotspot_x + 2,
					y - cursors[type].hotspot_y + 4, TEX_LAYER_CURSOR,
					COL_DEFAULT);
	gfx_qe_entity (&queue[1], cursors[type].tex[1],
					x - cursors[type].hotspot_x,
					y - cursors[type].hotspot_y, TEX_LAYER_CURSOR + 1,
					COL_DEFAULT);

	queue[0].entity.rad = roll;
	queue[1].entity.rad = roll;
}

