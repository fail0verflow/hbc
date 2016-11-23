#include <malloc.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>

#include <gccore.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/conf.h>

#include "../config.h"
#include "gfx.h"
#include "panic.h"
#include "title.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)
#define CLIPPING_X (view_width / 2 + 64)
#define CLIPPING_Y (view_height / 2 + 64)

#define FONT_NEAREST_NEIGHBOR

static u32 *xfb;
static GXRModeObj *vmode;

static float px_width, px_height;
static u8 *gp_fifo;

static Mtx view;

static u8 origin_stack_size;
static const gfx_coordinates *origin_stack[GFX_ORIGIN_STACK_SIZE];
static gfx_coordinates origin_current;

static u32 *efb_buffer = NULL;

u16 view_width = 0;
u16 view_height = 0;
bool widescreen = false;

void gfx_init_video (void) {
	u8 i;

	VIDEO_Init ();

	vmode = VIDEO_GetPreferredMode (NULL);

#ifdef ENABLE_WIDESCREEN
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9) {
		gprintf("aspect: 16:9\n");
		view_width = 852; // 16:9
		view_height = 480;

		vmode->viWidth = 672;

		widescreen = true;

		if (is_vwii()) {
			// poke DMCU to turn off pillarboxing
			write32(0xd8006a0, 0x30000004);
			mask32(0xd8006a8, 0, 2);
		}
	} else {
#endif
		gprintf("aspect: 4:3\n");
		view_width = 640;
		view_height = 480;

		vmode->viWidth = 672;

		if (is_vwii()) {
			// poke DMCU to turn on pillarboxing
			write32(0xd8006a0, 0x10000002);
			mask32(0xd8006a8, 0, 2);
		}
#ifdef ENABLE_WIDESCREEN
	}
#endif

	if (vmode == &TVPal576IntDfScale || vmode == &TVPal576ProgScale) {
		vmode->viXOrigin = (VI_MAX_WIDTH_PAL - vmode->viWidth) / 2;
		vmode->viYOrigin = (VI_MAX_HEIGHT_PAL - vmode->viHeight) / 2;
	} else {
		vmode->viXOrigin = (VI_MAX_WIDTH_NTSC - vmode->viWidth) / 2;
		vmode->viYOrigin = (VI_MAX_HEIGHT_NTSC - vmode->viHeight) / 2;
	}

	px_width = vmode->fbWidth / (float) view_width;
	px_height = vmode->efbHeight / (float) view_height;

	VIDEO_SetBlack (TRUE);
	VIDEO_Configure (vmode);
	VIDEO_Flush ();
	VIDEO_WaitVSync();

	xfb = (u32 *) SYS_AllocateFramebuffer (vmode);
	DCInvalidateRange(xfb, VIDEO_GetFrameBufferSize(vmode));
	xfb = MEM_K0_TO_K1 (xfb);

	VIDEO_ClearFrameBuffer (vmode, xfb, COLOR_BLACK);

	VIDEO_SetNextFramebuffer (xfb);
	VIDEO_SetBlack (TRUE);
	VIDEO_Flush ();

	gp_fifo = (u8 *) pmemalign (32, DEFAULT_FIFO_SIZE);

	for (i = 0; i < 4; ++i)
		VIDEO_WaitVSync();
}

void gfx_init () {
	Mtx44 p;

	GX_AbortFrame ();

	GXColor gxbackground = { 0, 0, 0, 0xff };

	memset (gp_fifo, 0, DEFAULT_FIFO_SIZE);

	GX_Init (gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear (gxbackground, 0x00ffffff);

	// GX voodoo - properly comment me some day
	// the 0.05 fudge factors for some reason help align textures on the pixel grid
	// it's still not perfect though
	GX_SetViewport (0, 0, vmode->fbWidth+0.05, vmode->efbHeight+0.05, 0, 1);
	GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE,
					  vmode->vfilter);
	GX_SetFieldMode (vmode->field_rendering,
					 ((vmode->viHeight == 2 * vmode->xfbHeight) ?
					 GX_ENABLE : GX_DISABLE));

	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode (GX_CULL_NONE);

	GX_SetDispCopyGamma (GX_GM_1_0);

	guOrtho(p, view_height / 2, -(view_height / 2),
			-(view_width / 2), view_width / 2, 100, 1000);


	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc (GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	memset (&view, 0, sizeof (Mtx));
	guMtxIdentity(view);

	GX_Flush ();
	GX_DrawDone ();

	GX_SetColorUpdate (GX_TRUE);
	GX_CopyDisp (xfb, GX_TRUE);
	GX_Flush ();
	GX_DrawDone ();
	GX_CopyDisp (xfb, GX_TRUE);
	GX_Flush ();
	GX_DrawDone ();

	VIDEO_SetBlack (FALSE);
	VIDEO_Flush ();
}

void gfx_deinit (void) {
	u8 i;

	GX_AbortFrame ();
	VIDEO_SetBlack (TRUE);
	VIDEO_Flush();

	for (i = 0; i < 4; ++i)
		VIDEO_WaitVSync();
}

void gfx_get_efb_size(u16 *x, u16*y) {
	*x = vmode->fbWidth;
	*y = vmode->efbHeight;
}

void gfx_set_efb_buffer(u32 *buffer) {
	efb_buffer = buffer;
}

// entity generators

void gfx_gen_gradient (gfx_entity *entity, u16 w, u16 h,
					   u32 c1, u32 c2, u32 c3, u32 c4) {
	entity->type = GFXE_GRADIENT;
	entity->w = w;
	entity->h = h;

	entity->gradient.c1 = c1;
	entity->gradient.c2 = c2;
	entity->gradient.c3 = c3;
	entity->gradient.c4 = c4;
}

void gfx_gen_tex (gfx_entity *entity, u16 w, u16 h, u8 *pixels, gfx_tex_type type) {
	entity->type = GFXE_TEXTURE;
	entity->w = w;
	entity->h = h;
	entity->texture.pixels = pixels;
	entity->texture.type = type;

	switch(type) {
		case GFXT_RGBA8:
			DCFlushRange (pixels, w * h * 4);
			GX_InitTexObj (&entity->texture.obj, pixels, w, h, GX_TF_RGBA8, GX_CLAMP,
							GX_CLAMP, GX_FALSE);
			break;
		case GFXT_A8:
			DCFlushRange (pixels, w * h);
			GX_InitTexObj (&entity->texture.obj, pixels, w, h, GX_TF_I8, GX_CLAMP,
							GX_CLAMP, GX_FALSE);
#ifdef FONT_NEAREST_NEIGHBOR
			GX_InitTexObjFilterMode(&entity->texture.obj, GX_NEAR, GX_NEAR);
#endif
			break;
	}
}

// queue entry generators

void gfx_qe_origin_push (gfx_queue_entry *entry, gfx_coordinates *coords) {
	entry->type = GFXQ_ORIGIN;
	entry->origin.coords = coords;
}

void gfx_qe_origin_pop (gfx_queue_entry *entry) {
	entry->type = GFXQ_ORIGIN;
	entry->origin.coords = NULL;
}

void gfx_qe_scissor_reset (gfx_queue_entry *entry) {
	entry->type = GFXQ_SCISSOR;
	entry->scissor.x = 0;
	entry->scissor.y = 0;
	entry->scissor.z = 0;
	entry->scissor.w = 0;
	entry->scissor.h = 0;
}

void gfx_qe_scissor (gfx_queue_entry *entry, u16 x, u16 y, u16 z, u16 w, u16 h) {
	entry->type = GFXQ_SCISSOR;
	entry->scissor.x = x;
	entry->scissor.y = y;
	entry->scissor.z = z;
	entry->scissor.w = w;
	entry->scissor.h = h;
}

void gfx_qe_entity (gfx_queue_entry *entry, gfx_entity *entity, f32 x, f32 y,
					s16 layer, u32 color) {
	entry->type = GFXQ_ENTITY;
	entry->entity.coords.x = x;
	entry->entity.coords.y = y + entity->h;
	entry->entity.coords.z = layer;
	entry->entity.scale = 1.0f;
	entry->entity.rad = 0.0f;
	entry->entity.entity = entity;
	entry->entity.color = color;
}

// helper functions

static void get_origin (s16 *x, s16 *y, s16 *z, u8 m) {
	u8 i;

	*x = -view_width / 2;
	*y = view_height / 2;
	*z = VIEW_Z_ORIGIN;

	for (i = 0; i < origin_stack_size - m; ++i) {
		*x += origin_stack[i]->x;
		*y -= origin_stack[i]->y;
		*z += origin_stack[i]->z;
	}
}

static void set_scissor (const gfx_queue_entry *entry) {
	s16 x, y, z, w, h;

	if (entry->scissor.w != 0 && entry->scissor.h != 0) {
		// TODO 1ast parm hardcoded for memo
		get_origin (&x, &y, &z, 1);
		x = roundf ((float) (x + view_width / 2 + entry->scissor.x) * px_width);
		y = roundf ((float) (-y + view_height / 2 + entry->scissor.y) *
					px_height);
		w = roundf ((float) entry->scissor.w * px_width);
		h = roundf ((float) entry->scissor.h * px_height);

		GX_SetScissor (x, y, w, h);
	} else {
		GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
	}
}

static void plot_vert_c (f32 x, f32 y, f32 z, u32 c) {
	GX_Position3f32 (x, y, z);
	GX_Color1u32 (c);
	GX_TexCoord2f32 (0.0f, 0.0f);
}

static void plot_gradient (const gfx_queue_entry *entry, Mtx v) {
	Mtx m, m2;
	Mtx mv;
	f32 xf, yf, zf, wf, hf;

	xf = origin_current.x + entry->entity.coords.x;
	yf = origin_current.y - entry->entity.coords.y;
	zf = origin_current.z + entry->entity.coords.z;

	wf = entry->entity.entity->w;
	hf = entry->entity.entity->h;

	guMtxIdentity(m);
	guMtxTransApply(m, m, -wf/2, -hf/2, 0);

	if (entry->entity.scale != 1.0f) {
		guMtxScale(m2, entry->entity.scale, entry->entity.scale, 0.0);
		guMtxConcat(m2, m, m);
	}

	if (entry->entity.rad != 0.0f) {
		guMtxRotRad(m2, 'z', entry->entity.rad);
		guMtxConcat(m2, m, m);
	}

	guMtxTransApply(m, m, wf/2+xf, hf/2+yf, zf);
	guMtxConcat(v, m, mv);

	GX_LoadPosMtxImm (mv, GX_PNMTX0);

	GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
	plot_vert_c (0, hf, 0, entry->entity.entity->gradient.c1);
	plot_vert_c (wf, hf, 0, entry->entity.entity->gradient.c2);
	plot_vert_c (wf, 0, 0, entry->entity.entity->gradient.c3);
	plot_vert_c (0, 0, 0, entry->entity.entity->gradient.c4);
	GX_End ();
}

static void plot_vert (f32 x, f32 y, f32 z, f32 s, f32 t, u32 c) {
	GX_Position3f32 (x, y, z);
	GX_Color1u32 (c);
	GX_TexCoord2f32 (s, t);
}

static void plot_texture (const gfx_queue_entry *entry, Mtx v) {
	Mtx m, m2;
	Mtx mv;
	f32 xf, yf, zf, wf, hf;

	yf = origin_current.y - entry->entity.coords.y;
	zf = origin_current.z + entry->entity.coords.z;
	hf = entry->entity.entity->h;

	if (widescreen && entry->entity.entity->texture.type == GFXT_A8) {
		// align origin to pixel grid
		xf = (int)(origin_current.x / WIDESCREEN_RATIO);
		xf = xf*WIDESCREEN_RATIO + entry->entity.coords.x;
		// width is specified in physical pixels
		wf = entry->entity.entity->w * WIDESCREEN_RATIO;
	} else {
		xf = origin_current.x + entry->entity.coords.x;

		wf = entry->entity.entity->w;
	}

	if (yf > CLIPPING_Y)
		return;

	if ((yf + hf) < -CLIPPING_Y)
		return;

	if (xf > CLIPPING_X)
		return;

	if ((xf + wf) < -CLIPPING_X)
		return;

	GX_LoadTexObj (&entry->entity.entity->texture.obj, GX_TEXMAP0);

	guMtxIdentity(m);
	guMtxTransApply(m, m, -wf/2, -hf/2, 0);

	if (entry->entity.scale != 1.0f) {
		guMtxScale(m2, entry->entity.scale, entry->entity.scale, 0.0);
		guMtxConcat(m2, m, m);
	}

	if (entry->entity.rad != 0.0f) {
		guMtxRotRad(m2, 'z', entry->entity.rad);
		guMtxConcat(m2, m, m);
	}

	guMtxTransApply(m, m, wf/2+xf, hf/2+yf, zf);
	guMtxConcat(v, m, mv);

	switch (entry->entity.entity->texture.type) {
		case GFXT_RGBA8:
			GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
			break;
		case GFXT_A8:
			GX_SetNumTevStages(1);
			GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_RASC);
			GX_SetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_TEXA,GX_CA_RASA,GX_CA_ZERO);
			GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
			GX_SetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
			break;
	}
	
	GX_LoadPosMtxImm (mv, GX_PNMTX0);

	GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
	plot_vert (0, hf, 0, 0.0, 0.0, entry->entity.color);
	plot_vert (wf, hf, 0, 1.0, 0.0, entry->entity.color);
	plot_vert (wf, 0, 0, 1.0, 1.0, entry->entity.color);
	plot_vert (0, 0, 0, 0.0, 1.0, entry->entity.color);
	GX_End ();
}

// higher level frame functions

void gfx_frame_start (void) {
	origin_stack_size = 0;
	origin_current.x = -view_width / 2;
	origin_current.y = view_height / 2;
	origin_current.z = VIEW_Z_ORIGIN;

	GX_InvVtxCache ();
	GX_InvalidateTexAll ();

	GX_SetNumChans (1);
	GX_SetNumTexGens (1);
	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	GX_SetBlendMode (GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
						GX_LO_NOOP);
}

void gfx_frame_push (const gfx_queue_entry *entries, int count) {
	while (count--) {
		switch (entries->type) {
		case GFXQ_ORIGIN:
			if (entries->origin.coords) {
				origin_current.x += entries->origin.coords->x;
				origin_current.y -= entries->origin.coords->y;
				origin_current.z += entries->origin.coords->z;
				origin_stack[origin_stack_size] = entries->origin.coords;
				origin_stack_size++;
			} else {
				if (origin_stack_size > 0) {
					origin_current.x -= origin_stack[origin_stack_size - 1]->x;
					origin_current.y += origin_stack[origin_stack_size - 1]->y;
					origin_current.z -= origin_stack[origin_stack_size - 1]->z;
					origin_stack_size--;
				} else {
					origin_current.x += -view_width / 2;
					origin_current.y -= view_height / 2;
					origin_current.z += view_height / 2;
				}
			}
			break;

		case GFXQ_SCISSOR:
			set_scissor (entries);
			break;

		case GFXQ_ENTITY:
			switch (entries->entity.entity->type) {
			case GFXE_GRADIENT:
				GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
				plot_gradient (entries, view);

				break;

			case GFXE_TEXTURE:
				plot_texture (entries, view);
				break;
			}
		case GFXQ_NULL:
			break;
		default:
			gprintf("Unknown queue entry type 0x%x\n", entries->type);
			break;
		}
		entries++;
	}
}

void gfx_frame_end (void) {
	GX_DrawDone ();

#ifdef ENABLE_SCREENSHOTS
	if (efb_buffer) {
		u16 x, y;
		GXColor c;
		u32 val;
		u32 *p = efb_buffer;

		for (y = 0; y < vmode->efbHeight; ++y) {
			for (x = 0; x < vmode->fbWidth; ++x) {
				GX_PeekARGB(x, y, &c);
				val = ((u32) c.a) << 24;
				val |= ((u32) c.r) << 16;
				val |= ((u32) c.g) << 8;
				val |= c.b;

				*p++ = val;
			}
		}

		efb_buffer = NULL;
	}
#endif

	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate (GX_TRUE);

	VIDEO_WaitVSync ();
	GX_CopyDisp (xfb, GX_TRUE);
	GX_Flush ();
}

