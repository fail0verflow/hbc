#include <sys/types.h>
#include <sys/errno.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <ogcsys.h>
#include <network.h>
#include <ogc/mutex.h>
#include <ogc/lwp_watchdog.h>

#include "../config.h"
#include "panic.h"

#include "tcp.h"

s32 tcp_socket (void) {
	s32 s, res;
	u32 val;

	s = net_socket (PF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		gprintf ("net_socket failed: %d\n", s);
		return s;
	}

	val = 1;
	net_setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));

	res = net_fcntl (s, F_GETFL, 0);
	if (res < 0) {
		gprintf ("F_GETFL failed: %d\n", res);
		net_close (s);
		return res;
	}

	res = net_fcntl (s, F_SETFL, res | 4);
	if (res < 0) {
		gprintf ("F_SETFL failed: %d\n", res);
		net_close (s);
		return res;
	}

	return s;
}

s32 tcp_connect (char *host, u16 port) {
	struct hostent *hp;
	struct sockaddr_in sa;
	s32 s, res;
	s64 t;

	hp = net_gethostbyname (host);
	if (!hp || !(hp->h_addrtype == PF_INET)) {
		gprintf ("net_gethostbyname failed: %d\n", errno);
		return errno;
	}

	s = tcp_socket ();
	if (s < 0)
		return s;

	memset (&sa, 0, sizeof (struct sockaddr_in));
	sa.sin_family= PF_INET;
	sa.sin_len = sizeof (struct sockaddr_in);
	sa.sin_port= htons (port);
	memcpy ((char *) &sa.sin_addr, hp->h_addr_list[0], hp->h_length);

	t = gettime ();
	while (true) {
		if (ticks_to_millisecs (diff_ticks (t, gettime ())) >
				TCP_CONNECT_TIMEOUT) {
			gprintf ("tcp_connect timeout\n");
			net_close (s);

			return -ETIMEDOUT;
		}

		res = net_connect (s, (struct sockaddr *) &sa,
							sizeof (struct sockaddr_in));

		if (res < 0) {
			if (res == -EISCONN)
				break;

			if (res == -EINPROGRESS || res == -EALREADY) {
				usleep (20 * 1000);

				continue;
			}

			gprintf ("net_connect failed: %d\n", res);
			net_close (s);

			return res;
		}

		break;
	}

	return s;
}

s32 tcp_listen (u16 port, s32 backlog) {
	s32 s, res;
	struct sockaddr_in sa;

	s = tcp_socket ();
	if (s < 0)
		return s;

	memset(&sa, 0, sizeof (struct sockaddr_in));
	sa.sin_family = PF_INET;
	sa.sin_port = htons (port);
	sa.sin_addr.s_addr = net_gethostip ();
	sa.sin_len = sizeof (struct sockaddr_in);

	res = net_bind (s, (struct sockaddr *) &sa, sizeof (struct sockaddr_in));
	if (res < 0) {
		gprintf ("net_bind failed: %d\n", res);
		net_close (s);
		return res;
	}

	res = net_listen (s, backlog);
	if (res < 0) {
		gprintf ("net_listen failed: %d\n", res);
		net_close (s);
		return res;
	}

	return s;
}

char * tcp_readln (s32 s, u16 max_length, s64 start_time, u16 timeout) {
	char *buf;
	u16 c;
	s32 res;
	char *ret;

	buf = (char *) pmalloc (max_length);

	c = 0;
	ret = NULL;
	while (true) {
		if (ticks_to_millisecs (diff_ticks (start_time, gettime ())) > timeout)
			break;

		res = net_read (s, &buf[c], 1);

		if ((res == 0) || (res == -EAGAIN)) {
			usleep (20 * 1000);

			continue;
		}

		if (res < 0) {
			gprintf ("tcp_readln failed: %d\n", res);

			break;
		}

		if ((c > 0) && (buf[c - 1] == '\r') && (buf[c] == '\n')) {
			if (c == 1) {
				ret = pstrdup ("");

				break;
			}

			ret = strndup (buf, c - 1);

			break;
		}

		c++;

		if (c == max_length)
			break;
	}

	free (buf);
	return ret;
}

bool tcp_read (s32 s, u8 *buffer, u32 length, const mutex_t *mutex, u32 *progress) {
	u32 step, left, block, received;
	s64 t;
	s32 res;

	step = 0;
	left = length;
	received = 0;

	t = gettime ();
	while (left) {
		if (ticks_to_millisecs (diff_ticks (t, gettime ())) >
				TCP_BLOCK_RECV_TIMEOUT) {
			gprintf ("tcp_read timeout\n");

			break;
		}

		block = left;
		if (block > 2048)
			block = 2048;

		res = net_read (s, buffer, block);

		if ((res == 0) || (res == -EAGAIN)) {
			usleep (20 * 1000);

			continue;
		}

		if (res < 0) {
			gprintf ("net_read failed: %d\n", res);

			break;
		}

		received += res;
		left -= res;
		buffer += res;

		if ((received / TCP_BLOCK_SIZE) > step) {
			t = gettime ();
			step++;
		}

		if (mutex && progress) {
			LWP_MutexLock (*mutex);
			*progress = received;
			LWP_MutexUnlock (*mutex);
		}
	}

	return left == 0;
}

bool tcp_write (s32 s, const u8 *buffer, u32 length, const mutex_t *mutex,
				u32 *progress) {
	const u8 *p;
	u32 step, left, block, sent;
	s64 t;
	s32 res;

	step = 0;
	p = buffer;
	left = length;
	sent = 0;

	t = gettime ();
	while (left) {
		if (ticks_to_millisecs (diff_ticks (t, gettime ())) >
				TCP_BLOCK_SEND_TIMEOUT) {

			gprintf ("tcp_write timeout\n");
			break;
		}

		block = left;
		if (block > 2048)
			block = 2048;

		res = net_write (s, p, block);

		if ((res == 0) || (res == -EAGAIN)) {
			usleep (20 * 1000);
			continue;
		}

		if (res < 0) {
			gprintf ("net_write failed: %d\n", res);
			break;
		}

		sent += res;
		left -= res;
		p += res;

		if ((sent / TCP_BLOCK_SIZE) > step) {
			t = gettime ();
			step++;
		}

		if (mutex && progress) {
			LWP_MutexLock (*mutex);
			*progress = sent;
			LWP_MutexUnlock (*mutex);
		}
	}

	return left == 0;
}

