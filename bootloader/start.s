
/* Protected Mode Hello World */
#.code16
#
#.global start
#start:
#	movw %cs, %ax
#	movw %ax, %ds
#	movw %ax, %es
#	movw %ax, %ss
#
#.code32
#start32:
#	movw $0x10, %ax # setting data segment selector
#	movw %ax, %ds
#	movw %ax, %es
#	movw %ax, %fs
#	movw %ax, %ss
#loop32:
#	jmp loop32
#
#
#.p2align 2



