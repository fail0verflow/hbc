#ifndef _ELF_H
#define _ELF_H

int valid_elf_image (void *addr);
u32 load_elf_image (void *addr);

#endif
