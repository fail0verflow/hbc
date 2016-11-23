#ifndef _UPDATE_H_
#define _UPDATE_H_

#include <gctypes.h>

#include "loader_reloc.h"

bool update_signal(void);
bool update_busy(bool *update_available);
bool update_execute(view *sub_view, entry_point *ep);

#endif

