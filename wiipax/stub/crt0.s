# crt0.s file for the GameCube V1.0 by Costis (costis@gbaemu.com)!

	.text
	.section .start,"ax",@progbits
	.extern _start
	.align 2
	.globl _start
_start:

	lis	3,__mem1_start@h
	ori 3,3,__mem1_start@l
	lis	4,__mem2_start@h
	ori 4,4,__mem2_start@l
	lis 5,__self_end@h
	ori 5,5,__self_end@l

_reloc_loop:
	lwz 2,0(3)
	stw 2,0(4)
	
	addi 3,3,4
	addi 4,4,4
	cmplw 4,5
	blt _reloc_loop

	lis 4,__mem2_start@h
	ori 4,4,__mem2_start@l
_flush_loop:
	dcbst 0,4
	sync
	icbi 0,4
	addi 4,4,32
	cmplw 4,5
	blt _flush_loop

	sync
	isync

	lis 3,_mem2_entry@h
	ori 3,3,_mem2_entry@l
	mtctr 3
	bctr

_mem2_entry:
	# Clear all GPRs except
	.irp i, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	li \i,0
	.endr

	lis 1,_stack_bot@h
	ori 1,1,_stack_bot@l
	stwu 0,-64(1)

	lis      2,0x8000
	stw      1,0x34(2) # write sp
#	lis      13,_SDA_BASE_@h
#	ori      13,13,_SDA_BASE_@l # Set the Small Data (Read\Write) base register.	

	bl       InitHardware # Initialize the GameCube Hardware (Floating Point Registers, Caches, etc.) 
	bl       SystemInit # Initialize more cache aspects, clear a few SPR's, and disable interrupts.
	
	# clear BSS
	lis 3, __bss_start@h
	ori 3, 3, __bss_start@l
	li 4, 0
	lis 5, __bss_end@h
	ori 5, 5, __bss_end@l
	sub 5, 5, 3
	
	bl memset

	mr 3, 28
	bl       stubmain # Branch to the user code!

	b .

InitHardware:
	mflr     31 # Store the link register in r31
	
	bl		 PSInit 	# Initialize Paired Singles
	bl       FPRInit 	# Initialize the FPR's
	bl       CacheInit 	# Initialize the system caches

	mtlr     31 # Retreive the link register from r31
	blr

PSInit:
	mfspr    3, 920 # (HID2)
	oris     3, 3, 0xA000
	mtspr    920, 3 # (HID2)

	# Set the Instruction Cache invalidation bit in HID0
	mfspr    3,1008
	ori      3,3,0x0800
	mtspr    1008,3

	sync

	# Clear various Special Purpose Registers
	li       3,0
	mtspr    912,3
	mtspr    913,3
	mtspr    914,3
	mtspr    915,3
	mtspr    916,3
	mtspr    917,3
	mtspr    918,3
	mtspr    919,3

	# Return 
	blr

FPRInit:
	# Enable the Floating Point Registers
	mfmsr    3
	ori      3,3,0x2000
	mtmsr    3
	
	# Clear all of the FPR's to 0
	lis		 3, zfloat@h
	ori		 3, 3, zfloat@l
	lfd		 0, 0(3)
	fmr      1,0
	fmr      2,0
	fmr      3,0
	fmr      4,0
	fmr      5,0
	fmr      6,0
	fmr      7,0
	fmr      8,0
	fmr      9,0
	fmr      10,0
	fmr      11,0
	fmr      12,0
	fmr      13,0
	fmr      14,0
	fmr      15,0
	fmr      16,0
	fmr      17,0
	fmr      18,0
	fmr      19,0
	fmr      20,0
	fmr      21,0
	fmr      22,0
	fmr      23,0
	fmr      24,0
	fmr      25,0
	fmr      26,0
	fmr      27,0
	fmr      28,0
	fmr      29,0
	fmr      30,0
	fmr      31,0
	mtfsf    255,0

	# Return
	blr

CacheInit:
	mflr     0
	stw      0, 4(1)
	stwu     1, -16(1)
	stw      31, 12(1)
	stw      30, 8(1)

	mfspr    3,1008 # (HID0)
	rlwinm   0, 3, 0, 16, 16
	cmplwi   0, 0x0000 # Check if the Instruction Cache has been enabled or not.
	bne      ICEnabled

	# If not, then enable it.
	isync
	mfspr    3, 1008
	ori      3, 3, 0x8000
	mtspr    1008, 3

ICEnabled:
	mfspr    3, 1008 # bl       PPCMfhid0
	rlwinm   0, 3, 0, 17, 17
	cmplwi   0, 0x0000 # Check if the Data Cache has been enabled or not.
	bne      DCEnabled
	
	# If not, then enable it.
	sync
	mfspr    3, 1008
	ori      3, 3, 0x4000
	mtspr    1008, 3
	
DCEnabled:
	
	mfspr    3, 1017 # (L2CR)
	clrrwi   0, 3, 31 # Clear all of the bits except 31
	cmplwi   0, 0x0000
	bne      L2GISkip # Skip the L2 Global Cache Invalidation process if it has already been done befor.

	# Store the current state of the MSR in r30
	mfmsr    3
	mr       30,3
	
	sync
	
	# Enable Instruction and Data Address Translation
	li       3, 48
	mtmsr    3

	sync
	sync

	# Disable the L2 Global Cache.
	mfspr    3, 1017 # (L2CR
	clrlwi   3, 3, 1
	mtspr    1017, 3 # (L2CR)
	sync

	# Invalidate the L2 Global Cache.
	bl       L2GlobalInvalidate

	# Restore the previous state of the MSR from r30
	mr       3, 30
	mtmsr    3

	# Enable the L2 Global Cache and disable the L2 Data Only bit and the L2 Global Invalidate Bit.
	mfspr    3, 1017 # (L2CR)
	oris     0, 3, 0x8000
	rlwinm   3, 0, 0, 11, 9
	mtspr    1017, 3 # (L2CR)


L2GISkip:
	# Restore the non-volatile registers to their previous values and return.
	lwz      0, 20(1)
	lwz      31, 12(1)
	lwz      30, 8(1)
	addi     1, 1, 16
	mtlr     0
	blr

L2GlobalInvalidate:
	mflr     0
	stw      0, 4(1)
	stwu     1, -16(1)
	stw      31, 12(1)
	sync

	# Disable the L2 Cache.
	mfspr    3, 1017  # bl       PPCMf1017
	clrlwi   3, 3, 1
	mtspr    1017, 3 # bl       PPCMt1017

	sync

	# Initiate the L2 Cache Global Invalidation process.
	mfspr    3, 1017  # (L2CR)
	oris     3, 3, 0x0020
	mtspr    1017, 3 # (L2CR)

	# Wait until the L2 Cache Global Invalidation has been completed.
L2GICheckComplete:
	mfspr    3, 1017 # (L2CR)
	clrlwi   0, 3, 31
	cmplwi   0, 0x0000
	bne      L2GICheckComplete
	
	# Clear the L2 Data Only bit and the L2 Global Invalidate Bit.
	mfspr    3, 1017  # (L2CR)
	rlwinm   3, 3, 0, 11, 9
	mtspr    1017, 3 # (L2CR)

	# Wait until the L2 Cache Global Invalidation status bit signifies that it is ready.
L2GDICheckComplete:
	mfspr    3, 1017  # (L2CR)
	clrlwi   0, 3, 31
	cmplwi   0, 0x0000
	bne      L2GDICheckComplete

	# Restore the non-volatile registers to their previous values and return.
	lwz      0, 20(1)
	lwz      31, 12(1)
	addi     1, 1, 16
	mtlr     0
	blr

SystemInit:
	mflr    0
	stw     0, 4(1)
	stwu    1, -0x18(1)
	stw     31, 0x14(1)
	stw     30, 0x10(1)
	stw     29, 0xC(1)

	# Disable interrupts!
	mfmsr    3
	rlwinm   4,3,0,17,15
	rlwinm   4,4,0,26,24
	mtmsr    4

	# Clear various SPR's
	li       3,0
	mtspr    952, 3
	mtspr    956, 3
	mtspr    953, 3
	mtspr    954, 3
	mtspr    957, 3
	mtspr    958, 3

	# Disable Speculative Bus Accesses to non-guarded space from both caches.
	mfspr    3, 1008 # (HID0)
	ori      3, 3, 0x0200
	mtspr    1008, 3

	# Set the Non-IEEE mode in the FPSCR
	mtfsb1   29
	
	mfspr    3,920 # (HID2)
	rlwinm   3, 3, 0, 2, 0
	mtspr    920,3 # (HID2)		

	# Restore the non-volatile registers to their previous values and return.
	lwz     0, 0x1C(1)
	lwz     31, 0x14(1)
	lwz     30, 0x10(1)
	lwz     29, 0xC(1)
	addi    1, 1, 0x18
	mtlr    0
	blr

zfloat:
	.float	0
	.align	4
_got_start:
