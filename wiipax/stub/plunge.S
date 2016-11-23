	.text
	.section .text
	.globl _plunge

_plunge:
	isync
	mtsrr0 3
	
	mfmsr 3
	li 4,0x30
	andc 3,3,4
	mtsrr1 3
	rfi
