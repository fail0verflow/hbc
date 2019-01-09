#include <stdio.h>
#include <string.h>

#include <ogcsys.h>
#include <ogc/machine/processor.h>

#include "../config.h"
#include "loader.h"

#include "elf_abi.h"
#include "asm.h"

extern void __exception_closeall ();

typedef struct _dolheader {
	u32 text_pos[7];
	u32 data_pos[11];
	u32 text_start[7];
	u32 data_start[11];
	u32 text_size[7];
	u32 data_size[11];
	u32 bss_start;
	u32 bss_size;
	u32 entry_point;
} dolheader;

static void patch_crt0 (entry_point *ep) {
	u8 *p = (u8 *) *ep;

	if (p[0x20] == 0x41) {
		gprintf ("patching crt0\n");
		p[0x20] = 0x40;

		DCFlushRange ((void *) &p[0x20], 1);
	}
}

static void set_argv (entry_point *ep, const char *args, u16 arg_len) {
	u32 *p = (u32 *) *ep;
	struct __argv *argv;
	char *cmdline;

	if (p[1] != ARGV_MAGIC) {
		gprintf ("application does not support argv\n");
		return;
	}

	gprintf ("setting argv\n");

	argv = (struct __argv *) &p[2];
	cmdline = (char *) LD_ARGS_ADDR;

	memcpy (cmdline, args, arg_len);

#ifdef DEBUG_APP
	size_t i;

	gprintf ("cmdline='");
	for (i = 0; i < arg_len; ++i)
		if (cmdline[i] == 0)
			gprintf ("\\0");
		else
			gprintf ("%c", cmdline[i]);
	gprintf ("'\n");
#endif

	argv->argvMagic = ARGV_MAGIC;
	argv->commandLine = cmdline;
	argv->length = arg_len;

	DCFlushRange (&p[2], 4);
	DCFlushRange ((void *) LD_ARGS_ADDR, arg_len);
}

static bool reloc_dol (entry_point *ep, const u8 *addr, u32 size,
					   bool check_overlap) {
	u32 i;
	dolheader *dolfile;

	dolfile = (dolheader *) addr;
	for (i = 0; i < 7; i++) {
		if (!dolfile->text_size[i])
			continue;

		gprintf ("loading text section %u @ 0x%08x (0x%08x bytes)\n", i,
					dolfile->text_start[i], dolfile->text_size[i]);

		if (dolfile->text_pos[i] + dolfile->text_size[i] > size)
			return false;

		if (check_overlap && ((dolfile->text_start[i] < LD_MIN_ADDR) ||
				((dolfile->text_start[i] + dolfile->text_size[i] >
				LD_MAX_ADDR))))
			return false;


		memmove ((void *) dolfile->text_start[i], addr + dolfile->text_pos[i],
					dolfile->text_size[i]);

		DCFlushRange ((void *) dolfile->text_start[i], dolfile->text_size[i]);
		ICInvalidateRange ((void *) dolfile->text_start[i],
		                   dolfile->text_size[i]);
	}

	for(i = 0; i < 11; i++) {
		if (!dolfile->data_size[i])
			continue;

		gprintf ("loading data section %u @ 0x%08x (0x%08x bytes)\n", i,
					dolfile->data_start[i], dolfile->data_size[i]);

		if (dolfile->data_pos[i] + dolfile->data_size[i] > size)
			return false;

		if (check_overlap && ((dolfile->data_start[i] < LD_MIN_ADDR) ||
				(dolfile->data_start[i] + dolfile->data_size[i] > LD_MAX_ADDR)))
			return false;

		memmove ((void*) dolfile->data_start[i], addr + dolfile->data_pos[i],
					dolfile->data_size[i]);

		DCFlushRange ((void *) dolfile->data_start[i], dolfile->data_size[i]);
	}

	*ep = (entry_point) dolfile->entry_point;

	return true;
}

static s8 is_valid_elf (const u8 *addr, u32 size) {
	Elf32_Ehdr *ehdr; /* Elf header structure pointer */

	ehdr = (Elf32_Ehdr *) addr;

	if (!IS_ELF (*ehdr))
		return 0;

	if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
		return -1;

	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB)
		return -1;

	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		return -1;

	if (ehdr->e_type != ET_EXEC)
		return -1;

	if (ehdr->e_machine != EM_PPC)
		return -1;

	return 1;
}

static bool reloc_elf (entry_point *ep, const u8 *addr, u32 size,
					   bool check_overlap) {
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdrs;
	u8 *image;
	int i;

	ehdr = (Elf32_Ehdr *) addr;

	if(ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
		gprintf("ELF has no phdrs\n");
		return false;
	}
	if(ehdr->e_phentsize != sizeof(Elf32_Phdr)) {
		gprintf("Invalid ELF phdr size\n");
		return false;
	}

	phdrs = (Elf32_Phdr*)(addr + ehdr->e_phoff);

	for(i=0;i<ehdr->e_phnum;i++) {

		if(phdrs[i].p_type != PT_LOAD) {
			gprintf("skip PHDR %d of type %d\n", i, phdrs[i].p_type);
		} else {
			phdrs[i].p_paddr &= 0x3FFFFFFF;
			phdrs[i].p_paddr |= 0x80000000;

			gprintf ("PHDR %d 0x%08x [0x%x] -> 0x%08x [0x%x] <", i,
			         phdrs[i].p_offset, phdrs[i].p_filesz,
			         phdrs[i].p_paddr, phdrs[i].p_memsz);

			if(phdrs[i].p_flags & PF_R)
				gprintf("R");
			if(phdrs[i].p_flags & PF_W)
				gprintf("W");
			if(phdrs[i].p_flags & PF_X)
				gprintf("X");
			gprintf(">\n");

			if(phdrs[i].p_filesz > phdrs[i].p_memsz) {
				gprintf ("-> file size > mem size\n");
				return false;
			}

			if(phdrs[i].p_filesz) {
				if (check_overlap && ((phdrs[i].p_paddr < LD_MIN_ADDR) ||
				    (phdrs[i].p_paddr + phdrs[i].p_filesz) > LD_MAX_ADDR)) {
					gprintf ("-> failed overlap check\n");
					return false;
				}
				gprintf ("-> load 0x%x\n", phdrs[i].p_filesz);
				image = (u8 *) (addr + phdrs[i].p_offset);
				memmove ((void *) phdrs[i].p_paddr, (const void *) image,
				         phdrs[i].p_filesz);

				DCFlushRange ((void *) phdrs[i].p_paddr, phdrs[i].p_memsz);
				if(phdrs[i].p_flags & PF_X)
					ICInvalidateRange ((void *) phdrs[i].p_paddr, phdrs[i].p_memsz);
			} else {
				gprintf ("-> skip\n");
			}
		}
	}

	*ep = (entry_point) ((ehdr->e_entry & 0x3FFFFFFF) | 0x80000000);

	return true;
}

bool loader_reloc (entry_point *ep, const u8 *addr, u32 size, const char *args,
				   u16 arg_len, bool check_overlap) {
	s8 res;
	bool b;

	res = is_valid_elf (addr, size);

	if (res < 0)
		return false;

	if (res == 1)
		b = reloc_elf (ep, addr, size, check_overlap);
	else
		b = reloc_dol (ep, addr, size, check_overlap);

	if (b) {
		patch_crt0 (ep);

		if (args && arg_len)
			set_argv (ep, args, arg_len);
	}

	return b;
}

static const u32 exec_stub[] = {
	0x7c6903a6,	// mtctr r3
	0x3d208133,	// lis r9, 0x8133
	0x3d408180,	// lis r10, 0x8180
	0x90090000,	// 1: stw r0, 0(r9)
	0x39290004,	// addi r9, r9, 4
	0x7c295000,	// cmpd r9, r10
	0x4180fff4,	// blt 1b
	0x4e800420	// bctr
};

void loader_exec (entry_point ep) {
	gprintf ("shutting down services and vectoring...\n");
	SYS_ResetSystem (SYS_SHUTDOWN, 0, 0);

	__exception_closeall ();
	
	// these pokes make ninty SDK dols work, I'm told
	*(vu32*)0x800000F8 = 0x0E7BE2C0; // Bus Clock Speed
	*(vu32*)0x800000FC = 0x2B73A840; // CPU Clock Speed

	void *target = (void *)((int)(SYS_GetArena2Hi() - sizeof exec_stub) & ~31);
	void (*f_exec_stub) (int) = target;

	memcpy(target, exec_stub, sizeof exec_stub);
	DCFlushRange(target, sizeof exec_stub);
	ICInvalidateRange(target, sizeof exec_stub);

	f_exec_stub((u32)ep);

	gprintf ("this cant be good\n");
}

