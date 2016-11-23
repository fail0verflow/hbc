#ifndef _HTTP_H_
#define _HTTP_H_

#include <gctypes.h>

typedef enum {
	HTTPS_IDLE = 0,
	HTTPS_CONNECTING,
	HTTPS_REQUESTING,
	HTTPS_RECEIVING
} http_state;

typedef enum {
	HTTPR_OK,
	HTTPR_ERR_CONNECT,
	HTTPR_ERR_REQUEST,
	HTTPR_ERR_STATUS,
	HTTPR_ERR_TOOBIG,
	HTTPR_ERR_RECEIVE
} http_res;

void http_init (void);
void http_deinit (void);

bool http_ready (void);

bool http_request (const char *url, u32 max_size);
bool http_get_state (http_state *state, u32 *content_length, u32 *progress);
bool http_get_result (http_res *res, u32 *http_status, u8 **content,
						u32 *length);

#endif

