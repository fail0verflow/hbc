#ifndef _MANAGE_H_
#define _MANAGE_H_

#include <gctypes.h>

#include "view.h"

s32 dir_exists(char *dirname);

bool manage_is_zip(const u8 *data);
bool manage_check_zip_app(u8 *data, u32 data_len, char *dirname, u32 *bytes);
bool manage_check_zip_theme(u8 *data, u32 data_len);

bool manage_run(view *sub_view, const char *dirname,
				u8 *data, u32 data_len, u32 bytes);

#endif

