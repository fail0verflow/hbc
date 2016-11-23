// this file was taken from libogc, see http://www.devkitpro.org/

#include "asm.h"

#define _SDA_BASE_ 32768
#define _SDA2_BASE_ 32768


        .text
        .section .init

        # crt0.s file for the GameCube V1.1 by Costis (costis@gbaemu.com)!
        #
        # Updates: Added support for clearing the BSS section so that global
        #            variables are cleared to 0 upon start-up. 
        #
        # This is start-up code for initializing the GameCube system and hardware
        # before executing the actual user program code. It clears the GPR's,
        # initializes the FPR's, initializes the Data, Code, and L2 caches, clears
        # and initializes SPR's, and disables exceptions (interrupts).
        #
        # Have fun!!! Please e-mail any suggestions or bugs to costis@gbaemu.com.

        # Entry Point

        .extern __PSInit
        .extern __SyscallInit
        .extern __CacheInit
        .extern __SystemInit
        .extern __sbss_start, __bss_end
        .extern __bat_config
        .extern _main
        .globl _start, __main
_start:
        b startup
        .ascii "STUB"
        .ascii "HAXX"
        .long 0
startup:
        # Disable interrupts first thing
        mfmsr   r3
        rlwinm  r4,r3,0,17,15
        rlwinm  r4,r4,0,26,24
        mtmsr   r4

        # Go into real mode
        isync
        lis     r3,real@h
        ori     r3,r3,real@l
        clrlwi  r3,r3,2
        mtsrr0  r3

        mfmsr   r3
        li      r4,0x30
        andc    r3,r3,r4
        mtsrr1  r3
        rfi
        
real:
        # Set up the BATs the way we like them
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
        lis 3,virtual@h ; ori 3,3, virtual@l ; mtsrr0 3

        mfmsr 3 ; ori 3,3,0x30 ; mtsrr1 3
        rfi

virtual:

        bl      InitGPRS                # Initialize the General Purpose Registers
        bl      __CacheInit             # Initialize the system caches
        bl      __SyscallInit           # Initialize the System Call handler
        bl      __SystemInit            # Initialize more cache aspects, clear a few SPR's, and disable interrupts.

        # Clear the BSS section!
        lis             r3,__sbss_start@h
        ori             r3,r3,__sbss_start@l
        li              r4,0
        lis             r5,__bss_end@h
        ori             r5,r5,__bss_end@l
        sub             r5,r5,r3
        bl              _memset

        bl      _main # Branch to the user code!

eloop:
        b eloop                 # If the main function returns, which should never happen then just loop endlessly.

InitGPRS:
        # Clear all of the GPR's to 0
        li      r0,0
        li      r3,0
        li      r4,0
        li      r5,0
        li      r6,0
        li      r7,0
        li      r8,0
        li      r9,0
        li      r10,0
        li      r11,0
        li      r12,0
        li      r14,0
        li      r15,0
        li      r16,0
        li      r17,0
        li      r18,0
        li      r19,0
        li      r20,0
        li      r21,0
        li      r22,0
        li      r23,0
        li      r24,0
        li      r25,0
        li      r26,0
        li      r27,0
        li      r28,0
        li      r29,0
        li      r30,0
        li      r31,0

        lis             sp,__crt0stack@h                    # we take 0x8173FFF0 as the topmost starting point for our stack,this gives us ~128Kb Stack
        ori             sp,sp,__crt0stack@l
        addi    sp,sp,-4
        stw             r0,0(sp)
        stwu    sp,-56(sp)

        lis     r2,_SDA2_BASE_@h
        ori     r2,r2,_SDA2_BASE_@l  # Set the Small Data 2 (Read Only) base register.
        lis     r13,_SDA_BASE_@h
        ori     r13,r13,_SDA_BASE_@l # Set the Small Data (Read\Write) base register.   
        blr

        //r3 = ptr, r4 = fill, r5 = size
        .globl _memset
_memset:
        clrlwi. r6,r5,29
        srwi    r5,r5,2
        subi    r3,r3,4
        mtctr   r5
1:      stwu    r4,4(r3)
        bdnz    1b
        cmplwi  r6,0
        beq             3f
2:      stbu    r4,1(r3)
        addic.  r6,r6,-1
        bne+    2b
3:      blr

        .section .bss
        .balign 8
__crt0stack_end:
        .space 0x8000
__crt0stack:

