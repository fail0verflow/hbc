#ifndef _CURSORS_H_
#define _CURSORS_H_

#include <gctypes.h>

#include "gfx.h"
#include "tex.h"

typedef enum {
	CUR_STD = 0,
	CUR_DRAG,

	CUR_MAX
} cursor_type;

typedef struct {
	gfx_entity *tex[2];

	s16 hotspot_x, hotspot_y;
} cursor;

void cursors_init (void);
void cursors_deinit (void);
void cursors_queue (gfx_queue_entry *queue, cursor_type type, s16 x, s16 y,
					f32 roll);

#endif

