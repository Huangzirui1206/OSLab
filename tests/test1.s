# test1.s 
 .section .data 
 .section .text 
 .globl _start 
 _start: 
     movl $1, %eax 
     movl $3, %ebx 
     int $0x80
