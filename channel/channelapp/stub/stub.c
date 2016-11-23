/*
 *  Copyright (C) 2008 dhewg, #wiidev efnet
 *  Copyright (C) 2008 marcan, #wiidev efnet
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

#include "system.h"
#include "stub_debug.h"
#include "usb.h"
#include "ios.h"
#include "cache.h"
#include "../config.h"

#define IOCTL_ES_LAUNCH					0x08
#define IOCTL_ES_GETVIEWCNT				0x12
#define IOCTL_ES_GETVIEWS				0x13

struct ioctlv vecs[3] __attribute__((aligned(32)));

u64 *conf_magic = STUB_ADDR_MAGIC;
u64 *conf_titleID = STUB_ADDR_TITLE;

int es_fd;

int LaunchTitle(u64 titleID) {
	static u64 xtitleID __attribute__((aligned(32)));
	static u32 cntviews __attribute__((aligned(32)));
	static u8 tikviews[0xd8*4] __attribute__((aligned(32)));
	int ret;
	
	debug_string("LaunchTitle: ");
	debug_uint(titleID>>32);
	debug_string("-");
	debug_uint(titleID&0xFFFFFFFF);
	
	xtitleID = titleID;
	
	debug_string("\n\rGetTicketViewCount: ");
	
	vecs[0].data = &xtitleID;
	vecs[0].len = 8;
	vecs[1].data = &cntviews;
	vecs[1].len = 4;
	ret = ios_ioctlv(es_fd, IOCTL_ES_GETVIEWCNT, 1, 1, vecs);
	debug_uint(ret);
	debug_string(", views: ");
	debug_uint(cntviews);
	debug_string("\n\r");
	if(ret<0) return ret;
	if(cntviews>4) return -1;
		
	debug_string("GetTicketViews: ");
	vecs[0].data = &xtitleID;
	vecs[0].len = 8;
	vecs[1].data = &cntviews;
	vecs[1].len = 4;
	vecs[2].data = tikviews;
	vecs[2].len = 0xd8*cntviews;
	ret = ios_ioctlv(es_fd, IOCTL_ES_GETVIEWS, 2, 1, vecs);
	debug_uint(ret);
	debug_string("\n\r");
	if(ret<0) return ret;
	debug_string("Attempting to launch...\n\r");
	vecs[0].data = &xtitleID;
	vecs[0].len = 8;
	vecs[1].data = tikviews;
	vecs[1].len = 0xd8;
	ret = ios_ioctlvreboot(es_fd, IOCTL_ES_LAUNCH, 2, 0, vecs);
	if(ret < 0) {
		debug_string("Launch failed: ");
		debug_uint(ret);
		debug_string("\r\n");
	}
	return ret;
}

s32 IOS_GetVersion()
{
	u32 vercode;
	u16 version;
	DCInvalidateRange((void*)0x80003140,8);
	vercode = *((u32*)0x80003140);
	version = vercode >> 16;
	if(version == 0) return -1;
	if(version > 0xff) return -1;
	return version;
}

void printversion(void) {
	debug_string("IOS version: ");
	debug_uint(IOS_GetVersion());
	debug_string("\n\r");
}

int es_init(void) {
	debug_string("Opening /dev/es: ");
	es_fd = ios_open("/dev/es", 0);
	debug_uint(es_fd);
	debug_string("\n\r");
	return es_fd;
}


void _main (void) {
		int iosver;
		u64 titleID = MY_TITLEID;
		
        debug_string("\n\rHomebrew Channel stub code\n\r");
		
		if(*conf_magic == STUB_MAGIC) titleID = *conf_titleID;
			
		reset_ios();

		if(es_init() < 0) goto fail;

		iosver = STUB_LOAD_IOS_VERSION;
		if(iosver < 0)
			iosver = 21; //bah
		printversion();
		debug_string("\n\rReloading IOS...\n\r");
		LaunchTitle(0x0000000100000000LL | iosver);
		printversion();

		if(es_init() < 0) goto fail;
		debug_string("\n\rLoading requested channel...\n\r");
		LaunchTitle(titleID);
		// if fail, try system menu
		debug_string("\n\rChannel load failed, trying with system menu...\n\r");
		LaunchTitle(0x0000000100000002LL);
		printversion();

fail:
		debug_string("FAILURE\n\r");
		while(1);
}

