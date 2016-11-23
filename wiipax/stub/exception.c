#include "common.h"
#include "string.h"

extern char exception_asm_start, exception_asm_end;

void exception_handler(int exception)
{
	u32 *x;
	u32 i;

	printf("\nException %04x occurred!\n", exception);

	x = (u32 *)0x80003500;

	printf("\n R0..R7    R8..R15  R16..R23  R24..R31\n");
	for (i = 0; i < 8; i++) {
		printf("%08x  %08x  %08x  %08x\n", x[0], x[8], x[16], x[24]);
		x++;
	}
	x += 24;

	printf("\n CR/XER    LR/CTR  SRR0/SRR1 DAR/DSISR\n");
	for (i = 0; i < 2; i++) {
		printf("%08x  %08x  %08x  %08x\n", x[0], x[2], x[4], x[6]);
		x++;
	}

	// Hang.
	for (;;)
		;
}

void exception_init(void)
{
	u32 vector;
	u32 len_asm;

	for (vector = 0x100; vector < 0x1800; vector += 0x10) {
		u32 *insn = (u32 *)(0x80000000 + vector);

		insn[0] = 0xbc003500;			// stmw 0,0x3500(0)
		insn[1] = 0x38600000 | vector;	// li 3,vector
		insn[2] = 0x48003602;			// ba 0x3600
		insn[3] = 0;
	}
	sync_before_exec((void *)0x80000100, 0x1f00);

	len_asm = &exception_asm_end - &exception_asm_start;
	memcpy((void *)0x80003600, &exception_asm_start, len_asm);
	sync_before_exec((void *)0x80003600, len_asm);
}

