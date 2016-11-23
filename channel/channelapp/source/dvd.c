#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <gccore.h>

#include "../config.h"

static u32 inbuf[8] __attribute__((aligned(0x20)));
static u32 outbuf[8] __attribute__((aligned(0x20)));

static s32 _dvd_fd = -1;

static const char _dvd_path[] __attribute__((aligned(0x20))) = "/dev/di";

typedef enum {
	WDVD_OPEN = 0,
	WDVD_STOP,
	WDVD_CLOSE,
	WDVD_DONE
} wiidvd_state;

static wiidvd_state _state;

static s32 __WiiDVD_Callback(s32 result, void *userdata)
{
	s32 res = -1;

	switch (_state) {
	case WDVD_OPEN:
		_dvd_fd = result;
		if (_dvd_fd < 0)
			return 0;

		memset(inbuf, 0, 0x20);
		inbuf[0x00] = 0xe3000000;
		inbuf[0x01] = 0;
		inbuf[0x02] = 0;

		_state = WDVD_STOP;
		res = IOS_IoctlAsync( _dvd_fd, 0xe3, inbuf, 0x20, outbuf, 0x20,
				__WiiDVD_Callback, NULL);
		break;

	case WDVD_STOP:
		_state = WDVD_CLOSE;
		res = IOS_CloseAsync(_dvd_fd, __WiiDVD_Callback, NULL);
		break;

	case WDVD_CLOSE:
		_dvd_fd = -1;
		_state = WDVD_DONE;
		res = 0;
		break;

	default:
		break;
	}

	return res;
}

s32 WiiDVD_StopMotorAsync(void) {
	_state = WDVD_OPEN;
	
	gprintf("starting DVD motor stop callback chain\n");
	return IOS_OpenAsync(_dvd_path, 0, __WiiDVD_Callback, NULL);
}

void WiiDVD_ShutDown(void) {
	s32 fd = _dvd_fd;

	if (fd > 0) {
		_dvd_fd = -1;
		_state = WDVD_DONE;
		IOS_Close(fd);
		gprintf("killed DVD motor stop callback chain\n");
	}
}

