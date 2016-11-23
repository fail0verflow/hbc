#ifndef _BUBBLES_H_
#define _BUBBLES_H_

#include <gctypes.h>

void bubbles_init(void);
void bubbles_deinit(void);
void bubbles_theme_reinit(void);

void bubble_update(bool wm, s32 x, s32 y);

void bubble_popall(void);

#endif

