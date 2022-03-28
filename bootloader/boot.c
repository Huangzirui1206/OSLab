#include "boot.h"

#define SECTSIZE 512
#define PT_LOAD 1

void bootMain(void)
{
	int i = 0;
	int phoff = 0x34;
	unsigned int elf = 0x100000;	// Take care of the problem of covering the original data
	void (*kMainEntry)(void);
	kMainEntry = (void (*)(void))0x100000;
	int offset = 0x1000;
	int filesz = 0x2000;

	for (i = 0; i < 200; i++)
	{
		readSect((void *)(elf + i * 512), 1 + i);
	}

	// TODO: 填写kMainEntry、phoff、offset...... 然后加载Kernel（可以参考NEMU的某次lab）
	// If we use codes to analyse this elf, the boot block wiil be too large.
	// So just use readelf to analyse elf outside, and fill the variables by hand.
	// Load .text only.
	ELFHeader* eh=(ELFHeader*)elf;
    kMainEntry=(void(*)(void))(eh->entry);
    phoff=eh->phoff;
    ProgramHeader* ph=(ProgramHeader*)(elf+phoff);
    offset=ph->off;
	filesz = ph->filesz;
	for (i = 0; i < filesz; i++) {
        *(unsigned char *)(elf + i) = *(unsigned char *)(elf + i + offset);
    }
	kMainEntry();
}

void waitDisk(void)
{ // waiting for disk
	while ((inByte(0x1F7) & 0xC0) != 0x40)
		;
}

void readSect(void *dst, int offset)
{ // reading a sector of disk
	int i;
	waitDisk();
	outByte(0x1F2, 1);
	outByte(0x1F3, offset);
	outByte(0x1F4, offset >> 8);
	outByte(0x1F5, offset >> 16);
	outByte(0x1F6, (offset >> 24) | 0xE0);
	outByte(0x1F7, 0x20);

	waitDisk();
	for (i = 0; i < SECTSIZE / 4; i++)
	{
		((int *)dst)[i] = inLong(0x1F0);
	}
}
