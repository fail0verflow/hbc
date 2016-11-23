#define IBAT0U          528
#define IBAT0L          529
#define IBAT1U          530
#define IBAT1L          531
#define IBAT2U          532
#define IBAT2L          533
#define IBAT3U          534
#define IBAT3L          535
#define IBAT4U          560
#define IBAT4L          561
#define IBAT5U          562
#define IBAT5L          563
#define IBAT6U          564
#define IBAT6L          565
#define IBAT7U          566
#define IBAT7L          567

#define DBAT0U          536
#define DBAT0L          537
#define DBAT1U          538
#define DBAT1L          539
#define DBAT2U          540
#define DBAT2L          541
#define DBAT3U          542
#define DBAT3L          543
#define DBAT4U          568
#define DBAT4L          569
#define DBAT5U          570
#define DBAT5L          571
#define DBAT6U          572
#define DBAT6L          573
#define DBAT7U          574
#define DBAT7L          575


	.text
	.section .realmode,"ax",@progbits
	.extern _start
	.align 2
	.globl _realmode_vector

_realmode_vector:
	// HID0 = 00110c64:
	// bus checkstops off, sleep modes off,
	// caches off, caches invalidate,
	// store gathering off, enable data cache
	// flush assist, enable branch target cache,
	// enable branch history table
	lis 3,0x0011 ; ori 3,3,0x0c64 ; mtspr 1008,3 ; isync

	// MSR = 00002000 (FP on)
	li 4,0x2000 ; mtmsr 4

	// HID0 |= 0000c000 (caches on)
	ori 3,3,0xc000 ; mtspr 1008,3 ; isync

	// clear all BATs
	li 0,0
	mtspr 528,0 ; mtspr 530,0 ; mtspr 532,0 ; mtspr 534,0 // IBATU 0..3
	mtspr 536,0 ; mtspr 538,0 ; mtspr 540,0 ; mtspr 542,0 // DBATU 0..3
	mtspr 560,0 ; mtspr 562,0 ; mtspr 564,0 ; mtspr 566,0 // IBATU 4..7
	mtspr 568,0 ; mtspr 570,0 ; mtspr 572,0 ; mtspr 574,0 // DBATU 4..7
	isync

	// clear all SRs
	lis 0,0x8000
	mtsr  0,0 ; mtsr  1,0 ; mtsr  2,0 ; mtsr  3,0
	mtsr  4,0 ; mtsr  5,0 ; mtsr  6,0 ; mtsr  7,0
	mtsr  8,0 ; mtsr  9,0 ; mtsr 10,0 ; mtsr 11,0
	mtsr 12,0 ; mtsr 13,0 ; mtsr 14,0 ; mtsr 15,0
	isync

	// set [DI]BAT0 for 256MB@80000000,
	// real 00000000, WIMG=0000, R/W
	li 3,2 ; lis 4,0x8000 ; ori 4,4,0x1fff
	mtspr IBAT0L,3 ; mtspr IBAT0U,4 ; mtspr DBAT0L,3 ; mtspr DBAT0U,4 ; isync

	// set [DI]BAT4 for 256MB@90000000,
	// real 10000000, WIMG=0000, R/W
	addis 3,3,0x1000 ; addis 4,4,0x1000
	mtspr IBAT4L,3 ; mtspr IBAT4U,4 ; mtspr DBAT4L,3 ; mtspr DBAT4U,4 ; isync

	// set DBAT1 for 256MB@c0000000,
	// real 00000000, WIMG=0101, R/W
	li 3,0x2a ; lis 4,0xc000 ; ori 4,4,0x1fff
	mtspr DBAT1L,3 ; mtspr DBAT1U,4 ; isync

	// set DBAT5 for 256MB@d0000000,
	// real 10000000, WIMG=0101, R/W
	addis 3,3,0x1000 ; addis 4,4,0x1000
	mtspr DBAT5L,3 ; mtspr DBAT5U,4 ; isync

	// enable [DI]BAT4-7 in HID4
	lis 3, 0x8200
	mtspr 1011,3

	// set MSR[DR:IR] = 11, jump to _start
	lis 3,__mem1_entry@h ; ori 3,3,__mem1_entry@l ; mtsrr0 3

	mfmsr 3 ; ori 3,3,0x30 ; mtsrr1 3
	rfi
