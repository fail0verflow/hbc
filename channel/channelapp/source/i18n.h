#ifndef _I18N_H_
#define _I18N_H_

#include <gctypes.h>

#define _(x) i18n(x)

void i18n_set_mo(const void *mo_file);
const char *i18n(char *english_string);
u32 i18n_have_mo(void);

#endif

