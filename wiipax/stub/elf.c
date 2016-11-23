/*
 * Copyright (c) 2001 William L. Pitts
 * Modifications (c) 2004 Felix Domke
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include "common.h"
#include "elf_abi.h"
#include "string.h"

int valid_elf_image (void *addr) {
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

u32 load_elf_image(void *addr) {
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdrs;
	u8 *image;
	int i;

	ehdr = (Elf32_Ehdr *)addr;

	if(ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
		printf("ELF has no phdrs\n");
		return 0;
	}

	if(ehdr->e_phentsize != sizeof(Elf32_Phdr)) {
		printf("Invalid ELF phdr size\n");
		return 0;
	}

	phdrs = (Elf32_Phdr*)(addr + ehdr->e_phoff);

	for(i = 0; i < ehdr->e_phnum; i++) {
		if(phdrs[i].p_type != PT_LOAD) {
			printf("skip PHDR %d of type %d\n", i, phdrs[i].p_type);
			continue;
		}

		// translate paddr to this BAT setup
		phdrs[i].p_paddr &= 0x3FFFFFFF;
		phdrs[i].p_paddr |= 0x80000000;

		printf("PHDR %d 0x%08x [0x%x] -> 0x%08x [0x%x] <", i,
				phdrs[i].p_offset, phdrs[i].p_filesz,
				phdrs[i].p_paddr, phdrs[i].p_memsz);

		if(phdrs[i].p_flags & PF_R)
			printf("R");
		if(phdrs[i].p_flags & PF_W)
			printf("W");
		if(phdrs[i].p_flags & PF_X)
			printf("X");
		printf(">\n");

		if(phdrs[i].p_filesz > phdrs[i].p_memsz) {
			printf("-> file size > mem size\n");
			return 0;
		}

		if(phdrs[i].p_filesz) {
			printf("-> load 0x%x\n", phdrs[i].p_filesz);
			image = (u8 *)(addr + phdrs[i].p_offset);
			memcpy((void *)phdrs[i].p_paddr, (const void *)image,
					phdrs[i].p_filesz);
			memset((void *)image, 0, phdrs[i].p_filesz);

			sync_after_write((void *)phdrs[i].p_paddr, phdrs[i].p_memsz);

			if(phdrs[i].p_flags & PF_X)
				sync_before_exec((void *)phdrs[i].p_paddr, phdrs[i].p_memsz);
		} else {
			printf ("-> skip\n");
		}
	}

	// entry point of the ELF _has_ to be correct - no translation done
	return ehdr->e_entry;
}

