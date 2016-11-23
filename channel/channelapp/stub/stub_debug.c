/*
 *  Copyright (C) 2008 dhewg, #wiidev efnet
 *
 *  this file is part of the Homebrew Channel
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "stub_debug.h"
#include "usb.h"

#ifdef DEBUG_STUB

static char int2hex[] = "0123456789abcdef";

void debug_uint (u32 i) {
        int j;

        usb_sendbuffersafe ("0x", 2);
        for (j = 0; j < 8; ++j) {
                usb_sendbuffersafe (&int2hex[(i >> 28) & 0xf], 1);
                i <<= 4;
        }
}

void debug_string (const char *s) {
        u32 i = 0;

        while (s[i])
                i++;

        usb_sendbuffersafe (s, i);
}

#endif

