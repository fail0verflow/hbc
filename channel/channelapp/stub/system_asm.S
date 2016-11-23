// this code was taken from libogc, see http://www.devkitpro.org/ 

#include "asm.h"

	.extern ICFlashInvalidate
	.extern ICEnable
	.extern DCEnable
	.extern L2Init
	.extern L2Enable
	.globl __CacheInit
__CacheInit:
	mflr    r0
	stw     r0, 4(sp)
	stwu    sp, -16(sp)
	stw     r31, 12(sp)

	mfspr   r3,HID0 # (HID0)
	rlwinm  r0,r3, 0, 16, 16
	cmplwi  r0, 0x0000 # Check if the Instruction Cache has been enabled or not.
	bne     ICEnabled

	bl		ICEnable
ICEnabled:
	mfspr   r3, HID0 # bl       PPCMfhid0
	rlwinm  r0, r3, 0, 17, 17
	cmplwi  r0, 0x0000 # Check if the Data Cache has been enabled or not.
	bne     DCEnabled
	
	bl		DCEnable
DCEnabled:
	
	mfspr   r3, L2CR # (L2CR)
	clrrwi  r0, r3, 31 # Clear all of the bits except 31
	cmplwi  r0, 0x0000
	bne     L2Enabled

	bl		L2Init
	bl		L2Enable

L2Enabled:
	# Restore the non-volatile registers to their previous values and return.
	lwz     r0, 20(sp)
	lwz     r31, 12(sp)
	addi    sp, sp, 16
	mtlr    r0
	blr

	.globl __SystemInit
__SystemInit:
	mflr    r0
	stw     r0, 4(sp)
	stwu    sp, -24(sp)
	stw     r31, 20(sp)
	stw     r30, 16(sp)
	stw     r29, 12(sp)

	# Clear various SPR's
	li      r3,0
	mtspr   952, r3
	mtspr   956, r3
	mtspr   953, r3
	mtspr   954, r3
	mtspr   957, r3
	mtspr   958, r3

#if 0
	lis		r3,0x8390		//bits set: H4A(HID4 access), SBE(2nd BAT enabled),  SR0(store 0), LPE(PS LE exception), L2CFI(L2 castout prior to L2 inv. flash)
	mtspr	HID4,r3
#endif

	# Disable Speculative Bus Accesses to non-guarded space from both caches.
	mfspr   r3, HID0
	ori     r3, r3, 0x0200
	mtspr   HID0, r3

	mfspr   r3,HID2 # (HID2)
	rlwinm  r3, r3, 0, 2, 0
	mtspr   HID2,r3 # (HID2)

	# Restore the non-volatile registers to their previous values and return.
	lwz     r0, 28(sp)
	lwz     r31,20(sp)
	lwz     r30,16(sp)
	lwz     r29,12(sp)
	addi    sp, sp, 24
	mtlr    r0
	blr


	.global systemcallhandler_start,systemcallhandler_end
systemcallhandler_start:
	mfspr	r3,HID0
	ori		r4,r3,0x0008
	mtspr	HID0,r4
	isync
	sync
	mtspr	HID0,r3
	rfi
systemcallhandler_end:
	nop
