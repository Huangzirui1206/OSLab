#include "common.h"
#include "x86.h"
#include "device.h"

#define BLUE_SCREEN_TEXT "Assertion failed: " 

static void displayMessage(const char *file, int line) {  
    putStr((char*)BLUE_SCREEN_TEXT);  
	putStr((char*)file);  
	putChar(':');  
	putNum(line);  
	putChar('\n');  
}  


int abort(const char *fname, int line) {
	disableInterrupt();
	displayMessage(fname, line);
	while (TRUE) {
		waitForInterrupt();
	}
}
