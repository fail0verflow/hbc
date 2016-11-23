#include <gccore.h>
#include <ogc/conf.h>
#include <ogc/lwp_watchdog.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../config.h"
#include "playtime.h"

static char _playtime_path[] __attribute__((aligned(32))) =
	"/title/00000001/00000002/data/play_rec.dat";

void playtime_destroy(void) {
	s32 res;
	s32 pt_fd = -1;
	static u8 pt_buf[4] __attribute__((aligned(32)));

	gprintf("destroying playtime\n");

	pt_fd = IOS_Open(_playtime_path, IPC_OPEN_RW);
	if(pt_fd < 0) {
		gprintf("playtime open failed: %ld\n", pt_fd);
		return;
	}

	memset(pt_buf, 0, sizeof(pt_buf));

	res = IOS_Write(pt_fd, &pt_buf, sizeof(pt_buf));
	if (res != sizeof(pt_buf)) {
		IOS_Close(pt_fd);
		gprintf("error destroying playtime (%ld)\n", res);
		return;
	}

	IOS_Close(pt_fd);
}

