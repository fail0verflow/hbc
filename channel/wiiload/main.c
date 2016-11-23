/*
 *	Copyright (C) 2008 dhewg, #wiidev efnet
 *
 *	this file is part of geckoloader
 *	http://wiibrew.org/index.php?title=Geckoloader
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#else
#include <winsock2.h>
#endif

#include <zlib.h>

#include "gecko.h"

#define WIILOAD_VERSION_MAYOR 0
#define WIILOAD_VERSION_MINOR 5

#define LD_TCP_PORT 4299

#define MAX_ARGS_LEN 1024

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef enum { false, true } bool;

#ifndef __WIN32__
static const char *desc_export = "export";
#ifndef __APPLE__
static const char *desc_gecko = "/dev/ttyUSB0";
#else
static const char *desc_gecko = "/dev/tty.usbserial-GECKUSB0";
#endif
#else
static const char *desc_export = "set";
static const char *desc_gecko = "COM4";
#endif

static const char *envvar = "WIILOAD";

static bool send_gecko (const char *dev, const u8 *buf, u32 len, u32 len_un,
						const char *args, u16 args_len) {
	u8 b[4];
	u32 left, block;
	const u8 *p;

	if (gecko_open (dev)) {
		fprintf (stderr, "unable to open the device '%s'\n", dev);
		return false;
	}

	printf ("sending upload request\n");

	b[0] = 'H';
	b[1] = 'A'; 
	b[2] = 'X'; 
	b[3] = 'X';

	if (gecko_write (b, 4)) {
		gecko_close ();
		fprintf (stderr, "error sending data\n");
		return false;
	}

	b[0] = WIILOAD_VERSION_MAYOR;
	b[1] = WIILOAD_VERSION_MINOR; 
	b[2] = (args_len >> 8) & 0xff;
	b[3] = args_len & 0xff;

	if (gecko_write (b, 4)) {
		gecko_close ();
		fprintf (stderr, "error sending data\n");
		return false;
	}

	printf ("sending file size (%u bytes)\n", len);

	b[0] = (len >> 24) & 0xff;
	b[1] = (len >> 16) & 0xff;
	b[2] = (len >> 8) & 0xff;
	b[3] = len & 0xff;

	if (gecko_write (b, 4)) {
		gecko_close ();
		fprintf (stderr, "error sending data\n");
		return false;
	}

	b[0] = (len_un >> 24) & 0xff;
	b[1] = (len_un >> 16) & 0xff;
	b[2] = (len_un >> 8) & 0xff;
	b[3] = len_un & 0xff;

	if (gecko_write (b, 4)) {
		gecko_close ();
		fprintf (stderr, "error sending data\n");
		return false;
	}

	printf ("sending data");
	fflush (stdout);

	left = len;
	p = buf;
	while (left) {
		block = left;
		if (block > 63488)
			block = 63488;
		left -= block;

		if (gecko_write (p, block)) {
			fprintf (stderr, "error sending block\n");
			break;
		}
		p += block;

		printf (".");
		fflush (stdout);

	}
	printf ("\n");

	if (args_len) {
		printf ("sending arguments (%u bytes)\n", args_len);

		if (gecko_write ((u8 *) args, args_len)) {
			gecko_close ();
			return false;
		}
	}

	gecko_close ();

	return true;
}

static bool tcp_write (int s, const u8 *buf, u32 len) {
	s32 left, block;
	const u8 *p;

	left = len;
	p = buf;
	while (left) {
		block = send (s, p, left, 0);

		if (block < 0) {
			perror ("send failed");
			return false;
		}

		left -= block;
		p += block;
	}

	return true;
}

static bool send_tcp (const char *host, const u8 *buf, u32 len, u32 len_un,
						const char *args, u16 args_len) {
	struct sockaddr_in sa;
	struct hostent *he;
	int s, bc;
	u8 b[4];
	off_t left, block;
	const u8 *p;

#ifdef __WIN32__
	WSADATA wsa_data;
	if (WSAStartup (MAKEWORD(2,2), &wsa_data)) {
		printf ("WSAStartup failed\n");
		return false;
	}
#endif

	memset (&sa, 0, sizeof (sa));

	sa.sin_addr.s_addr = inet_addr (host);

	if (sa.sin_addr.s_addr == INADDR_NONE) {
		printf ("resolving %s\n", host);

		he = gethostbyname (host);

		if (!he) {
#ifndef __WIN32__
			herror ("error resolving hostname");
#else
			fprintf (stderr, "error resolving hostname\n");
#endif
			return false;
		}

		if (he->h_addrtype != AF_INET) {
			fprintf (stderr, "unsupported address");
			return false;
		}

		sa.sin_addr.s_addr = *((u32 *) he->h_addr);
	}

	s = socket (PF_INET, SOCK_STREAM, 0);

	if (s < 0) {
		perror ("error creating socket");
		return false;
	}

	sa.sin_port = htons (LD_TCP_PORT);
	sa.sin_family = AF_INET;

	printf ("connecting to %s:%d\n", inet_ntoa (sa.sin_addr), LD_TCP_PORT);

	if (connect (s, (struct sockaddr *) &sa, sizeof (sa)) == -1) {
		perror ("error connecting");
		close (s);
		return false;
	}

	printf ("sending upload request\n");

	b[0] = 'H';
	b[1] = 'A'; 
	b[2] = 'X'; 
	b[3] = 'X';

	if (!tcp_write (s, b, 4)) {
		close (s);
		return false;
	}

	b[0] = WIILOAD_VERSION_MAYOR;
	b[1] = WIILOAD_VERSION_MINOR; 
	b[2] = (args_len >> 8) & 0xff;
	b[3] = args_len & 0xff;

	if (!tcp_write (s, b, 4)) {
		close (s);
		return false;
	}

	printf ("sending file size (%u bytes)\n", len);

	b[0] = (len >> 24) & 0xff;
	b[1] = (len >> 16) & 0xff;
	b[2] = (len >> 8) & 0xff;
	b[3] = len & 0xff;

	if (!tcp_write (s, b, 4)) {
		close (s);
		return false;
	}

	b[0] = (len_un >> 24) & 0xff;
	b[1] = (len_un >> 16) & 0xff;
	b[2] = (len_un >> 8) & 0xff;
	b[3] = len_un & 0xff;

	if (!tcp_write (s, b, 4)) {
		close (s);
		return false;
	}

	printf ("sending data");
	fflush (stdout);

	left = len;
	p = buf;
	bc = 0;
	while (left) {
		block = left;
		if (block > 4 * 1024)
			block = 4 * 1024;
		left -= block;

		if (!tcp_write (s, p, block)) {
			close (s);
			return false;
		}

		p += block;
		bc++;

		if (!(bc % 16)) {
			printf (".");
			fflush (stdout);
		}
	}

	printf ("\n");

	if (args_len) {
		printf ("sending arguments (%u bytes)\n", args_len);

		if (!tcp_write (s, (u8 *) args, args_len)) {
			close (s);
			return false;
		}
	}

#ifndef __WIN32__
	close (s);
#else
	shutdown (s, SD_SEND);
	closesocket (s);
	WSACleanup ();
#endif

	return true;
}

static void usage (const char *argv0) {
	fprintf (stderr, "set the environment variable %s to a valid "
				"destination.\n\n"
				"examples:\n"
				"\tusbgecko mode:\n"
				"\t\t%s %s=%s\n\n"
				"\ttcp mode:\n"
				"\t\t%s %s=tcp:wii\n"
				"\t\t%s %s=tcp:192.168.0.30\n\n"
				"usage:\n"
				"\t%s <filename> <application arguments>\n\n",
				envvar,
				desc_export, envvar, desc_gecko,
				desc_export, envvar,
				desc_export, envvar,
				argv0);
	exit (EXIT_FAILURE);
}

int main (int argc, char **argv) {
	int fd;
	struct stat st;
	char *ev;
	bool compress = true;
	u8 *buf, *bufz;
	off_t fsize;
	uLongf bufzlen = 0;
	u32 len, len_un;

	int i, c;
	char args[MAX_ARGS_LEN];
	char *arg_pos;
	u16 args_len, args_left;

	bool res;

	printf ("wiiload v%u.%u\n"
			"coded by dhewg, #wiidev efnet\n\n",
			WIILOAD_VERSION_MAYOR, WIILOAD_VERSION_MINOR);

	if (argc < 2)
		usage (*argv);

	ev = getenv (envvar);
	if (!ev)
		usage (*argv);

	fd = open (argv[1], O_RDONLY | O_BINARY);
	if (fd < 0) {
		perror ("error opening the file");
		exit (EXIT_FAILURE);
	}

	if (fstat (fd, &st)) {
		close (fd);
		perror ("error stat'ing the file");
		exit (EXIT_FAILURE);
	}

	fsize = st.st_size;

	if (fsize < 64 || fsize > 20 * 1024 * 1024) {
		close (fd);
		fprintf (stderr, "error: invalid file size\n");
		exit (EXIT_FAILURE);
	}

	buf = malloc (fsize);
	if (!buf) {
		close (fd);
		fprintf (stderr, "out of memory\n");
		exit (EXIT_FAILURE);
	}

	if (read (fd, buf, fsize) != fsize) {
		close (fd);
		free (buf);
		perror ("error reading the file");
		exit (EXIT_FAILURE);
	}
	close (fd);

	len = fsize;
	len_un = 0;

	if (!memcmp(buf, "PK\x03\x04", 4))
		compress = false;

	if (compress) {
		bufzlen = (uLongf) ((float) fsize * 1.02);

		bufz = malloc (bufzlen);
		if (!bufz) {
			fprintf (stderr, "out of memory\n");
			exit (EXIT_FAILURE);
		}

		printf("compressing %u bytes...", (u32) fsize);
		fflush(stdout);

		res = compress2 (bufz, &bufzlen, buf, fsize, 6);
		if (res != Z_OK) {
			free(buf);
			free(bufz);
			fprintf (stderr, "error compressing data: %d\n", res);
			exit (EXIT_FAILURE);
		}

		if (bufzlen < (u32) fsize) {
			printf(" %.2f%%\n", 100.0f * (float) bufzlen / (float) fsize);

			len = bufzlen;
			len_un = fsize;
			free(buf);
			buf = bufz;
		} else {
			printf(" compressed size gained size, discarding\n");
			free(bufz);
		}
	}

	args_len = 0;

	arg_pos = args;
	args_left = MAX_ARGS_LEN;

	c = snprintf (arg_pos, args_left, "%s", basename (argv[1]));
	arg_pos += c + 1;
	args_left -= c + 1;

	if (argc > 2) {
		for (i = 2; i < argc; ++i) {
			c = snprintf (arg_pos, args_left, "%s", argv[i]);

			if (c >= args_left) {
				free (buf);
				fprintf (stderr, "argument string too long\n");
				exit (EXIT_FAILURE);
			}

			arg_pos += c + 1;
			args_left -= c + 1;
		}

		if (args_left < 1) {
			free (buf);
			fprintf (stderr, "argument string too long\n");
			exit (EXIT_FAILURE);
		}
	}

	arg_pos[0] = 0;
	args_len = MAX_ARGS_LEN - args_left + 1;

	if (strncmp (ev, "tcp:", 4)) {
		if (stat (ev, &st))
			usage (*argv);

		res = send_gecko (ev, buf, len, len_un, args, args_len);
	} else {
		if (strlen (ev) < 5)
			usage (*argv);

		res = send_tcp (&ev[4], buf, len, len_un, args, args_len);
	}

	if (res)
		printf ("done.\n");
	else
		printf ("transfer failed.\n");

	free (buf);

	return 0;
}

