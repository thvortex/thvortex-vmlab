; ******************************************************
; BASIC .ASM template file for AVR
; ******************************************************

.include "C:\VMLAB\include\m8def.inc"

.def data = r16

.org 0
	rjmp begin

.org 19

begin:
	ldi data, 0xff
	out DDRD, data

loop:
	in data, PINB
	out PORTD, data
	rjmp loop



