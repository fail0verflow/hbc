#include <stdlib.h>
#include <math.h>

#include "../config.h"
#include "gfx.h"
#include "theme.h"

#include "bubbles.h"

#include <ogc/lwp_watchdog.h>

#define BUBBLE_DELTA (MAX_BUBBLE_COUNT-MIN_BUBBLE_COUNT)

typedef struct {
	float x;
	float py;
	float speed;
	float xm;
	float val;
	float step;
	int popped;
	int popcnt;
	int tex;
} bubble;

static gfx_entity *tex_bubbles[3];

static gfx_queue_entry entries_bubbles[MAX_BUBBLE_COUNT];
static gfx_queue_entry entries_sub_bubbles[MAX_BUBBLE_COUNT][BUBBLE_POP_MAX];

static bubble bubbles[MAX_BUBBLE_COUNT];
static bubble sub_bubbles[MAX_BUBBLE_COUNT][BUBBLE_POP_MAX];

static int bubble_count = -1;

static void bubble_rand(int i) {
	int tex;

	tex = IRAND (3);

	bubbles[i].x = IRAND (view_width);
	bubbles[i].py = view_height + IRAND (200);
	bubbles[i].speed = 1.2 + FRAND (4 - tex);
	bubbles[i].xm = 3.0 + (IRAND ((tex + 1)) * bubbles[i].speed);
	bubbles[i].val = 0;
	bubbles[i].step = M_TWOPI / (64 + FRAND (64.0));
	bubbles[i].popped = 0;
	bubbles[i].popcnt = 0;
	bubbles[i].tex = tex;

	gfx_qe_entity(&entries_bubbles[i], tex_bubbles[tex], bubbles[i].x,
					bubbles[i].py, -2, COL_DEFAULT);

	entries_bubbles[i].entity.scale = BUBBLE_SIZE_MIN +
				FRAND (BUBBLE_SIZE_MAX - BUBBLE_SIZE_MIN);
	entries_bubbles[i].entity.rad = FRAND (M_PI_4);
}

static void bubble_update_count(void) {
	static int div = 0;
	s32 minute;
	static int new_count;
	int t;

	// time() might be expensive due to RTC reading
	// so slow it down a bit
	if ((div++ >= 600) || (bubble_count < 0)) {
		div = 0;
		t = time(NULL);
		minute = (t / 60 - BUBBLE_MIN_TIME) % BUBBLE_TIME_CYCLE;

		if (minute <= BUBBLE_MAX_OFFSET)
			new_count = (BUBBLE_DELTA * minute / BUBBLE_MAX_OFFSET) +
							MIN_BUBBLE_COUNT;
		else
			new_count = (BUBBLE_DELTA * (BUBBLE_TIME_CYCLE - minute) /
							(BUBBLE_TIME_CYCLE - BUBBLE_MAX_OFFSET)) +
							MIN_BUBBLE_COUNT;

		if (new_count < MIN_BUBBLE_COUNT) // should never happen
			new_count = MIN_BUBBLE_COUNT;

		if (new_count > MAX_BUBBLE_COUNT) // should never happen
			new_count = MAX_BUBBLE_COUNT;
	}

	if (((div % 6) == 0) || (bubble_count < 0)) {
		if (bubble_count < 0)
			bubble_count = 0;
		if (bubble_count < new_count) {
			while (bubble_count < new_count)
				bubble_rand(bubble_count++);
		} else if (bubble_count > new_count)
			bubble_count--;
	}
}

static void bubble_pop(int i) {
	int j;
	bubbles[i].popped = 1;
	bubbles[i].popcnt = IRAND(BUBBLE_POP_MAX - BUBBLE_POP_MIN) + BUBBLE_POP_MIN;
	entries_bubbles[i].entity.color = 0x00000000;

	for (j = 0; j < bubbles[i].popcnt; j++) {
		int tex;
		float sa;
		float dx,dy;

		sa = FRAND(M_TWOPI);
		dx = sin(sa) * FRAND(BUBBLE_POP_SPREAD_X);
		dy = (cos(sa) - 1.5) * FRAND(BUBBLE_POP_SPREAD_Y);

		tex = bubbles[i].tex;

		sub_bubbles[i][j].x = bubbles[i].x + dx;
		sub_bubbles[i][j].py = entries_bubbles[i].entity.coords.y + dy;
		sub_bubbles[i][j].speed = bubbles[i].speed - 0.5 + FRAND (4 - tex);
		sub_bubbles[i][j].xm = bubbles[i].xm * (0.8 + FRAND(1.0));
		sub_bubbles[i][j].val = bubbles[i].val;
		sub_bubbles[i][j].step = bubbles[i].step * (0.8 + FRAND(0.4));
		sub_bubbles[i][j].popped = 0;
		sub_bubbles[i][j].popcnt = 0;

		gfx_qe_entity (&entries_sub_bubbles[i][j], tex_bubbles[tex],
						sub_bubbles[i][j].x, sub_bubbles[i][j].py,
						-2, COL_DEFAULT);

		entries_sub_bubbles[i][j].entity.scale = (BUBBLE_POP_SIZE_MIN +
					FRAND (BUBBLE_POP_SIZE_MAX - BUBBLE_POP_SIZE_MIN)) *
					entries_bubbles[i].entity.scale;
		entries_sub_bubbles[i][j].entity.rad = entries_bubbles[i].entity.rad;
	}
}

void bubbles_init(void) {
	srand (gettime ());
	bubbles_theme_reinit();
	bubble_update_count();
}

void bubbles_deinit(void) {
}

void bubbles_theme_reinit(void) {
	tex_bubbles[0] = theme_gfx[THEME_BUBBLE1];
	tex_bubbles[1] = theme_gfx[THEME_BUBBLE2];
	tex_bubbles[2] = theme_gfx[THEME_BUBBLE3];

	bubble_count = -1;
	bubble_update_count();
}

void bubble_update(bool wm, s32 x, s32 y) {
	int i, j, deadcnt;
	gfx_coordinates *coords;
	f32 radius;

	bubble_update_count();

	for (i = 0; i < bubble_count; ++i) {
		coords = &entries_bubbles[i].entity.coords;
		radius = entries_bubbles[i].entity.scale *
					entries_bubbles[i].entity.entity->w / 2 * BUBBLE_POP_RADIUS;
		if (!bubbles[i].popped && wm) {
			float cx = coords->x + entries_bubbles[i].entity.entity->w/2;
			float cy = coords->y - entries_bubbles[i].entity.entity->h/2;
			if ((abs(x - cx) < radius) && (abs(y - cy) < radius))
				bubble_pop(i);
		}

		if (bubbles[i].popped) {
			deadcnt = 0;

			for (j = 0; j < bubbles[i].popcnt; j++) {
				coords = &entries_sub_bubbles[i][j].entity.coords;
				radius = entries_sub_bubbles[i][j].entity.scale *
						entries_sub_bubbles[i][j].entity.entity->w / 2 *
						BUBBLE_POP_RADIUS;

				if (!sub_bubbles[i][j].popped && wm) {
					float cx = coords->x + entries_bubbles[i].entity.entity->w/2;
					float cy = coords->y - entries_bubbles[i].entity.entity->h/2;
					if ((abs(x - cx) < radius) && (abs(y - cy) < radius)) {
						entries_sub_bubbles[i][j].entity.color = 0x00000000;
						sub_bubbles[i][j].popped = 1;
					}
				}

				sub_bubbles[i][j].py -= sub_bubbles[i][j].speed;
				coords->y = sub_bubbles[i][j].py;

				if ((coords->y < -100) || sub_bubbles[i][j].popped) {
					deadcnt++;
					continue;
				}

				coords->x = sub_bubbles[i][j].x + roundf (sub_bubbles[i][j].xm *
							sinf (sub_bubbles[i][j].val));
				sub_bubbles[i][j].val += sub_bubbles[i][j].step;
			}

			if(deadcnt >= bubbles[i].popcnt) {
				bubble_rand(i);
				continue;
			}

			gfx_frame_push (entries_sub_bubbles[i], bubbles[i].popcnt);
		} else {
			bubbles[i].py -= bubbles[i].speed;
			coords->y = bubbles[i].py;

			if (coords->y < -100) {
				bubble_rand(i);
				continue;
			}

			coords->x = bubbles[i].x + roundf (bubbles[i].xm *
						sinf (bubbles[i].val));
			bubbles[i].val += bubbles[i].step;
		}
	}

	gfx_frame_push(entries_bubbles, bubble_count);
}

void bubble_popall(void) {
	int i;

	for (i = 0; i < bubble_count; ++i)
		if (!bubbles[i].popped)
			bubble_pop(i);
}

