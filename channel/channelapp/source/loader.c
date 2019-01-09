#include <sys/errno.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <ogcsys.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/cond.h>
#include <zlib.h>

#include "../config.h"
#include "blob.h"
#include "tcp.h"
#include "appentry.h"
#include "manage.h"
#include "dialogs.h"
#include "i18n.h"
#include "panic.h"

#include "loader.h"

#define USBGECKO_RETRIES 1000

static const char *text_err_read;
static const char *text_err_receive;
static const char *text_err_uncompress;
static const char *text_err_invalid_bin;
static const char *text_extract_zip;
static const char *text_err_invalid_zip;
static const char *text_err_extract_zip;
static const char *text_warn_overwrite;
static const char *text_err_oom;

static inline u16 buf_u16(const u8 *buf, u8 pos) {
	return (buf[pos] << 8) | buf[pos + 1];
}

static inline u32 buf_u32(const u8 *buf, u8 pos) {
	return (buf[pos] << 24) | (buf[pos + 1] << 16) |
			(buf[pos + 2] << 8) | buf[pos + 3];
}

// gecko handshake thread

static lwp_t ld_gecko_thread;
static u8 ld_gecko_stack[LD_THREAD_STACKSIZE] ATTRIBUTE_ALIGN (32);

typedef enum {
	LDGECKOCMD_IDLE = 0,
	LDGECKOCMD_EXIT,
	LDGECKOCMD_POLL,

} ld_gecko_cmd;

typedef struct {
	bool running;
	bool handshaked;
	ld_gecko_cmd cmd;
	u32 data_len;
	u32 data_len_un;
	u16 args_len;
	mutex_t cmutex;
	cond_t cond;
} ld_gecko_arg;

static void * ld_gecko_func (void *arg) {
	ld_gecko_arg *ta = (ld_gecko_arg *) arg;
	u8 buf[16];
	u8 left;
	int res;
	s64 t;
	u16 wiiload_version;

	ta->running = true;

	LWP_MutexLock (ta->cmutex);
	while (true) {
		usb_flush(USBGECKO_CHANNEL);

		while (ta->cmd == LDGECKOCMD_IDLE)
			LWP_CondWait(ta->cond, ta->cmutex);

		if (ta->cmd == LDGECKOCMD_EXIT)
			break;

		ta->cmd = LDGECKOCMD_IDLE;

		ta->handshaked = false;

		left = 16;
		t = gettime ();
		while (left) {
			res = usb_recvbuffer_safe_ex(USBGECKO_CHANNEL, &buf[16 - left],
										left, USBGECKO_RETRIES);

			if (res) {
				left -= res;

				continue;
			}

			if (ticks_to_millisecs (diff_ticks (t, gettime ())) > LD_TIMEOUT)
				break;
		}

		if (left)
			continue;

		wiiload_version = buf_u16(buf, 4);
		ta->args_len = buf_u16(buf, 6);
		ta->data_len = buf_u32(buf, 8);
		ta->data_len_un = buf_u32(buf, 12);

		if (strncmp((char *) buf, "HAXX", 4) ||
				(wiiload_version < WIILOAD_MIN_VERSION) ||
				(ta->args_len > ARGS_MAX_LEN) ||
				(!ta->data_len || ta->data_len > LD_MAX_SIZE) ||
				(ta->data_len_un > LD_MAX_SIZE)) {
			continue;
		}

		gprintf_enable(0);

		ta->handshaked = true;
	}

	LWP_MutexUnlock (ta->cmutex);

	gprintf("gecko handshake thread done\n");

	ta->running = false;
	return NULL;
}

// tcp handshake thread

static lwp_t ld_tcp_thread;
static u8 ld_tcp_stack[LD_THREAD_STACKSIZE] ATTRIBUTE_ALIGN (32);

typedef enum {
	LDTCPCMD_IDLE = 0,
	LDTCPCMD_EXIT,
	LDTCPCMD_INIT,
	LDTCPCMD_ACCEPT
} ld_tcp_cmd;

typedef enum {
	LDTCPS_UNINITIALIZED = 0,
	LDTCPS_INITIALIZING,
	LDTCPS_INITIALIZED
} ld_tcp_state;

typedef struct {
	bool running;
	ld_tcp_cmd cmd;
	ld_tcp_state state;
	bool handshaked;
	u16 args_len;
	u32 data_len;
	u32 data_len_un;
	int s;
	char *client;
	mutex_t cmutex;
	cond_t cond;
} ld_tcp_arg;

static void * ld_tcp_func (void *arg) {
	ld_tcp_arg *ta = (ld_tcp_arg *) arg;
	int s = -1, sn;
	struct sockaddr_in sa;
	u32 mask, len_sa;
	s32 res;
	u8 buf[16];
	u16 wiiload_version;
	u8 retries;

	mask = 0;

	ta->running = true;
	LWP_MutexLock (ta->cmutex);

	ta->cmd = LDTCPCMD_INIT;
	ta->state = LDTCPS_UNINITIALIZED;
	ta->handshaked = false;

	while (true) {
		while (ta->cmd == LDTCPCMD_IDLE)
			LWP_CondWait(ta->cond, ta->cmutex);

		ta->handshaked = false;

		if (ta->cmd == LDTCPCMD_EXIT) {
			break;
		}

		if (ta->cmd == LDTCPCMD_INIT) {
			ta->cmd = LDTCPCMD_IDLE;

			if (ta->state == LDTCPS_INITIALIZED) {
				gprintf ("reinit net\n");
				net_close (s);
			}

			retries = 32;
			ta->state = LDTCPS_INITIALIZING;

			while (retries) {
				res = net_init_async(NULL, NULL);

				if (res) {
					gprintf("net_init_async failed: %d\n", res);
					break;
				}

				res = net_get_status();
				while (res == -EBUSY) {
					if (ta->cmd == LDTCPCMD_EXIT) {
						gprintf("exit while net_init_async still busy\n");
						res = -1;
						break;
					}

					usleep(50 * 1000);
					res = net_get_status();
				}

				if ((res == -EAGAIN) || (res == -ETIMEDOUT)) {
					gprintf ("net_init failed: %d, trying again...\n", res);
					retries--;
					usleep(50 * 1000);
					continue;
				}

				break;
			}

			if (res < 0) {
				gprintf ("net_init failed: %d\n", res);
				ta->state = LDTCPS_UNINITIALIZED;
				continue;
			}

			gprintf ("net_init success: %d\n", res);

			mask = net_gethostip () & 0xffff0000;

			s = tcp_listen (LD_TCP_PORT, 3);

			if ((s == -ENETRESET) && retries) {
				gprintf("ENETRESET, reiniting\n");
				net_deinit();
				ta->cmd = LDTCPCMD_INIT;
				continue;
			}

			if (s < 0) {
				gprintf ("tcp_listen failed: %d\n",s);
				ta->state = LDTCPS_UNINITIALIZED;
				continue;
			}

			ta->state = LDTCPS_INITIALIZED;

			continue;
		}

		if (ta->cmd == LDTCPCMD_ACCEPT) {
			ta->cmd = LDTCPCMD_IDLE;
			memset (&sa, 0, sizeof (struct sockaddr_in));
			sa.sin_family = AF_INET;
			sa.sin_len = sizeof (struct sockaddr_in);

			len_sa = sizeof (struct sockaddr_in);

			sn = net_accept (s, (struct sockaddr *) &sa, &len_sa);
			if (sn == -EAGAIN)
				continue;

			if (sn == -ENETRESET) {
				gprintf("ENETRESET, reiniting\n");
				net_deinit();
				ta->cmd = LDTCPCMD_INIT;
				continue;
			}

			if (sn < 0) {
				gprintf ("net_accept failed: %d\n", sn);

				net_close (s);
				ta->state = LDTCPS_UNINITIALIZED;

				continue;
			}

			if ((sa.sin_addr.s_addr & 0xffff0000) != mask) {
				gprintf ("non local ip (%x)\n", sa.sin_addr.s_addr);
				net_close (sn);
				continue;
			}

			if (!tcp_read (sn, buf, 16, NULL, NULL)) {
				net_close (sn);
				continue;
			}

			wiiload_version = buf_u16(buf, 4);
			ta->args_len = buf_u16(buf, 6);
			ta->data_len = buf_u32(buf, 8);
			ta->data_len_un = buf_u32(buf, 12);

			if (strncmp((char *) buf, "HAXX", 4) ||
					(wiiload_version < WIILOAD_MIN_VERSION) ||
					(ta->args_len > ARGS_MAX_LEN) ||
					(!ta->data_len || ta->data_len > LD_MAX_SIZE) ||
					(ta->data_len_un > LD_MAX_SIZE)) {
				gprintf ("invalid upload request\n");
				net_close (sn);
				continue;
			}

			ta->s = sn;
			ta->client = inet_ntoa (sa.sin_addr);
			ta->handshaked = true;

			continue;
		}
	}

	if (ta->state == LDTCPS_INITIALIZED) {
		gprintf ("net_shutdown\n");
		res = net_shutdown (s, 2);
		if (res)
			gprintf ("net_shutdown failed: %d\n", res);

		gprintf ("net_close\n");
		res = net_close (s);
		if (res)
			gprintf ("net_close failed: %d\n", res);
	}

	gprintf ("tcp thread deiniting\n");
	net_deinit();
	gprintf ("tcp thread exiting\n");
	ta->state = LDTCPS_UNINITIALIZED;

	ta->running = false;
	LWP_MutexUnlock (ta->cmutex);

	return NULL;
}

// thread setup

static ld_gecko_arg ta_gecko;
static ld_tcp_arg ta_tcp;

void loader_init (void) {
	int res;

	if (usb_isgeckoalive (USBGECKO_CHANNEL)) {
		gprintf ("starting gecko thread\n");

		memset (&ta_gecko, 0, sizeof (ld_gecko_arg));

		memset (&ld_gecko_stack, 0, LD_THREAD_STACKSIZE);

		res = LWP_MutexInit (&ta_gecko.cmutex, false);
		if (res) {
			gprintf ("error creating cmutex: %d\n", res);
			return;
		}

		res = LWP_CondInit (&ta_gecko.cond);
		if (res) {
			gprintf ("error creating cond: %d\n", res);
			return;
		}

		res = LWP_CreateThread (&ld_gecko_thread, ld_gecko_func, &ta_gecko,
								ld_gecko_stack, LD_THREAD_STACKSIZE,
								LD_THREAD_PRIO);

		if (res) {
			gprintf ("error creating thread: %d\n", res);
		}
	}

	gprintf ("starting tcp thread\n");

	memset (&ta_tcp, 0, sizeof (ld_tcp_arg));

	memset (&ld_tcp_stack, 0, LD_THREAD_STACKSIZE);
	res = LWP_MutexInit (&ta_tcp.cmutex, false);
	if (res) {
		gprintf ("error creating cmutex: %d\n", res);
		return;
	}
	res = LWP_CondInit (&ta_tcp.cond);
	if (res) {
		gprintf ("error creating cond: %d\n", res);
		return;
	}
	res = LWP_CreateThread (&ld_tcp_thread, ld_tcp_func, &ta_tcp, ld_tcp_stack,
							LD_THREAD_STACKSIZE, LD_THREAD_PRIO);
	if (res) {
		gprintf ("error creating thread: %d\n", res);
	}
}

void loader_deinit (void) {
	u8 i;

	// the tcp thread does stuff on exit; the ideal order is stopping it first
	if (ta_tcp.running) {
		gprintf ("stopping tcp thread\n");

		for (i = 0; i < 25; ++i) {
			if (LWP_MutexTryLock (ta_tcp.cmutex) == 0)
				break;
			usleep (20 * 1000);
		}
		if (i >= 25) {
			gprintf("tcp thread didn't shutdown gracefully!\n");
		} else {
			gprintf ("sending tcp entry thread the exit cmd\n");
			ta_tcp.cmd = LDTCPCMD_EXIT;
			LWP_SetThreadPriority (ld_tcp_thread, LWP_PRIO_HIGHEST);
			LWP_CondBroadcast (ta_tcp.cond);
			LWP_MutexUnlock (ta_tcp.cmutex);

			for (i = 0; i < 25; ++i) {
				if (LWP_MutexTryLock (ta_tcp.cmutex) == 0) {
					if (!ta_tcp.running)
						break;
					LWP_MutexUnlock (ta_tcp.cmutex);
				}
				usleep (20 * 1000);
			}
			if (i >= 25) {
				gprintf("tcp thread didn't shutdown gracefully!\n");
			} else {
				LWP_MutexUnlock (ta_tcp.cmutex);
				LWP_JoinThread(ld_tcp_thread, NULL);
				LWP_CondDestroy (ta_tcp.cond);
				LWP_MutexDestroy (ta_tcp.cmutex);
			}
		}
	}

	if (ta_gecko.running) {
		gprintf ("stopping gecko thread\n");

		for (i = 0; i < 25; ++i) {
			if (LWP_MutexTryLock (ta_gecko.cmutex) == 0)
				break;
			usleep (20 * 1000);
		}
		if (i < 25) {
			gprintf ("sending gecko entry thread the exit cmd\n");
			ta_gecko.cmd = LDGECKOCMD_EXIT;
			LWP_SetThreadPriority (ld_gecko_thread, LWP_PRIO_HIGHEST);
			LWP_CondBroadcast (ta_gecko.cond);
			LWP_MutexUnlock (ta_gecko.cmutex);
			LWP_JoinThread(ld_gecko_thread, NULL);
			LWP_CondDestroy (ta_gecko.cond);
			LWP_MutexDestroy (ta_gecko.cmutex);
		} else {
			gprintf("gecko thread didn't shutdown gracefully!\n");
		}
	}

}

void loader_tcp_init (void) {
	if (LWP_MutexTryLock(ta_tcp.cmutex) != 0)
		return;

	if (ta_tcp.running && ta_tcp.state == LDTCPS_UNINITIALIZED) {
		ta_tcp.cmd = LDTCPCMD_INIT;
		LWP_CondBroadcast(ta_tcp.cond);
	}
	LWP_MutexUnlock(ta_tcp.cmutex);
}

void loader_signal_threads (void) {
	if (LWP_MutexTryLock(ta_gecko.cmutex) == 0) {
		if (loader_gecko_initialized () && !ta_gecko.handshaked) {
			ta_gecko.cmd = LDGECKOCMD_POLL;
			LWP_CondBroadcast(ta_gecko.cond);
		}
		LWP_MutexUnlock(ta_gecko.cmutex);
	}

	if (LWP_MutexTryLock(ta_tcp.cmutex) == 0) {
		if (loader_tcp_initialized () && !ta_tcp.handshaked) {
			ta_tcp.cmd = LDTCPCMD_ACCEPT;
			LWP_CondBroadcast(ta_tcp.cond);
		}
		LWP_MutexUnlock(ta_tcp.cmutex);
	}
}

bool loader_gecko_initialized (void) {
	return ta_gecko.running;
}

bool loader_tcp_initializing (void) {
	return ta_tcp.running && ta_tcp.state == LDTCPS_INITIALIZING;
}

bool loader_tcp_initialized (void) {
	return ta_tcp.running && ta_tcp.state == LDTCPS_INITIALIZED;
}

bool loader_handshaked (void) {
	bool handshaked = false;
	if (LWP_MutexTryLock(ta_gecko.cmutex) == 0) {
		handshaked = handshaked || (ta_gecko.running && ta_gecko.handshaked);
		LWP_MutexUnlock(ta_gecko.cmutex);
	}
	if (LWP_MutexTryLock(ta_tcp.cmutex) == 0) {
		handshaked = handshaked || (ta_tcp.running && ta_tcp.handshaked);
		LWP_MutexUnlock(ta_tcp.cmutex);
	}

	return handshaked;
}

// loading thread

static lwp_t ld_load_thread;
static u8 ld_load_stack[LD_THREAD_STACKSIZE] ATTRIBUTE_ALIGN (32);

typedef enum {
	LDC_FILE = 0,
	LDC_GECKO,
	LDC_TCP
} ld_load_cmd;

typedef enum {
	LDS_RUNNING = 0,
	LDS_SUCCESS,
	LDS_ERR_READ,
	LDS_ERR_RECEIVE,
	LDS_ERR_UNCOMPRESS
} ld_load_state;

typedef struct {
	ld_load_cmd cmd;

	u32 data_len;
	u8 *data;

	u32 data_len_un;
	u8 *data_un;

	u16 args_len;

	int fd;

	mutex_t mutex;
	ld_load_state state;
	u32 progress;
} ld_load_arg;

static void * ld_load_func (void *arg) {
	ld_load_arg *ta = (ld_load_arg *) arg;
	u8 *d;
	u32 left, received;
	s32 block;
	int res;
	s64 t;

	d = ta->data;
	received = 0;
	left = ta->data_len + ta->args_len;

	switch (ta->cmd) {
	case LDC_FILE:
		while (left) {
			block = left;
			if (block > 32 * 1024)
				block = 32 * 1024;

			block = read (ta->fd, d, block);

			if (block < 0) {
				gprintf ("read failed: %d\n", block);
				close (ta->fd);

				LWP_MutexLock (ta->mutex);
				ta->state = LDS_ERR_READ;
				LWP_MutexUnlock (ta->mutex);

				return NULL;
			} else {
				d += block;
				received += block;
				left -= block;

				LWP_MutexLock (ta->mutex);
				ta->progress = received;
				LWP_MutexUnlock (ta->mutex);
			}
		}

		close (ta->fd);

		break;

	case LDC_GECKO:
		block = 0;
		t = gettime ();

		while (left) {
			res = usb_recvbuffer_ex(USBGECKO_CHANNEL, d, left, USBGECKO_RETRIES);

			if (res) {
				d += res;
				received += res;
				left -= res;
				block += res;

				if (block >= 1024) {
					block = 0;

					LWP_MutexLock (ta->mutex);
					ta->progress = received;
					LWP_MutexUnlock (ta->mutex);

					t = gettime ();
				}
			}

			if (ticks_to_millisecs (diff_ticks (t, gettime ())) > LD_TIMEOUT) {
				LWP_MutexLock (ta->mutex);
				ta->state = LDS_ERR_RECEIVE;
				LWP_MutexUnlock (ta->mutex);

				gprintf_enable(1);

				return NULL;
			}
		}

		gprintf_enable(1);

		break;

	case LDC_TCP:
		if (!tcp_read (ta->fd, ta->data, left, &ta->mutex, &ta->progress)) {
			LWP_MutexLock (ta->mutex);
			ta->state = LDS_ERR_RECEIVE;
			LWP_MutexUnlock (ta->mutex);

			net_close (ta->fd);

			return NULL;
		}

		net_close (ta->fd);

		break;
	}

	if (ta->data_un) {
		int res;
		uLongf len = ta->data_len_un;

		res = uncompress(ta->data_un, &len, ta->data, ta->data_len);

		if (res != Z_OK) {
			gprintf("error uncompressing: %d\n", res);

			LWP_MutexLock (ta->mutex);
			ta->state = LDS_ERR_UNCOMPRESS;
			LWP_MutexUnlock (ta->mutex);

			return NULL;
		}

		if (len != ta->data_len_un) {
			gprintf("short uncompress: %u\n", (u32) len);

			LWP_MutexLock (ta->mutex);
			ta->state = LDS_ERR_UNCOMPRESS;
			LWP_MutexUnlock (ta->mutex);

			return NULL;
		}
	}

	LWP_MutexLock (ta->mutex);
	ta->state = LDS_SUCCESS;
	LWP_MutexUnlock (ta->mutex);

	return NULL;
}

// public loading function

void loader_load(loader_result *result, view *sub_view, app_entry *entry) {
	char caption[PATH_MAX + 32];
	char filename[PATH_MAX];

	ld_load_arg ta;
	s32 res;
	bool running;
	u32 progress;

	view *v;

	text_err_read = _("Error while reading the application from the SD card");
	text_err_receive = _("Error while receiving the application");
	text_err_uncompress = _("Error uncompressing the received data");
	text_err_invalid_bin = _("This is not a valid Wii application");
	text_err_invalid_zip = _("This is not a usable ZIP file");
	text_err_oom = _("Not enough memory");

	memset(result, 0, sizeof (loader_result));
	memset(&ta, 0, sizeof (ld_load_arg));
	memset(filename, 0, sizeof(filename));

	bool gecko_handshaked = false;
	bool tcp_handshaked = false;

	if (LWP_MutexTryLock (ta_gecko.cmutex) == 0) {
		gecko_handshaked = ta_gecko.handshaked;
		ta_gecko.cmd = LDGECKOCMD_IDLE;
		LWP_MutexUnlock (ta_gecko.cmutex);
	}

	if (LWP_MutexTryLock (ta_tcp.cmutex) == 0) {
		tcp_handshaked = ta_tcp.handshaked;
		ta_gecko.cmd = LDTCPCMD_IDLE;
		LWP_MutexUnlock (ta_tcp.cmutex);
	}

	if (entry) {
		char *name;

		if (!app_entry_get_filename(filename, entry))
			return;

		gprintf ("loading %s\n", filename);

		ta.cmd = LDC_FILE;

		ta.fd = open (filename, O_RDONLY);
		if (ta.fd < 0)
			return;

		ta.data_len = entry->size;

		if (entry->meta && entry->meta->name)
			name = entry->meta->name;
		else
			name = entry->dirname;

		sprintf (caption, _("Loading %s"), name);
	} else if (gecko_handshaked) {
		ta.cmd = LDC_GECKO;
		ta_gecko.handshaked = false;

		ta.data_len = ta_gecko.data_len;
		ta.data_len_un = ta_gecko.data_len_un;
		ta.args_len = ta_gecko.args_len;

		sprintf (caption, _("Receiving over USBGecko"));
	} else if (tcp_handshaked) {
		ta.cmd = LDC_TCP;
		ta_tcp.handshaked = false;

		ta.data_len = ta_tcp.data_len;
		ta.data_len_un = ta_tcp.data_len_un;
		ta.args_len = ta_tcp.args_len;
		ta.fd = ta_tcp.s;

		sprintf (caption, _("Receiving from %s"), ta_tcp.client);
	} else {
		return;
	}

	if (ta.data_len == ta.data_len_un)
		ta.data_len_un = 0;

	if (ta.data_len_un) {
		ta.data_un = (u8 *) blob_alloc(ta.data_len_un);

		if (!ta.data_un) {
			if (ta.cmd == LDC_TCP)
				net_close (ta.fd);
			show_message (sub_view, DLGMT_ERROR, DLGB_OK, text_err_oom, 0);
			return;
		}
	}

	ta.data = (u8 *) blob_alloc(ta.data_len + ta.args_len);

	if (!ta.data) {
		blob_free(ta.data_un);
		if (ta.cmd == LDC_TCP)
			net_close (ta.fd);
		show_message (sub_view, DLGMT_ERROR, DLGB_OK, text_err_oom, 0);
		return;
	}


	res = LWP_MutexInit (&ta.mutex, false);

	if (res) {
		gprintf ("error creating mutex: %d\n", res);
		blob_free (ta.data);
		blob_free (ta.data_un);
		if (ta.cmd == LDC_TCP)
			net_close (ta.fd);
		panic(); // if this happens, let's find out
		return;
	}

	memset (&ld_load_stack, 0, LD_THREAD_STACKSIZE);
	res = LWP_CreateThread (&ld_load_thread, ld_load_func, &ta, ld_load_stack,
							LD_THREAD_STACKSIZE, LD_THREAD_PRIO);

	if (res) {
		gprintf ("error creating thread: %d\n", res);
		blob_free (ta.data);
		blob_free (ta.data_un);
		if (ta.cmd == LDC_TCP)
			net_close (ta.fd);
		panic(); // if this happens, let's find out
		return;
	}

	v = dialog_progress (sub_view, caption, ta.data_len + ta.args_len);

	dialog_fade (v, true);

	running = true;
	while (running) {
		LWP_MutexLock (ta.mutex);
		running = ta.state == LDS_RUNNING;
		progress = ta.progress;
		LWP_MutexUnlock (ta.mutex);

		dialog_set_progress (v, progress);

		view_plot (v, DIALOG_MASK_COLOR, NULL, NULL, NULL);
	}

	dialog_fade (v, false);

	view_free (v);

	LWP_MutexDestroy (ta.mutex);

	if (ta.data_len_un) {
		if (ta.args_len) {
			memcpy(result->args, &ta.data[ta.data_len], ta.args_len);
			result->args_len = ta.args_len;
		}

		blob_free(ta.data);
		ta.data = ta.data_un;
		ta.data_len = ta.data_len_un;
		ta.data_un = NULL;
		ta.data_len_un = 0;
	}

	const char *text = NULL;

	switch (ta.state) {
	case LDS_RUNNING:
	case LDS_SUCCESS:
		break;
	case LDS_ERR_READ:
		text = text_err_read;
		break;
	case LDS_ERR_RECEIVE:
		text = text_err_receive;
		break;
	case LDS_ERR_UNCOMPRESS:
		text = text_err_uncompress;
		break;
	}

	if (text) {
		blob_free(ta.data);
		show_message (sub_view, DLGMT_ERROR, DLGB_OK, text, 0);
		return;
	}

	result->data = ta.data;
	result->data_len = ta.data_len;

	if (ta.cmd == LDC_FILE) {
		switch(entry->type) {
		case AET_BOOT_ELF:
		case AET_BOOT_DOL:
			break;

		case AET_THEME:
			result->type = LT_ZIP_THEME;
			return;
		}

		result->type = LT_EXECUTABLE;

		strcpy(result->args, filename);
		result->args_len = strlen (result->args);

		if (entry->meta && entry->meta->args) {
			result->args_len++;
			memcpy(result->args + result->args_len,
					entry->meta->args, entry->meta->argslen);
			result->args_len += entry->meta->argslen;
		}

		result->args[result->args_len + 1] = 0;
		result->args_len += 2;

		return;
	}

	if (manage_is_zip(ta.data)) {
		gprintf("we got a .zip\n");

		if (manage_check_zip_app(ta.data, ta.data_len, result->dirname,
									&result->bytes))
			result->type = LT_ZIP_APP;
		else if (manage_check_zip_theme(ta.data, ta.data_len))
			result->type = LT_ZIP_THEME;

		if (result->type == LT_UNKNOWN) {
			blob_free(ta.data);
			ta.data = NULL;
			ta.data_len = 0;
			show_message (sub_view, DLGMT_ERROR, DLGB_OK,
							text_err_invalid_zip, 0);
		}

		return;
	}

	result->type = LT_EXECUTABLE;
}

bool loader_load_executable(entry_point *ep, loader_result *result,
							view *sub_view) {
	bool res = loader_reloc(ep, result->data, result->data_len,
							result->args, result->args_len, true);

	if (!res)
		show_message (sub_view, DLGMT_ERROR, DLGB_OK, text_err_invalid_bin, 0);

	blob_free(result->data);

	return res;
}

bool loader_handle_zip_app(loader_result *result, view *sub_view) {
	char buf[1024];
	char buf2[1024];

	text_extract_zip = _("Extract the received ZIP file?\n%s of free space are required.");
	text_warn_overwrite = _("WARNING: Files in '%s' will be overwritten");
	text_err_extract_zip = _("Error while extracting the ZIP file");

	if ((result->bytes / 1024u) > 999)
		sprintf(buf2, "%.02f MB",
				(float)(result->bytes) / (float)(1024 * 1024));
	else
		sprintf(buf2, "%u KB", result->bytes / 1024u);
	sprintf(buf, text_extract_zip, buf2);

	if (!app_entry_get_path(buf2)) {
		blob_free(result->data);
		return false;
	}

	strcat(buf2, ":/");
	strcat(buf2, result->dirname);
	if (dir_exists(buf2)) {
		strcat(buf, "\n\n");
		sprintf(buf2, text_warn_overwrite, result->dirname);
		strcat(buf, buf2);
	}

	if (show_message(sub_view, DLGMT_CONFIRM, DLGB_YESNO,
					buf, 0) != 0) {
		blob_free(result->data);
		return false;
	}

	if (!manage_run(sub_view, result->dirname, result->data,
					result->data_len, result->bytes)) {
		show_message (sub_view, DLGMT_ERROR, DLGB_OK, text_err_extract_zip, 0);
		blob_free(result->data);
		return false;
	}

	blob_free(result->data);

	return true;
}

