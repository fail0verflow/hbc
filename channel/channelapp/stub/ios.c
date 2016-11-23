/*
 *  Copyright (C) 2008 segher, #wiidev efnet
 *  Copyright (C) 2008 bushing, #wiidev efnet
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
#include "stub_debug.h"
#include "ios.h"
#include "processor.h"
#include "cache.h"
#include "system.h"

#define virt_to_phys(x) ((u32*)(((u32)(x))&0x7FFFFFFF))
#define phys_to_virt(x) ((u32*)(((u32)(x))|0x80000000))

// Timebase frequency is core frequency / 8.  Ignore roundoff, this
// doesn't have to be very accurate.
#define TICKS_PER_USEC (729/8)

static u32 mftb(void)
{
	u32 x;

	asm volatile("mftb %0" : "=r"(x));

	return x;
}

static void __delay(u32 ticks)
{
	u32 start = mftb();

	while (mftb() - start < ticks)
		;
}

void udelay(u32 us)
{
	__delay(TICKS_PER_USEC * us);
}


// Low-level IPC access.

static inline u32 read32(u32 addr)
{
	u32 x;

	asm volatile("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline void write32(u32 addr, u32 x)
{
	asm volatile("stw %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}


static u32 ipc_read(u32 reg) {
	return read32(0x0d000000 + 4*reg);
}

static void ipc_write(u32 reg, u32 value) {
	write32(0x0d000000 + 4*reg, value);
}

static void ipc_bell(u32 w)
{
	ipc_write(1, (ipc_read(1) & 0x30) | w);
}

static void ipc_wait_ack(void)
{
	while ((ipc_read(1) & 0x22) != 0x22)
		;
	udelay(100);
}

static void ipc_wait_reply(void)
{
	while ((ipc_read(1) & 0x14) != 0x14)
		;
	udelay(100);
}

static void ipc_irq_ack(void)
{
	ipc_write(12, 0x40000000);
}


// Mid-level IPC access.

struct ipc {
	u32 cmd;
	int result;
	int fd;
	u32 arg[5];

	u32 user[8];
};

static struct ipc ipc __attribute__((aligned(64)));

static void ipc_send_request(void)
{
	DCFlushRange(&ipc, 0x40);

	ipc_write(0, (u32)virt_to_phys(&ipc));
	ipc_bell(1);

	ipc_wait_ack();

	ipc_bell(2);
	ipc_irq_ack();
}

void ipc_send_twoack(void)
{
	DCFlushRange(&ipc, 0x40);
	
	ipc_write(0, (u32)virt_to_phys(&ipc));
	ipc_bell(1);
	
	ipc_wait_ack();
	ipc_irq_ack();
	ipc_bell(2);

	ipc_wait_ack();
	ipc_irq_ack();
	ipc_bell(2);
	ipc_bell(8);
}

static void ipc_recv_reply(void)
{
	for (;;) {
		u32 reply;

		ipc_wait_reply();

		reply = ipc_read(2);
		ipc_bell(4);

		ipc_irq_ack();
		ipc_bell(8);

		if (((u32*)reply) == virt_to_phys(&ipc))
			break;

		debug_string("Ignoring unexpected IPC reply @");
		debug_uint((u32)reply);
		debug_string("\n\r");
	}

	DCInvalidateRange(&ipc, sizeof ipc);
}


// High-level IPC access.

int ios_open(const char *filename, u32 mode)
{
	DCFlushRange((void*)filename, strlen(filename) + 1);
	memset(&ipc, 0, sizeof ipc);

	ipc.cmd = 1;
	ipc.fd = 0;
	ipc.arg[0] = (u32)virt_to_phys(filename);
	ipc.arg[1] = mode;

	ipc_send_request();
	ipc_recv_reply();

	return ipc.result;
}

int ios_close(int fd)
{
	memset(&ipc, 0, sizeof ipc);

	ipc.cmd = 2;
	ipc.fd = fd;

	ipc_send_request();
	ipc_recv_reply();

	return ipc.result;
}

int _ios_ioctlv(int fd, u32 n, u32 in_count, u32 out_count, struct ioctlv *vec, int reboot)
{
	u32 i;

	memset(&ipc, 0, sizeof ipc);

	for (i = 0; i < in_count + out_count; i++)
		if (vec[i].data) {
			DCFlushRange(vec[i].data, vec[i].len);
			vec[i].data = (void *)virt_to_phys(vec[i].data);
		}

	DCFlushRange(vec, (in_count + out_count) * sizeof *vec);

	ipc.cmd = 7;
	ipc.fd = fd;
	ipc.arg[0] = n;
	ipc.arg[1] = in_count;
	ipc.arg[2] = out_count;
	ipc.arg[3] = (u32)virt_to_phys(vec);

	if(reboot) {
		ipc_send_twoack();
		return 0;
	} else {
		ipc_send_request();
		ipc_recv_reply();
	
		for (i = in_count; i < in_count + out_count; i++)
			if (vec[i].data) {
				vec[i].data = phys_to_virt((u32)vec[i].data);
				DCInvalidateRange(vec[i].data, vec[i].len);
			}
		return ipc.result;
	}
}

int ios_ioctlv(int fd, u32 n, u32 in_count, u32 out_count, struct ioctlv *vec) {
	return _ios_ioctlv(fd, n, in_count, out_count, vec, 0);
}

int ios_ioctlvreboot(int fd, u32 n, u32 in_count, u32 out_count, struct ioctlv *vec) {
	return _ios_ioctlv(fd, n, in_count, out_count, vec, 1);
}

// Cleanup any old state.

static void ipc_cleanup_reply(void) {
	if ((ipc_read(1) & 0x14) != 0x14)
		return;

	ipc_read(2);
	ipc_bell(4);

	ipc_irq_ack();
	ipc_bell(8);
}

static void ipc_cleanup_request(void) {
	if ((ipc_read(1) & 0x22) == 0x22)
		ipc_bell(2);
}

void reset_ios(void) {
	int i;

	debug_string("Flushing IPC transactions");
	for (i = 0; i < 10; i++) {
		ipc_cleanup_request();
		ipc_cleanup_reply();
		ipc_irq_ack();
		udelay(1000);
		debug_string(".");
	}
	debug_string(" Done.\n\r");

	debug_string("Closing file descriptors");
	for (i = 0; i < 32; i++) {
		ios_close(i);
		debug_string(".");
	}
	debug_string(" Done.\n\r");
}
