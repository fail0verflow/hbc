#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>

#include "../config.h"

#include "controls.h"
#include "gfx.h"

static s32 pointer_owner = -1;
WPADData *wpads[WPAD_MAX_WIIMOTES];

static int rumbling = 0;

void controls_init (void) {
	int i;

	i = WPAD_Init ();

	if(i < 0) {
		gprintf("WPAD_Init failed: %d\n",i);
		return;
	}

	for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
		WPAD_SetDataFormat (i, WPAD_FMT_BTNS_ACC_IR);
		WPAD_SetVRes (i, view_width + 128, view_height + 128);
	}
	WPAD_SetIdleTimeout (120);
}

void controls_deinit (void) {
	WPAD_Shutdown ();
}

static u32 wpad_button_transform(WPADData *wd, u32 btns) {
	btns &= ~PADW_BUTTON_NET_INIT;
	btns &= ~PADW_BUTTON_SCREENSHOT;

	switch(wd->exp.type) {
		case WPAD_EXP_NUNCHUK:
			if(btns & WPAD_NUNCHUK_BUTTON_Z)
				btns |= PADW_BUTTON_NET_INIT;
			if(btns & WPAD_NUNCHUK_BUTTON_C)
				btns |= PADW_BUTTON_SCREENSHOT;
			break;
		case WPAD_EXP_CLASSIC:
			if(btns & WPAD_CLASSIC_BUTTON_LEFT)
				btns |= WPAD_BUTTON_LEFT;
			if(btns & WPAD_CLASSIC_BUTTON_RIGHT)
				btns |= WPAD_BUTTON_RIGHT;
			if(btns & WPAD_CLASSIC_BUTTON_UP)
				btns |= WPAD_BUTTON_UP;
			if(btns & WPAD_CLASSIC_BUTTON_DOWN)
				btns |= WPAD_BUTTON_DOWN;
			if(btns & WPAD_CLASSIC_BUTTON_A)
				btns |= WPAD_BUTTON_A;
			if(btns & WPAD_CLASSIC_BUTTON_B)
				btns |= WPAD_BUTTON_B;
			if(btns & WPAD_CLASSIC_BUTTON_X)
				btns |= WPAD_BUTTON_1;
			if(btns & WPAD_CLASSIC_BUTTON_Y)
				btns |= WPAD_BUTTON_2;
			if((btns & WPAD_CLASSIC_BUTTON_FULL_L) || (btns & WPAD_CLASSIC_BUTTON_MINUS))
				btns |= WPAD_BUTTON_MINUS;
			if((btns & WPAD_CLASSIC_BUTTON_FULL_R) || (btns & WPAD_CLASSIC_BUTTON_PLUS))
				btns |= WPAD_BUTTON_PLUS;
			if(btns & WPAD_CLASSIC_BUTTON_HOME)
				btns |= WPAD_BUTTON_HOME;
			if(btns & WPAD_CLASSIC_BUTTON_ZR)
				btns |= PADW_BUTTON_NET_INIT;
			if(btns & WPAD_CLASSIC_BUTTON_ZL)
				btns |= PADW_BUTTON_SCREENSHOT;
			break;
		case WPAD_EXP_GUITARHERO3:
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP)
				btns |= WPAD_BUTTON_UP;
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN)
				btns |= WPAD_BUTTON_DOWN;
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_GREEN)
				btns |= WPAD_BUTTON_A;
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_RED)
				btns |= WPAD_BUTTON_B;
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_PLUS)
				btns |= WPAD_BUTTON_PLUS;
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_MINUS)
				btns |= WPAD_BUTTON_MINUS;
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_YELLOW)
				btns |= WPAD_BUTTON_LEFT;
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_BLUE)
				btns |= WPAD_BUTTON_RIGHT;
			if(btns & WPAD_GUITAR_HERO_3_BUTTON_ORANGE)
				btns |= WPAD_BUTTON_1;
			break;
	}
	return (btns&0xffff) << 16;
}

void controls_scan (u32 *down, u32 *held, u32 *up) {
	u32 bd, bh, bu;
	int i;
	s32 last_owner;

	PAD_ScanPads ();

	bd = PAD_ButtonsDown (0);
	bh = PAD_ButtonsHeld (0);
	bu = PAD_ButtonsUp (0);

	WPAD_ScanPads ();

	for(i=0;i<WPAD_MAX_WIIMOTES;i++)
		if(WPAD_Probe (i, NULL) == WPAD_ERR_NONE) {
			wpads[i] = WPAD_Data(i);
		} else {
			wpads[i] = NULL;
		}

	last_owner = pointer_owner;

	// kill pointer owner if it stops pointing
	if((pointer_owner >= 0) && (!wpads[pointer_owner] || !wpads[pointer_owner]->ir.valid)) {
		pointer_owner = -1;
	}

	// find a new pointer owner if necessary
	if(pointer_owner < 0) {
		for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
			if(wpads[i] && wpads[i]->ir.valid) {
				pointer_owner = i;
				break;
			}
		}
	}

	// pointer owner owns buttons
	if(pointer_owner >= 0) {
		bd |= wpad_button_transform(wpads[pointer_owner], wpads[pointer_owner]->btns_d);
		bh |= wpad_button_transform(wpads[pointer_owner], wpads[pointer_owner]->btns_h);
		bu |= wpad_button_transform(wpads[pointer_owner], wpads[pointer_owner]->btns_u);
	} else {
	// otherwise just mix all buttons together
		for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
			if(wpads[i]) {
				bd |= wpad_button_transform(wpads[i], wpads[i]->btns_d);
				bh |= wpad_button_transform(wpads[i], wpads[i]->btns_h);
				bu |= wpad_button_transform(wpads[i], wpads[i]->btns_u);
			}
		}
	}

	if(last_owner >= 0 && last_owner != pointer_owner && rumbling) {
		WPAD_Rumble (last_owner, 0);
		if(pointer_owner >= 0)
			WPAD_Rumble(pointer_owner, 1);
	}

	if (down)
		*down = bd;

	if (held)
		*held = bh;

	if (up)
		*up = bu;
}

bool controls_ir (s32 *x, s32 *y, f32 *roll) {
	if (pointer_owner >= 0 && wpads[pointer_owner] && wpads[pointer_owner]->ir.valid) {
		*x = wpads[pointer_owner]->ir.x - 64;
		*y = wpads[pointer_owner]->ir.y - 64;

		if(roll)
			*roll = -wpads[pointer_owner]->ir.angle / 180.0 * M_PI;
		return true;
	} else {
		return false;
	}
}

void controls_rumble(int rumble) {
	if(pointer_owner >= 0)
		WPAD_Rumble(pointer_owner, rumble);
	rumbling = rumble;
}

static s32 wpad_sticky(int chan) {
	s32 sy = 0;
	if(wpads[chan]) {
		switch(wpads[chan]->exp.type) {
			case WPAD_EXP_NUNCHUK:
				sy = wpads[chan]->exp.nunchuk.js.pos.y - wpads[chan]->exp.nunchuk.js.center.y;
				break;
			case WPAD_EXP_CLASSIC:
				sy = (wpads[chan]->exp.classic.ljs.pos.y - wpads[chan]->exp.classic.ljs.center.y) * 4;
				break;
			case WPAD_EXP_GUITARHERO3:
				sy = (wpads[chan]->exp.gh3.js.pos.y - wpads[chan]->exp.gh3.js.center.y) * 4;
				break;
		}
	}
	return sy;
}

#define DEADZONE 10

s32 deadzone(s32 v) {
	if(v > DEADZONE)
		return v - DEADZONE;
	if(v < -DEADZONE)
		return v + DEADZONE;
	return 0;
}

s32 controls_sticky(void) {
	s32 sy;
	int i;
	sy = deadzone(PAD_StickY(0));
	if(pointer_owner >= 0) {
		sy += deadzone(wpad_sticky(pointer_owner));
	} else {
		for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
			sy += deadzone(wpad_sticky(i));
		}
	}
	return sy;
}

void controls_set_ir_threshold(int on) {
	int i;
	for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
		if(on)
			WPAD_SetIdleThresholds(i, WPAD_THRESH_DEFAULT_BUTTONS, 10, WPAD_THRESH_DEFAULT_ACCEL, WPAD_THRESH_DEFAULT_JOYSTICK, WPAD_THRESH_DEFAULT_BALANCEBOARD, WPAD_THRESH_DEFAULT_MOTION_PLUS);
		else
			WPAD_SetIdleThresholds(i, WPAD_THRESH_DEFAULT_BUTTONS, WPAD_THRESH_DEFAULT_IR, WPAD_THRESH_DEFAULT_ACCEL, WPAD_THRESH_DEFAULT_JOYSTICK, WPAD_THRESH_DEFAULT_BALANCEBOARD, WPAD_THRESH_DEFAULT_MOTION_PLUS);
	}
}
