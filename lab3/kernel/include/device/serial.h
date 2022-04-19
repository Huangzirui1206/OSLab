#ifndef __SERIAL_H__
#define __SERIAL_H__

void initSerial(void);
void putChar(char);
void putNum(int);
void putNumX(uint32_t);
void putStr(const char*);
#define SERIAL_PORT  0x3F8

#endif
