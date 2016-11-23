#ifndef _M_MAIN_H_
#define _M_MAIN_H_

#include <gctypes.h>

#include "view.h"

view * m_main_init (void);
void m_main_deinit (void);
void m_main_theme_reinit (void);

void m_main_update (void);

#endif

