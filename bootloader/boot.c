#include "boot.h"

#define SECTSIZE 512

void bootMain(void) {
	//In Makefile the "cat" command catenates bootloader.bin and app.bin together,
	//and print the result file to os.img as a simulation of hard disk.
	//Bootloader.bin and app.bin are in the same size of 512 bytes,
	//so bootlader.bin is in sector 0 and app.bin is sector 1.
	readSect((void*)0x8c00,1);
	//After app.bin has been read, directly jmp to 0x8c00 by volatile asm
	asm ("movw $0x8c00 ,%ax\n\t"
	     "jmp %ax");

	//Anther asm method: indirect jmp
	//asm volatile ("jmp 0x8c00");
	


	/*another method: use function ptr*/
	//void (*appPtr)(void) = (void(*)(void)) 0x8c00;
	//readSect(appPtr, 1);
	//appPtr();	
}

void waitDisk(void) { // waiting for disk
	while((inByte(0x1F7) & 0xC0) != 0x40);
}

void readSect(void *dst, int offset) { // reading a sector of disk
	int i;
	waitDisk();
	outByte(0x1F2, 1);
	outByte(0x1F3, offset);
	outByte(0x1F4, offset >> 8);
	outByte(0x1F5, offset >> 16);
	outByte(0x1F6, (offset >> 24) | 0xE0);
	outByte(0x1F7, 0x20);

	waitDisk();
	for (i = 0; i < SECTSIZE / 4; i ++) {
		((int *)dst)[i] = inLong(0x1F0);
	}
}
