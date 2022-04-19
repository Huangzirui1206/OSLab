#include "x86.h"
#include "device.h"

void initSerial(void) {
	outByte(SERIAL_PORT + 1, 0x00);
	outByte(SERIAL_PORT + 3, 0x80);
	outByte(SERIAL_PORT + 0, 0x01);
	outByte(SERIAL_PORT + 1, 0x00);
	outByte(SERIAL_PORT + 3, 0x03);
	outByte(SERIAL_PORT + 2, 0xC7);
	outByte(SERIAL_PORT + 4, 0x0B);
}

static inline int serialIdle(void) {
	return (inByte(SERIAL_PORT + 5) & 0x20) != 0;
}

void putChar(char ch) {
	while (serialIdle() != TRUE);
	outByte(SERIAL_PORT, ch);
}

void putStr(const char* p){
	while(*p) putChar(*(p++));
}

void putNum(int num){  
    static char buf[30] = { 0 };  
    char* p = buf + sizeof(buf) - 1;  
    if (num == 0){ putChar('0'); return;}  
    if (num < 0){ putChar('-'); num = -num;}  
    while(num){  
	     *--p = (num % 10) + '0';  
         num /= 10;  
    }  
    while(*p) putChar(*(p++));  
} 

void putNumX(uint32_t num){
	 static char buf[30] = { 0 };  
    char* p = buf + sizeof(buf) - 1;  
    if (num == 0){ putChar('0'); return;}  
   // if (num < 0){ putChar('-'); num = -num;}  
    while(num){ 
		int i =  (num % 16);
		if(i<10){
			*--p = i + '0';  
		}
	    else{
			*--p = i - 10 + 'a';  
		}
         num /= 16;  
    } 
	putStr("0x"); 
    while(*p) putChar(*(p++));  
}
