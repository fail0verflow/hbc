#ifndef _TCP_H_
#define _TCP_H_

#include <gctypes.h>
#include <ogc/mutex.h>

s32 tcp_socket (void);
s32 tcp_connect (char *host, u16 port);
s32 tcp_listen (u16 port, s32 backlog);

char * tcp_readln (s32 s, u16 max_length, s64 start_time, u16 timeout);
bool tcp_read (s32 s, u8 *buffer, u32 length, const mutex_t *mutex, u32 *progress);
bool tcp_write (s32 s, const u8 *buffer, u32 length, const mutex_t *mutex,
				u32 *progress);

#endif

