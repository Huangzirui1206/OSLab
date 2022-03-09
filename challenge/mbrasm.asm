;mbr.asm file
;The asm format is <dst> <src>
;While the i386 format is <src> <dst>

;---------------|
;Under real mode, set BITS 16
;BITS 16

;------------------------------------------------------
;org <addr> addr is the base address of this segment  |
org 7c00h

mov ax, cs
mov es, ax
mov bp, message
mov cx, msglen
mov ax, 1301h
mov bx ,000ch
mov dx, 0000h
int 10h
loop:
 jmp loop

;------------------------------
;10 is '\n', and 14 is '\r'  |
message: db "Hello, World!",10,13
msglen: equ $ -message
times 510 - ($ - $$) db 0	;fill the .bin file with 0 to 510 B
dw 0aa55h		;set magic number 0x55aa in little-endian

