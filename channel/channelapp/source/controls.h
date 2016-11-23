#ifndef _CONTROLS_H_
#define _CONTROLS_H_

#include <gctypes.h>
#include <wiiuse/wpad.h>

// fake wiimote buttons for mapping extension buttons
#define PADW_BUTTON_NET_INIT 0x2000
#define PADW_BUTTON_SCREENSHOT 0x4000

#define PADS_A (PAD_BUTTON_A | (WPAD_BUTTON_A << 16))
#define PADS_B (PAD_BUTTON_B | (WPAD_BUTTON_B << 16))
#define PADS_MINUS (PAD_TRIGGER_L | (WPAD_BUTTON_MINUS << 16))
#define PADS_PLUS (PAD_TRIGGER_R | (WPAD_BUTTON_PLUS << 16))
#define PADS_1 (PAD_BUTTON_X | (WPAD_BUTTON_1 << 16))
#define PADS_2 (PAD_BUTTON_Y | (WPAD_BUTTON_2 << 16))
#define PADS_HOME (PAD_BUTTON_START | (WPAD_BUTTON_HOME << 16))
#define PADS_UP (PAD_BUTTON_UP | (WPAD_BUTTON_UP << 16))
#define PADS_DOWN (PAD_BUTTON_DOWN | (WPAD_BUTTON_DOWN << 16))
#define PADS_LEFT (PAD_BUTTON_LEFT | (WPAD_BUTTON_LEFT << 16))
#define PADS_RIGHT (PAD_BUTTON_RIGHT | (WPAD_BUTTON_RIGHT << 16))
#define PADS_DPAD (PADS_UP | PADS_DOWN | PADS_LEFT | PADS_RIGHT)
#define PADS_NET_INIT (PAD_TRIGGER_Z | (PADW_BUTTON_NET_INIT << 16))
#define PADS_SCREENSHOT (PADW_BUTTON_SCREENSHOT << 16)

void controls_init (void);
void controls_deinit (void);

void controls_scan (u32 *down, u32 *held, u32 *up);
bool controls_ir (s32 *x, s32 *y, f32 *roll);
void controls_rumble(int rumble);
s32 controls_sticky(void);
void controls_set_ir_threshold(int on);

#endif

