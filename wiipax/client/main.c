#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "lzma.h"
#include "elf_abi.h"

#define ALIGNOFFSET 32

extern int stub_mini_elf_len;
extern char stub_mini_elf[];
extern int stub_mini_debug_elf_len;
extern char stub_mini_debug_elf[];
extern int stub_dkp_elf_len;
extern char stub_dkp_elf[];
extern int stub_dkp_debug_elf_len;
extern char stub_dkp_debug_elf[];
extern int stub_dkpc_elf_len;
extern char stub_dkpc_elf[];
extern int stub_dkpc_debug_elf_len;
extern char stub_dkpc_debug_elf[];

typedef struct {
	const char *name;
	const int *len;
	const u8 *data;
} stub_t;

static const stub_t stubs[] = {
	{ "mini", &stub_mini_elf_len, (u8 *) stub_mini_elf },
	{ "mini_debug", &stub_mini_debug_elf_len, (u8 *) stub_mini_debug_elf },
	{ "devkitppc", &stub_dkp_elf_len, (u8 *) stub_dkp_elf },
	{ "devkitppc_debug", &stub_dkp_debug_elf_len, (u8 *) stub_dkp_debug_elf },
	{ "dkppcchannel", &stub_dkpc_elf_len, (u8 *) stub_dkpc_elf },
	{ "dkppcchannel_debug", &stub_dkpc_debug_elf_len, (u8 *) stub_dkpc_debug_elf },
	{ NULL, NULL, NULL }
};

typedef struct {
	u8 *data;
	u32 len;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdrs;
	Elf32_Shdr *shdrs;
} elf_t;

typedef struct {
	u32 dataptr;
	u32 len_in;
	u32 len_out;
	u8 props[LZMA_PROPS_SIZE];
} __attribute__((packed)) payload_t;

static inline u8 *phdr_data(const elf_t *elf, const u16 ndx, const u32 off) {
	return &elf->data[be32(elf->phdrs[ndx].p_offset) + off];
}

static elf_t *read_stub(const char *name) {
	elf_t *elf = (elf_t *) calloc(1, sizeof(elf_t));
	if (!elf)
		die("Error allocating %u bytes", (u32) sizeof(elf_t));

	int i = 0;
	while (stubs[i].name) {
		if (!strcmp(name, stubs[i].name)) {
			printf("Using stub '%s'\n", name);
			elf->len = *(stubs[i].len);
			elf->data = (u8 *) stubs[i].data;

			return elf;
		}
		++i;
	}

	die("Unknown stub '%s'", name);
	return NULL;
}

static elf_t *read_elf(const char *filename) {
	int fd;
	struct stat st;
	elf_t *elf;

	printf("Reading %s\n", filename);

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		perrordie("Could not open ELF file");

	if (fstat(fd, &st))
		perrordie("Could not stat ELF file");

	if ((u32) st.st_size < sizeof(Elf32_Ehdr))
		die("File too short for ELF");

	elf = (elf_t *) calloc(1, sizeof(elf_t));
	if (!elf)
		die("Error allocating %u bytes", (u32) sizeof(elf_t));

	elf->len = st.st_size;
	elf->data = (u8 *) malloc(elf->len);

	if (!elf->data)
		die("Error allocating %u bytes", elf->len);

	if (read(fd, elf->data, elf->len) != elf->len)
		perrordie("Could not read from file");

	close(fd);

	return elf;
}

static void write_elf(const char *filename, const elf_t *elf) {
	int fd;

	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd < 0)
		perrordie("Could not open ELF file");

	if (write(fd, elf->data, elf->len) != elf->len)
		perrordie("Could not write ELF file");

	close(fd);
}

static void free_elf(elf_t *elf) {
	free(elf->data);
	free(elf);
}

static void check_elf(elf_t *elf) {
	if (elf->len < sizeof(Elf32_Phdr))
		die("Too short for an ELF");

	Elf32_Ehdr *ehdr = (Elf32_Ehdr *) elf->data;

	if (!IS_ELF(*ehdr))
		die("Not an ELF");
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
		die("Invalid ELF class");
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB)
		die("Invalid ELF byte order");
	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		die("Invalid ELF ident version");
	if (be16(ehdr->e_type) != ET_EXEC)
		die("ELF is not an executable");
	if (be16(ehdr->e_machine) != EM_PPC)
		die("Machine is not PowerPC");
	if (be32(ehdr->e_version) != EV_CURRENT)
		die("Invalid ELF version");
	if (!be32(ehdr->e_entry))
		die("ELF has no entrypoint");
}

static void init_elf(elf_t *elf, int sections) {
	elf->ehdr = (Elf32_Ehdr *) elf->data;

	u16 num = be16(elf->ehdr->e_phnum);
	u32 off = be32(elf->ehdr->e_phoff);

	if (!num || !off)
		die("ELF has no program headers");

	if (be16(elf->ehdr->e_phentsize) != sizeof(Elf32_Phdr))
		die("Invalid program header entry size");

	if ((off + num * sizeof(Elf32_Phdr)) > elf->len)
		die("Program headers out of bounds");

	elf->phdrs = (Elf32_Phdr *) &elf->data[off];

	num = be16(elf->ehdr->e_shnum);
	off = be32(elf->ehdr->e_shoff);

	if (!num || !off) {
		if (!sections) {
			elf->shdrs = NULL;
			return;
		}

		die("ELF has no section headers");
	}

	if (be16(elf->ehdr->e_shentsize) != sizeof(Elf32_Shdr))
		die("Invalid section header entry size");

	if ((off + num * sizeof(Elf32_Shdr)) > elf->len)
		die("Section headers out of bounds");

	elf->shdrs = (Elf32_Shdr *) &elf->data[off];
}

static u32 find_payload_offset(const elf_t *elf) {
	u16 shnum = be16(elf->ehdr->e_shnum);

	u16 shstrndx = be16(elf->ehdr->e_shstrndx);
	if (!shstrndx || shstrndx > shnum)
		die("Invalid .shstrtab index");

	u32 off = be32(elf->shdrs[shstrndx].sh_offset);
	u32 size = be32(elf->shdrs[shstrndx].sh_size);

	if (off + size > elf->len - 1)
		die(".shstrtab section out of bounds");

	const char *shstr = (const char *) &elf->data[off];

	u16 i;
	for (i = 0; i < shnum; ++i) {
		off = be32(elf->shdrs[i].sh_name);

		if (off > size)
			die("Section #%u name out of .shstrtab bounds", i);

		if (!strcmp(&shstr[off], ".payload")) {
			printf(".payload section found: #%u\n", i);
			return be32(elf->shdrs[i].sh_offset);
		}
	}

	die(".payload section not present");
}

static void map_file_offset(const elf_t *elf, const u32 offset,
							u16 *phdrndx, u32 *phdroff) {
	u16 phnum = be16(elf->ehdr->e_phnum);

	u16 i;
	for (i = 0; i < phnum; ++i) {
		if (be32(elf->phdrs[i].p_type) != PT_LOAD)
			continue;

		if (be32(elf->phdrs[i].p_filesz) < 1)
			continue;

		u32 poff = be32(elf->phdrs[i].p_offset);
		u32 psize = be32(elf->phdrs[i].p_filesz);

		if (offset >= poff && offset <= poff + psize) {
			*phdrndx = i;
			*phdroff = offset - poff;
			printf("Mapped payload to program header [%u] 0x%06x\n",
					*phdrndx, *phdroff);
			return;
		}
	}

	die("File offset 0x%x is not part of any PT_LOAD program header", offset); 
}

static elf_t *strip_elf(const elf_t *elf, u16 *phdrndx) {
	elf_t *out;
	u32 pos;
	u16 count;
	u16 phnum = be16(elf->ehdr->e_phnum);
	u16 i;

	count = 0;
	pos = round_up(sizeof(Elf32_Ehdr), ALIGNOFFSET);
	for (i = 0; i < phnum; ++i) {
		if (be32(elf->phdrs[i].p_type) != PT_LOAD)
			continue;

		if (be32(elf->phdrs[i].p_filesz) < 1)
			continue;

		if (be32(elf->phdrs[i].p_memsz) < 1)
			continue;

		pos += round_up(be32(elf->phdrs[i].p_filesz), ALIGNOFFSET);
		count++;
	}
	pos += round_up(count * sizeof(Elf32_Phdr), ALIGNOFFSET);

	if (pos > 20 * 1024 * 1024)
		die("ELF too big, even after stripping (0x%x)", pos);

	printf("Stripping ELF from 0x%x to 0x%x bytes\n", elf->len, pos);

	out = (elf_t *) calloc(1, sizeof(elf_t));
	if (!out)
		die("Error allocating %u bytes", (u32) sizeof(elf_t));

	out->len = pos;
	out->data = calloc(1, pos);
	if (!out->data)
		die("Error allocating %u bytes", pos);

	out->ehdr = (Elf32_Ehdr *) out->data;
	pos = round_up(sizeof(Elf32_Ehdr), ALIGNOFFSET);
	out->ehdr->e_ident[EI_MAG0] = ELFMAG0;
	out->ehdr->e_ident[EI_MAG1] = ELFMAG1;
	out->ehdr->e_ident[EI_MAG2] = ELFMAG2;
	out->ehdr->e_ident[EI_MAG3] = ELFMAG3;
	out->ehdr->e_ident[EI_CLASS] = ELFCLASS32;
	out->ehdr->e_ident[EI_DATA] = ELFDATA2MSB;
	out->ehdr->e_ident[EI_VERSION] = EV_CURRENT;
	out->ehdr->e_type = be16(ET_EXEC);
	out->ehdr->e_machine = be16(EM_PPC);
	out->ehdr->e_version = be32(EV_CURRENT);
	out->ehdr->e_entry = elf->ehdr->e_entry;
	out->ehdr->e_phoff = be32(pos);
	out->ehdr->e_phentsize = be16(sizeof(Elf32_Phdr));
	out->ehdr->e_phnum = be16(count);
	out->ehdr->e_shentsize = be16(sizeof(Elf32_Shdr));

	out->phdrs = (Elf32_Phdr *) &out->data[pos];
	pos += round_up(count * sizeof(Elf32_Phdr), ALIGNOFFSET);

	count = 0;
	int found = 0;
	for (i = 0; i < phnum; ++i) {
		if (be32(elf->phdrs[i].p_type) != PT_LOAD)
			continue;

		if (be32(elf->phdrs[i].p_filesz) < 1)
			continue;

		if (be32(elf->phdrs[i].p_memsz) < 1)
			continue;

		if (phdrndx && i == *phdrndx) {
			*phdrndx = count;
			found = 1;
		}

		out->phdrs[count].p_type = elf->phdrs[i].p_type;
		out->phdrs[count].p_offset = be32(pos);
		out->phdrs[count].p_vaddr = elf->phdrs[i].p_vaddr;
		out->phdrs[count].p_paddr = elf->phdrs[i].p_paddr;
		out->phdrs[count].p_filesz = elf->phdrs[i].p_filesz;
		out->phdrs[count].p_memsz = elf->phdrs[i].p_memsz;
		out->phdrs[count].p_flags = elf->phdrs[i].p_flags;
		out->phdrs[count].p_align = elf->phdrs[i].p_align;

		u32 p_offset = be32(elf->phdrs[i].p_offset);
		u32 p_filesz = be32(elf->phdrs[i].p_filesz);

		printf("  PHDR[%u] 0x%08x 0x%06x -> [%u] 0x%06x\n",
				i, p_offset, p_filesz, count, pos);

		memcpy(&out->data[pos], &elf->data[p_offset], p_filesz);
		pos += round_up(p_filesz, ALIGNOFFSET);

		count++;
	}

	if (phdrndx && !found)
		die("PHDR #%u not part of the stripped ELF", *phdrndx);

	return out;
}

static elf_t *inject_elf(elf_t *dst, const u8 *src, const u32 len,
						u32 *dataaddr, u8 **dataptr) {
	u16 phdrndx = be16(dst->ehdr->e_phnum) - 1;
	Elf32_Phdr *phdr = &dst->phdrs[phdrndx];

	if (phdr->p_filesz != phdr->p_memsz)
		die("File size does not match the memory size for the last PHDR");

	u32 pos = be32(phdr->p_vaddr) + be32(phdr->p_filesz);
	u32 pos_a = round_up(pos, ALIGNOFFSET);
	u32 pos_d = pos_a - pos;
	u32 size_d = pos_d + round_up(len, ALIGNOFFSET);

	printf("Injecting payload in PHDR %u, size += 0x%x (0x%x/0x%x/0x%x)\n",
			phdrndx, size_d, pos_d, len, size_d - pos_d - len);

	elf_t *elf = (elf_t *) calloc(1, sizeof(elf_t));
	if (!elf)
		die("Error allocating %u bytes", (u32) sizeof(elf_t));

	elf->data = calloc(1, dst->len + size_d);
        elf->len = dst->len + size_d;
	if (!elf->data)
		die("Failed to alloc 0x%x bytes", dst->len + size_d);

	memcpy(elf->data, dst->data, dst->len);
	init_elf(elf, 0);

	phdr = &elf->phdrs[phdrndx];
	u8 *p = phdr_data(elf, phdrndx, be32(phdr->p_filesz));

	memcpy(&p[pos_d], src, len);

	*dataaddr = pos_a - be32(phdr->p_vaddr) + be32(phdr->p_paddr);
	*dataaddr |= 0x80000000;

	phdr->p_filesz = be32(be32(phdr->p_filesz) + size_d);
	phdr->p_memsz = be32(be32(phdr->p_memsz) + size_d);
	phdr->p_vaddr = phdr->p_paddr;


	printf("Payload blob @0x%x at runtime\n", pos_a);

	*dataptr = &p[pos_d];
	return elf;
}

#define CLOCKS_PER_BYTE 47
#define BUF_SIZE 256
#define ROUNDS 256


static void usage(const char *name) {
	printf("usage: %s [-s stub] in.elf out.elf\n", name);

	printf("stubs:");
	int i = 0;
	while (stubs[i].name) {
		printf(" %s", stubs[i].name);
		++i;
	}

	printf("\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	elf_t *elf_tmp, *elf_pl, *elf_stub;
	u32 offset;
	u16 phdrndx;
	u32 phdroff;
	u32 dataaddr;
	u8 *dataptr;
	lzma_t *lzma;

	printf("wiipax v0.2 (c) 2009 Team Twiizers\n\n");

	if (argc < 3)
		usage(argv[0]);

	char *stubname = "mini";

	char **arg = &argv[1];
	argc--;

	while (argc && *arg[0] == '-') {
		if (!strcmp(*arg, "-h")) {
			usage(argv[0]);
		} else if (!strcmp(*arg, "-s")) {
			if (argc < 2)
				usage(argv[0]);
			arg++;
			argc--;
			stubname = *arg;
		} else if (!strcmp(*arg, "--")) {
			arg++;
			argc--;
			break;
		} else {
			die("Unrecognized option %s\n", *arg);
			usage(argv[0]);
		}
		arg++;
		argc--;
	}

	if (argc != 2)
		usage(argv[0]);

	elf_tmp = read_stub(stubname);
	check_elf(elf_tmp);
	init_elf(elf_tmp, 1);

	offset = find_payload_offset(elf_tmp);
	map_file_offset(elf_tmp, offset, &phdrndx, &phdroff);
	elf_stub = strip_elf(elf_tmp, &phdrndx);
	free(elf_tmp);

	elf_tmp = read_elf(arg[0]);
	check_elf(elf_tmp);
	init_elf(elf_tmp, 0);
	elf_pl = strip_elf(elf_tmp, NULL);
	free_elf(elf_tmp);

	lzma = lzma_compress(elf_pl->data, elf_pl->len);
	// test decoding
	lzma_decode(lzma, elf_pl->data);
	free_elf(elf_pl);

#if 0
	lzma_write("x.lzma", lzma);
#endif

	u32 aes_len = (lzma->len_out + 15) & (~15);
	u8 aiv[16];
	memset(aiv, 0, 16);

	lzma->data = realloc(lzma->data, aes_len);
	memset(lzma->data + lzma->len_out, 0, aes_len - lzma->len_out);

	elf_tmp = inject_elf(elf_stub, lzma->data, aes_len, &dataaddr, &dataptr);
	free_elf(elf_stub);

	payload_t *pl = (payload_t *) phdr_data(elf_tmp, phdrndx, phdroff);
	pl->dataptr = be32(dataaddr);
	pl->len_in = be32(lzma->len_out);
	pl->len_out = be32(lzma->len_in);
	memcpy(pl->props, lzma->props, LZMA_PROPS_SIZE);
	lzma_free(lzma);

	write_elf(arg[1], elf_tmp);
	free_elf(elf_tmp);

	printf("Done.\n");
	return 0;
}

