#ifndef __SERIAL_H__
#define __SERIAL_H__

#define DEBUG
void initSerial(void);
void putChar(char);
void putString(const char *);
void putInt(int);

void eax_get_eip();
void putNumX(uint32_t);
void putStr(const char* p);

#endif
