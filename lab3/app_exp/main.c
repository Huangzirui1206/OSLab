#include "lib.h"
#include "pthread.h"
#include "types.h"

void ThreadCreate(int i)
{
	printf("ThreadCreate is operating.\n");
    for(;i>0;i--){
		printf("Thread ping with data %d\n", i);
		sleep(120);
	}
	exit(0);
}

void ThreadJoin(){
	printf("arg is NULL, test pthread_join.\n");
	sleep(120);
	pthread_exit("Pthread_exit from ThreadFun.\n");
}

int main(int argc, char *argv[]) {
	//test exec with arguments
	printf("Exec with %d arguments, sleep 120 tik-tok.\n",argc);
	sleep(120);
	if(argc == 0){
		printf("There is no argv, sleep 120 tik-tok.\n");
		sleep(120);
	}
	else{
		printf("%s",argv[0]);
		printf("The exec with argumets works, sleep 240 tik-tok.\n");
		sleep(230);
		printf("Expeirment challenge begins:\n");
		printf("Include kernel thread, exec with arguments and page memory-management.\n");
		int i = 3;
		printf("Test thread create with arguments and dispatch by round robin.\n");
		pthread_t tid1;
		int error = pthread_create(tid1,&ThreadCreate,1,i);
		if(error!=0){
			printf("The PTHREAD_ENABLED macro is not set.\n");
		}
		for(;i>0;i--){
			printf("Process ping with data %d\n", i);
			sleep(120);
		}
		printf("Test for thread create and dispatching by RR is passed, sleep 600 tik-tok.\n");
		sleep(600);
		printf("Test pthread_join:\n");
		pthread_t tid2;
		error = pthread_create(tid2,&ThreadCreate,0);
		if(error!=0){
			printf("The PTHREAD_ENABLED macro is not set.\n");
		}
		else{
			void* threadRes;
			error = pthread_join(tid2,&threadRes);
			if (error != 0) {
       		 	printf("pthread_join failed.\n");
			}
			else{
				printf("%s\n",(char*)threadRes);
			}
    	}
		printf("Exec app_print after thread test is finished.\n");
	}
	exec(221, 20, 0);
	exit(0);
	return 0;
}
