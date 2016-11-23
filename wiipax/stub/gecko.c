#include "common.h"
#include "vsprintf.h"

#define		EXI_REG_BASE			0xd806800
#define		EXI0_REG_BASE			(EXI_REG_BASE+0x000)
#define		EXI1_REG_BASE			(EXI_REG_BASE+0x014)
#define		EXI2_REG_BASE			(EXI_REG_BASE+0x028)

#define		EXI0_CSR				(EXI0_REG_BASE+0x000)
#define		EXI0_MAR				(EXI0_REG_BASE+0x004)
#define		EXI0_LENGTH				(EXI0_REG_BASE+0x008)
#define		EXI0_CR					(EXI0_REG_BASE+0x00c)
#define		EXI0_DATA				(EXI0_REG_BASE+0x010)

#define		EXI1_CSR				(EXI1_REG_BASE+0x000)
#define		EXI1_MAR				(EXI1_REG_BASE+0x004)
#define		EXI1_LENGTH				(EXI1_REG_BASE+0x008)
#define		EXI1_CR					(EXI1_REG_BASE+0x00c)
#define		EXI1_DATA				(EXI1_REG_BASE+0x010)

#define		EXI2_CSR				(EXI2_REG_BASE+0x000)
#define		EXI2_MAR				(EXI2_REG_BASE+0x004)
#define		EXI2_LENGTH				(EXI2_REG_BASE+0x008)
#define		EXI2_CR					(EXI2_REG_BASE+0x00c)
#define		EXI2_DATA				(EXI2_REG_BASE+0x010)

static int gecko_console_enabled = 0;

static u32 _gecko_command(u32 command) {
	u32 i;
	// Memory Card Port B (Channel 1, Device 0, Frequency 3 (32Mhz Clock))
	write32(EXI1_CSR, 0xd0);
	write32(EXI1_DATA, command);
	write32(EXI1_CR, 0x19);
	i = 1000;
	while ((read32(EXI1_CR) & 1) && (i--));
	i = read32(EXI1_DATA);
	write32(EXI1_CSR, 0);
	return i;
}

static u32 _gecko_sendbyte(char sendbyte) {
	u32 i = 0;
	i = _gecko_command(0xB0000000 | (sendbyte<<20));
	if (i&0x04000000)
		return 1; // Return 1 if byte was sent
	return 0;
}

#if 0
static u32 _gecko_recvbyte(char *recvbyte) {
	u32 i = 0;
	*recvbyte = 0;
	i = _gecko_command(0xA0000000);
	if (i&0x08000000) {
		// Return 1 if byte was received
		*recvbyte = (i>>16)&0xff;
		return 1;
	}
	return 0;
}

static u32 _gecko_checksend(void) {
	u32 i = 0;
	i = _gecko_command(0xC0000000);
	if (i&0x04000000)
		return 1; // Return 1 if safe to send
	return 0;
}

static u32 _gecko_checkrecv(void) {
	u32 i = 0;
	i = _gecko_command(0xD0000000);
	if (i&0x04000000)
		return 1; // Return 1 if safe to recv
	return 0;
}

static void gecko_flush(void) {
	char tmp;
	while(_gecko_recvbyte(&tmp));
}
#endif

static int gecko_isalive(void) {
	u32 i = 0;
	i = _gecko_command(0x90000000);
	if (i&0x04700000)
		return 1;
	return 0;
}

#if 0
static int gecko_recvbuffer(void *buffer, u32 size) {
	u32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		if(!_gecko_recvbyte(ptr))
			break;
		ptr++;
		left--;
	}
	return (size - left);
}
#endif

static int gecko_sendbuffer(const void *buffer, u32 size) {
	u32 left = size;
	char *ptr = (char*)buffer;

	while(left>0) {
		if(!_gecko_sendbyte(*ptr))
			break;
		ptr++;
		left--;
	}
	return (size - left);
}

#if 0
static int gecko_recvbuffer_safe(void *buffer, u32 size) {
	u32 left = size;
	char *ptr = (char*)buffer;
	
	while(left>0) {
		if(_gecko_checkrecv()) {
			if(!_gecko_recvbyte(ptr))
				break;
			ptr++;
			left--;
		}
	}
	return (size - left);
}

static int gecko_sendbuffer_safe(const void *buffer, u32 size) {
	u32 left = size;
	char *ptr = (char*)buffer;
	
	while(left>0) {
		if(_gecko_checksend()) {
			if(!_gecko_sendbyte(*ptr))
				break;
			ptr++;
			left--;
		}
	}
	return (size - left);
}
#endif

void gecko_init(void)
{
	// unlock EXI
	write32(0x0d00643c, 0);

	write32(EXI0_CSR, 0);
	write32(EXI1_CSR, 0);
	write32(EXI2_CSR, 0);
	write32(EXI0_CSR, 0x2000);
	write32(EXI0_CSR, 3<<10);
	write32(EXI1_CSR, 3<<10);

	if (!gecko_isalive())
		return;

	gecko_console_enabled = 1;
}

int printf(const char *fmt, ...) {
	if (!gecko_console_enabled)
		return 0;

	va_list args;
	char buffer[1024];
	int i;

	va_start(args, fmt);
	i = vsprintf(buffer, fmt, args);
	va_end(args);

	return gecko_sendbuffer(buffer, i);
}

