#include "lib.h"
#include "types.h"

#define TEST_PTHREAD
#define TEST_EXEC

int uEntry(void) {
	uint16_t selector;
	//uint16_t selector = 16;
	asm volatile("movw %%ss, %0":"=m"(selector)); //XXX necessary or not, iret may reset ds in QEMU
	asm volatile("movw %%ax, %%ds"::"a"(selector));
	
	int data = 0;	
	
	int ret = fork();
	int i = 8;
	if (ret == 0) {
		data = 2;
		while( i != 0) {
			i --;
			printf("Child Process: Pong %d, %d;\n", data, i);
			//sleep(1);
			sleep(60);
		}
// app_exp in disk 241-260. Test exec with argument and kernel thread library.
#ifdef TEST_PTHREAD
		exec(241, 20, 1, "Test kernel thread library.\n");
#elif defined TEST_EXEC
		exec(241, 20, 0); //argc
#else
		exec(221, 20, 0);
#endif
		exit(0);
	}
	else if (ret != -1) {
		data = 1;
		while( i != 0) {
			i --;
			printf("Father Process: Ping %d, %d;\n", data, i);
			//sleep(1);
			sleep(60);
		}
		exit(0);
	}

	return 0;
}
