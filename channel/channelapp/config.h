#ifndef _CONFIG_H_
#define _CONFIG_H_

#define CHANNEL_VERSION_DATE 201611230000llu
#define CHANNEL_VERSION_STR "1.1.3"

//#define DEBUG_APP
//#define DEBUG_STUB

#define ENABLE_WIDESCREEN
#define ENABLE_SCREENSHOTS
//#define ENABLE_UPDATES
//#define FORCE_LANG CONF_LANG_JAPANESE

#ifdef DEBUG_APP
void gprintf_enable(int enable);
int gprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void hexdump(const void *d, int len);
void memstats(int reset);
#define CHKBUFACC(access, ptr, len) \
	do { \
		if ((access < ptr) || (access >= ptr + len)) \
			gprintf("WARNING: buffer access out of range: %s:%d\n", __FILE__, __LINE__); \
	} while (0)
#else
#define gprintf(...)
#define hexdump(...)
#define memstats(...)
#define gprintf_enable(...)
#define CHKBUFACC(...)
#endif

#define UPDATE_URL "http://example.com/update.sxml"
#define UPDATE_PUBLIC_KEY \
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define USBGECKO_CHANNEL 1

#define STUB_MAGIC 0x4c4f41444b544858ull
#define STUB_ADDR_MAGIC ((u64 *) 0x80002f00)
#define STUB_ADDR_TITLE ((u64 *) 0x80002f08)

#define BOOTMII_IOS 254
#define TITLEID_BOOTMII (0x0000000100000000LL | BOOTMII_IOS)

#define PREFERRED IOS_GetPreferredVersion()
#define UNCHANGED IOS_GetVersion()

#define MY_TITLEID 0x000100014f484243ull
#define STUB_LOAD_IOS_VERSION UNCHANGED
#define APPS_IOS_VERSION PREFERRED

#define VIEW_Z_ORIGIN -420

#define GFX_ORIGIN_STACK_SIZE 16

// peak bubbles
#define MAX_BUBBLE_COUNT 20
// minimum bubbles
#define MIN_BUBBLE_COUNT 4
// cycle time in minutes
#define BUBBLE_TIME_CYCLE (60*24)
// time (in minutes, inside cycle) of minimum bubbles
#define BUBBLE_MIN_TIME (60*4)
// time (in minutes) offset from BUBBLE_MIN_TIME of maximum bubbles
#define BUBBLE_MAX_OFFSET (60*12)
// bubble size
#define BUBBLE_SIZE_MIN 0.4
#define BUBBLE_SIZE_MAX 1.0
// bubble pop radius modifier
#define BUBBLE_POP_RADIUS 0.8
// bubble pop generates this many bubbles
#define BUBBLE_POP_MAX 10
#define BUBBLE_POP_MIN 5
// bubble pop sub-bubble size
#define BUBBLE_POP_SIZE_MIN 0.2
#define BUBBLE_POP_SIZE_MAX 0.4
// bubble pop spread out range
#define BUBBLE_POP_SPREAD_X 40
#define BUBBLE_POP_SPREAD_Y 30

#define IRAND(max) ((int) ((float) (max) * (rand () / (RAND_MAX + 1.0))))
#define FRAND(max) ((max) * (rand () / (RAND_MAX + 1.0)))

#define WIDGET_DISABLED_COLOR 0xFFFFFF54

#define DIALOG_MASK_COLOR 0x101010a0

#define TEX_LAYER_WIDGETS 2
#define TEX_LAYER_DIALOGS 30

#define TEX_LAYER_CURSOR 80

#define APP_ENTRY_ICON_X 8
#define APP_ENTRY_ICON_Y 8
#define GRID_APP_ENTRY_ICON_X 8
#define GRID_APP_ENTRY_ICON_Y 8
#define APP_ENTRY_ICON_WIDTH 128
#define APP_ENTRY_ICON_HEIGHT 48
#define APP_ENTRY_TEXT1_X 156
#define APP_ENTRY_TEXT1_Y 8
#define APP_ENTRY_TEXT2_X 156
#define APP_ENTRY_TEXT2_Y 54

#define MAX_ENTRIES 1024

#define TCP_CONNECT_TIMEOUT 5000
#define TCP_BLOCK_SIZE (16 * 1024)
#define TCP_BLOCK_RECV_TIMEOUT 10000
#define TCP_BLOCK_SEND_TIMEOUT 4000

#define WIILOAD_MIN_VERSION 0x0005
#define ARGS_MAX_LEN 1024

#define LD_TCP_PORT 4299
#define LD_THREAD_STACKSIZE (1024 * 8)
#define LD_THREAD_PRIO 48
#define LD_TIMEOUT 3000
#define LD_MIN_ADDR 0x80003400
#define LD_MAX_ADDR (BASE_ADDR - 1 - ARGS_MAX_LEN)
#define LD_MAX_SIZE (LD_MAX_ADDR - LD_MIN_ADDR)
#define LD_ARGS_ADDR (LD_MAX_ADDR + 1)

#define HTTP_THREAD_STACKSIZE (1024 * 8)
#define HTTP_THREAD_PRIO 48
#define HTTP_TIMEOUT 30000

#define MANAGE_THREAD_STACKSIZE (1024 * 16)
#define MANAGE_THREAD_PRIO 48

#define APPENTRY_THREAD_STACKSIZE (1024 * 16)
#define APPENTRY_THREAD_PRIO 62

#define UPDATE_THREAD_STACKSIZE (1024 * 8)
#define UPDATE_THREAD_PRIO 58

#define FORCE_INLINE __attribute__((always_inline))

#endif

