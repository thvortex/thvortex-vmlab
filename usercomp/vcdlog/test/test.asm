; ******************************************************
; BASIC .ASM template file for AVR
; ******************************************************

.include "C:\VMLAB\include\m8def.inc"

.def data = r16
.def max = r17

.org 0
	rjmp begin

.org 19

begin:
	ldi data, 0x0f
	out DDRD, data
	clr data
	ldi max, 0x10
	
loop:
	out PORTD, data
	inc data
	cp data, max
	brne loop

halt:
	rjmp halt





