#ifndef _ECDSA_H_
#define _ECDSA_H_

#include <ogcsys.h>

int check_ecdsa(const u8 *Q, u8 *R, u8 *S, u8 *hash);

#endif
