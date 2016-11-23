#ifndef _BROWSER_H_
#define _BROWSER_H_

#include <gctypes.h>

#include "view.h"

typedef enum {
	BA_ADD = 0,
	BA_REMOVE,
	BA_REFRESH,
	BA_NEXT,
	BA_PREV
} browser_action;

view * browser_init(void);
void browser_deinit(void);
void browser_theme_reinit(void);

void browser_gen_view(browser_action action, const app_entry *app);
void browser_set_focus(u32 bd);
app_entry *browser_sel(void);
void browser_switch_mode(void);

#endif

