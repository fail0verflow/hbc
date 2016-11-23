	.globl exception_asm_start, exception_asm_end

exception_asm_start:
	# store all interesting regs
	mfcr 0 ;  stw 0,0x3580(0)
	mfxer 0 ; stw 0,0x3584(0)
	mflr 0 ;  stw 0,0x3588(0)
	mfctr 0 ; stw 0,0x358c(0)
	mfsrr0 0 ;  stw 0,0x3590(0)
	mfsrr1 0 ;  stw 0,0x3594(0)
	mfdar 0 ;   stw 0,0x3598(0)
	mfdsisr 0 ; stw 0,0x359c(0)

	# switch on FP, DR, IR
	mfmsr 0 ; ori 0,0,0x2030 ; mtsrr1 0

	# go to C handler
	lis 0,exception_handler@h ; ori 0,0,exception_handler@l ; mtsrr0 0
	rfi
exception_asm_end:

