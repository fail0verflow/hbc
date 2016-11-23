OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)

__mem1_start = 0x80004000;
__mem2_start = 0x90010000;

__mem1_entry = _start - __mem2_start + __mem1_start;
ENTRY(__mem1_entry)

PHDRS {
	realmode	PT_LOAD FLAGS(5);
	paxx		PT_LOAD FLAGS(7);
}

SECTIONS {
	. = 0x80003400;

	.realmode : { KEEP(*(.realmode)) } :realmode = 0

	. = __mem2_start;

	.start : AT(__mem1_start) { KEEP(*(.start)) } :paxx = 0
	.text : { *(.text) *(.text.*) }

	. = ALIGN(4);

	.payload : {
		__payload = .;
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(0);
	}

	.rodata : { *(.rodata) *(.rodata.*) }

	. = (( . +19)&0xFFFFFFF0) - 4;
	.padding : {
		LONG(0xdeadbeef);
	}

	.sdata : { *(.sdata) *(.sdata.*) }
	.data : { *(.data) *(.data.*) }

	. = ALIGN(32);
	__self_end = .;

	__bss_start = .;
	.bss : { *(.bss) } :NONE = 0
	.sbss : { *(.sbss) }
	__bss_end = .;

	. = ALIGN(32);

	.stack : {
		_stack_top = .;
		. += 32768;
		_stack_bot = .;
	}

}

