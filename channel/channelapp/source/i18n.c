#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <gctypes.h>

#include "../config.h"

struct mo_entry {
	u32 length;
	u32 offset;
} __attribute__((packed));

struct mo_hdr {
	u32 magic;
	u32 revision;
	u32 count;
	struct mo_entry *source;
	struct mo_entry *target;
	u32 hash_size;
	void *hashes;
} __attribute__((packed));

static struct mo_hdr mo;
static int mo_inited = false;
static const char *mo_data = NULL;

#define MAGIC_SWAB 0xde120495
#define MAGIC 0x950412de

#define SWAB16(x) ((((x)>>8)&0xFF) | (((x)&0xFF)<<8))
#define SWAB32(x) ((SWAB16((x)&0xFFFF)<<16)|(SWAB16(((x)>>16)&0xFFFF)))

#define ENDIAN(x) ((mo.magic == MAGIC_SWAB) ? SWAB32(x) : (x))

void i18n_set_mo(const void *mo_file) {
	mo_inited = true;
	if(!mo_file) {
		mo_data = NULL;
		gprintf("i18n: unset mo file\n");
		return;
	}

	memcpy(&mo, mo_file, sizeof(struct mo_hdr));
	if(mo.magic == MAGIC_SWAB) {
		mo.revision = SWAB32(mo.revision);
		mo.count = SWAB32(mo.count);
		mo.source = (struct mo_entry*)SWAB32((u32)mo.source);
		mo.target = (struct mo_entry*)SWAB32((u32)mo.target);
		mo.hash_size = SWAB32(mo.hash_size);
		mo.hashes = (void*)SWAB32((u32)mo.magic);
	} else if(mo.magic != MAGIC) {
		gprintf("i18n: bad mo file magic 0x%08lx\n",mo.magic);
		return;
	}

	if(mo.revision != 0)
		gprintf("i18n: bad mo file revision 0x%08lx\n",mo.revision);

	mo_data = (char*)mo_file;
	mo.source = (struct mo_entry*)(mo_data + (u32)mo.source);
	gprintf("i18n: configured mo file at %p\n",mo_data);
	mo.target = (struct mo_entry*)(mo_data + (u32)mo.target);
}

const char *i18n(char *english_string) {
	int i;

	if(!mo_inited)
		gprintf("i18n: warning: attempted to translate '%s' before init\n", english_string);

	if(!mo_data)
		return english_string;

	for(i = 0; i < mo.count; i++) {
		if(!strcmp(english_string, &mo_data[ENDIAN(mo.source[i].offset)]))
			return &mo_data[ENDIAN(mo.target[i].offset)];
	}

	gprintf("i18n: could not find string for %s\n", english_string);

	return english_string;
}

u32 i18n_have_mo(void) {
	return mo_data ? 1 : 0;
}
