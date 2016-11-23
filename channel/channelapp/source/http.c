#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <ogcsys.h>
#include <network.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/cond.h>

#include "../config.h"

#ifdef ENABLE_UPDATES

#include "tcp.h"
#include "panic.h"

#include "http.h"

extern u32 ms_id;
extern u32 ng_id;

typedef enum {
	HTTPCMD_IDLE = 0,
	HTTPCMD_EXIT,
	HTTPCMD_FETCH
} http_cmd;

typedef struct {
	bool running;
	bool done;

	http_cmd cmd;
	char *host;
	u16 port;
	char *path;
	u32 max_size;

	u16 sysmenu_version;
	char *region;
	char *area;

	mutex_t mutex;
	mutex_t cmutex;
	cond_t cond;
	http_state state;
	http_res res;
	u32 http_status;
	u32 content_length;
	u32 progress;
	u8 *data;
} http_arg;

static lwp_t http_thread;
static u8 http_stack[HTTP_THREAD_STACKSIZE] ATTRIBUTE_ALIGN (32);

static http_arg ta_http;

static u16 get_tmd_version(u64 title) {
	STACK_ALIGN(u8, tmdbuf, 1024, 32);
	u32 tmd_view_size = 0;
	s32 res;

	res = ES_GetTMDViewSize(title, &tmd_view_size);
	if (res < 0)
		return 0;

	if (tmd_view_size > 1024)
		return 0;

	res = ES_GetTMDView(title, tmdbuf, tmd_view_size);
	if (res < 0)
		return 0;

	return (tmdbuf[88] << 8) | tmdbuf[89];
}

static bool http_split_url (char **host, char **path, const char *url) {
	const char *p;
	char *c;

	if (strncasecmp (url, "http://", 7))
		return false;

	p = url + 7;
	c = strchr (p, '/');

	if (c[0] == 0)
		return false;

	*host = strndup (p, c - p);
	*path = pstrdup (c);

	return true;
}

static void * http_func (void *arg) {
	http_arg *ta = (http_arg *) arg;
	int s;
	char *request, *r;
	int linecount;
	char *line;
	bool b;
	s64 t;

	ta->running = true;
	LWP_MutexLock (ta->cmutex);
	LWP_MutexLock (ta_http.mutex);
	ta->state = HTTPS_IDLE;
	ta->done = true;
	LWP_MutexUnlock (ta_http.mutex);

	while (true) {
		while (ta->cmd == HTTPCMD_IDLE)
			LWP_CondWait(ta->cond, ta->cmutex);
		
		if (ta->cmd == HTTPCMD_EXIT)
			break;

		ta->cmd = HTTPCMD_IDLE;

		s = tcp_connect (ta->host, ta->port);

		LWP_MutexLock (ta_http.mutex);
		if (s < 0) {
			ta->state = HTTPS_IDLE;
			ta->done = true;
			ta->res = HTTPR_ERR_CONNECT;
			LWP_MutexUnlock (ta_http.mutex);
			continue;
		}

		ta->state = HTTPS_REQUESTING;
		LWP_MutexUnlock (ta_http.mutex);

		request = (char *) pmalloc (2048);
		r = request;
		r += sprintf (r, "GET %s HTTP/1.1\r\n", ta->path);
		r += sprintf (r, "Host: %s\r\n", ta->host);
		r += sprintf (r, "Cache-Control: no-cache\r\n");
		r += sprintf (r, "User-Agent: TheHomebrewChannel/%s Wii/%08lx"
						" (%lu; %u; %s-%s)\r\n", CHANNEL_VERSION_STR,
						ng_id, ms_id, ta->sysmenu_version, ta->region,
						ta->area);

		strcat (r, "\r\n");

		b = tcp_write (s, (u8 *) request, strlen (request), NULL, NULL);

		free (request);

		if (!b) {
			LWP_MutexLock (ta_http.mutex);
			ta->state = HTTPS_IDLE;
			ta->done = true;
			ta->res = HTTPR_ERR_REQUEST;
			LWP_MutexUnlock (ta_http.mutex);
			net_close (s);
			continue;
		}

		linecount = 0;
		t = gettime ();
		while (true) {
			line = tcp_readln (s, 0xff, t, HTTP_TIMEOUT);
			if (!line) {
				LWP_MutexLock (ta_http.mutex);
				ta->http_status = 404;
				ta->res = HTTPR_ERR_REQUEST;
				break;
			}

			if (strlen (line) < 1) {
				free (line);
				LWP_MutexLock (ta_http.mutex);
				break;
			}

			sscanf (line, "HTTP/1.%*u %lu", &ta->http_status);
			sscanf (line, "Content-Length: %lu", &ta->content_length);

			free (line);

			linecount++;
			if (linecount == 32) {
				LWP_MutexLock (ta_http.mutex);
				ta->http_status = 404;
				ta->res = HTTPR_ERR_REQUEST;
				break;
			}
		}

		if (ta->content_length < 1)
			ta->http_status = 404;

		if (ta->http_status != 200) {
			ta->res = HTTPR_ERR_STATUS;
			ta->state = HTTPS_IDLE;
			ta->done = true;
			LWP_MutexUnlock (ta_http.mutex);
			net_close (s);

			continue;
		}

		if (ta->content_length > ta->max_size) {
			ta->res = HTTPR_ERR_TOOBIG;
			ta->state = HTTPS_IDLE;
			ta->done = true;
			LWP_MutexUnlock (ta_http.mutex);
			net_close (s);

			continue;
		}

		ta->state = HTTPS_RECEIVING;

		// safety, for char[] parsing
		ta->data = (u8 *) pmalloc (ta->content_length + 1);
		LWP_MutexUnlock (ta_http.mutex);

		b = tcp_read (s, ta->data, ta->content_length, &ta->mutex,
						&ta->progress);

		if (!b) {
			LWP_MutexLock (ta_http.mutex);
			free (ta->data);
			ta->data = NULL;
			ta->res = HTTPR_ERR_RECEIVE;
			ta->state = HTTPS_IDLE;
			ta->done = true;
			LWP_MutexUnlock (ta_http.mutex);
			net_close (s);

			continue;
		}

		gprintf("done with download\n");

		LWP_MutexLock (ta_http.mutex);
		ta->data[ta->content_length] = 0;
		ta->res = HTTPR_OK;
		ta->state = HTTPS_IDLE;
		ta->done = true;
		LWP_MutexUnlock (ta_http.mutex);

		net_close (s);
	}

	LWP_MutexUnlock (ta->cmutex);

	gprintf("http thread done\n");

	ta->running = false;

	return NULL;
}

void http_init (void) {
	s32 res;

	gprintf ("starting http thread\n");

	memset (&ta_http, 0, sizeof (http_arg));

	ta_http.sysmenu_version = get_tmd_version(0x0000000100000002ll);

	switch (CONF_GetRegion()) {
	case CONF_REGION_JP:
		ta_http.region = pstrdup("JP");
		break;
	case CONF_REGION_US:
		ta_http.region = pstrdup("US");
		break;
	case CONF_REGION_EU:
		ta_http.region = pstrdup("EU");
		break;
	case CONF_REGION_KR:
		ta_http.region = pstrdup("KR");
		break;
	case CONF_REGION_CN:
		ta_http.region = pstrdup("CN");
		break;
	default:
		ta_http.region = pstrdup("UNK");
		break;
	}

	switch (CONF_GetArea()) {
	case CONF_AREA_JPN:
		ta_http.area = pstrdup("JPN");
		break;
	case CONF_AREA_USA:
		ta_http.area = pstrdup("USA");
		break;
	case CONF_AREA_EUR:
		ta_http.area = pstrdup("EUR");
		break;
	case CONF_AREA_AUS:
		ta_http.area = pstrdup("AUS");
		break;
	case CONF_AREA_BRA:
		ta_http.area = pstrdup("BRA");
		break;
	case CONF_AREA_TWN:
		ta_http.area = pstrdup("TWN");
		break;
	case CONF_AREA_ROC:
		ta_http.area = pstrdup("ROC");
		break;
	case CONF_AREA_KOR:
		ta_http.area = pstrdup("KOR");
		break;
	case CONF_AREA_HKG:
		ta_http.area = pstrdup("HKG");
		break;
	case CONF_AREA_ASI:
		ta_http.area = pstrdup("ASI");
		break;
	case CONF_AREA_LTN:
		ta_http.area = pstrdup("LTN");
		break;
	case CONF_AREA_SAF:
		ta_http.area = pstrdup("SAF");
		break;
	case CONF_AREA_CHN:
		ta_http.area = pstrdup("CHN");
		break;
	default:
		ta_http.area = pstrdup("UNK");
		break;
	}

	res = LWP_MutexInit (&ta_http.mutex, false);

	if (res) {
		gprintf ("error creating mutex: %ld\n", res);
		return;
	}

	res = LWP_MutexInit (&ta_http.cmutex, false);

	if (res) {
		gprintf ("error creating cmutex: %ld\n", res);
		return;
	}

	res = LWP_CondInit (&ta_http.cond);

	if (res) {
		gprintf ("error creating cond: %ld\n", res);
		return;
	}

	memset (&http_stack, 0, HTTP_THREAD_STACKSIZE);
	res = LWP_CreateThread (&http_thread, http_func, &ta_http, http_stack,
							HTTP_THREAD_STACKSIZE, HTTP_THREAD_PRIO);

	gprintf("created http thread\n");

	if (res) {
		gprintf ("error creating thread: %ld\n", res);
	}
}

void http_deinit (void) {
	int i;

	if (ta_http.running) {
		gprintf ("stopping http thread\n");

		for (i = 0; i < 25; ++i) {
			if (LWP_MutexTryLock (ta_http.cmutex) == 0) {
				break;
			}
			usleep (20 * 1000);
		}
		if (i < 25) {
			gprintf ("sending http entry thread the exit cmd\n");
			ta_http.cmd = HTTPCMD_EXIT;
			LWP_SetThreadPriority (http_thread, LWP_PRIO_HIGHEST);
			LWP_CondBroadcast (ta_http.cond);
			LWP_MutexUnlock (ta_http.cmutex);
			LWP_JoinThread(http_thread, NULL);
			LWP_CondDestroy (ta_http.cond);
			LWP_MutexDestroy (ta_http.cmutex);
			LWP_MutexDestroy (ta_http.mutex);
			free(ta_http.region);
			free(ta_http.area);
			return;
		}
		gprintf("http thread didn't shutdown gracefully!\n");
	}

	return;
}

bool http_ready (void) {
	return ta_http.running && ta_http.done;
}

bool http_request (const char *url, u32 max_size) {
	if (!http_ready () || !http_split_url (&ta_http.host, &ta_http.path, url))
		return false;

	LWP_MutexLock (ta_http.cmutex);
	ta_http.done = false;
	ta_http.cmd = HTTPCMD_FETCH;
	ta_http.port = 80;
	ta_http.max_size = max_size;

	LWP_CondBroadcast (ta_http.cond);
	LWP_MutexUnlock (ta_http.cmutex);

	return true;
}

bool http_get_state (http_state *state, u32 *content_length, u32 *progress) {
	if (!ta_http.running)
		return false;

	LWP_MutexLock (ta_http.mutex);
	*state = ta_http.state;
	*content_length = ta_http.content_length;
	*progress = ta_http.progress;
	LWP_MutexUnlock (ta_http.mutex);

	return true;
}

bool http_get_result (http_res *res, u32 *http_status, u8 **content,
						u32 *length) {
	if (!http_ready())
		return false;

	*res = ta_http.res;
	*http_status = ta_http.http_status;
	if (ta_http.res == HTTPR_OK) {
		*content = ta_http.data;
		*length = ta_http.content_length;
	} else {
		*content = NULL;
		*length = 0;
	}

	free (ta_http.host);
	ta_http.host = NULL;
	free (ta_http.path);
	ta_http.path = NULL;

	return true;
}

#endif
