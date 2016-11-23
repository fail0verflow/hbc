#ifndef _GFX_H_
#define _GFX_H_

#include <ogc/gx.h>

#define COL_DEFAULT 0xFFFFFFFF

#define WIDESCREEN_RATIO (852.0/640.0)

typedef struct {
	f32 x, y, z;
} gfx_coordinates;

typedef enum {
	GFXT_RGBA8,
	GFXT_A8
} gfx_tex_type;

typedef enum {
	GFXE_GRADIENT,
	GFXE_TEXTURE
} gfx_entity_type;

typedef struct {
	gfx_entity_type type;
	u16 w, h;

	union {
		struct {
			u32 c1, c2, c3, c4;
		} gradient;

		struct {
			u8 *pixels;
			gfx_tex_type type;
			GXTexObj obj;
		} texture;
	};
} gfx_entity;

typedef enum {
	GFXQ_NULL = -1,
	GFXQ_ORIGIN,
	GFXQ_SCISSOR,
	GFXQ_ENTITY
} gfx_queue_type;

typedef struct {
	gfx_queue_type type;

	union {
		struct {
			gfx_coordinates *coords;
		} origin;

		struct {
			u16 x, y, z, w, h;
		} scissor;

		struct {
			gfx_coordinates coords;
			f32 scale;
			f32 rad;
			gfx_entity *entity;
			u32 color;
		} entity;
	};
} gfx_queue_entry;

extern bool widescreen;
extern u16 view_height, view_width;

void gfx_init_video (void);
void gfx_init (void);
void gfx_deinit (void);

void gfx_get_efb_size(u16 *x, u16 *y);
void gfx_set_efb_buffer(u32 *buffer);

void gfx_gen_gradient (gfx_entity *entity, u16 w, u16 h,
					   u32 c1, u32 c2, u32 c3, u32 c4);
void gfx_gen_tex (gfx_entity *entity, u16 w, u16 h, u8 *pixels, gfx_tex_type type);

void gfx_qe_origin_push (gfx_queue_entry *entry, gfx_coordinates *coords);
void gfx_qe_origin_pop (gfx_queue_entry *entry);
void gfx_qe_scissor_reset (gfx_queue_entry *entry);
void gfx_qe_scissor (gfx_queue_entry *entry, u16 x, u16 y, u16 z, u16 w, u16 h);
void gfx_qe_entity (gfx_queue_entry *entry, gfx_entity *entity, f32 x, f32 y,
					s16 layer, u32 color);

void gfx_frame_start (void);
void gfx_frame_push (const gfx_queue_entry *entries, int count);
void gfx_frame_end (void);

#endif

