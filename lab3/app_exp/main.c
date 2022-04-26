#include "lib.h"
#include "pthread.h"
#include "types.h"

void ThreadCreate(int i)
{
	printf("ThreadCreate is operating with argument %d\n",i);
    for(;i>0;i--){
		printf("Thread ping with data %d\n", i);
		sleep(120);
	}
	exit(0);
}

void ThreadJoin(){
	printf("arg is NULL, test pthread_join, sleep 120 tik-tok.\n");
	sleep(120);
	printf("Thread join awake.\n");
	pthread_exit("Pthread_exit from ThreadFun.\n");
}

int main(int argc, char *argv[]) {
	//test exec with arguments
	printf("\nExec with %d arguments, sleep 120 tik-tok.\n",argc);
	sleep(120);
	if(argc == 0){
		printf("There is no argv, sleep 120 tik-tok.\n");
		sleep(120);
	}
	else{
		printf("\n%s",argv[0]);
		printf("The exec with argumets works, sleep 240 tik-tok.\n");
		sleep(240);
		printf("\nExpeirment challenge begins:\n");
		printf("Include kernel thread, exec with arguments and page memory-management.\n");
		int i = 3;
		printf("Test thread create with arguments and dispatch by round robin.\n");
		pthread_t tid1;
		int error = pthread_create(tid1,&ThreadCreate,1,i);
		if(error!=0){
			printf("The PTHREAD_ENABLED macro is not set, sleep 900 tik-tok.\n");
			sleep(900);
		}
		else{
			for(;i>0;i--){
				printf("Process ping with data %d\n", i);
				sleep(120);
			}
			printf("Test for thread create and dispatching by RR is passed, sleep 240 tik-tok.\n");
			sleep(240);
		}
		printf("\nTest pthread_join:\n");
		pthread_t tid2;
		error = pthread_create(tid2,&ThreadJoin,0);
		if(error!=0){
			printf("The PTHREAD_ENABLED macro is not set, sleep 600 tik-tok.\n");
			sleep(600);
		}
		else{
			void* threadRes;
			error = pthread_join(tid2,&threadRes);
			printf("pthread_join finished.");
			if (error != 0) {
       		 	printf("pthread_join failed.\n");
			}
			else{
				printf("The string return from pthread_exit:\n%s\n",(char*)threadRes);
				sleep(360);
			}
    	}
		printf("\nExec app_print after thread test is finished, sleep 240 tik-tok.\n\n");
		sleep(240);
	}
	exec(231, 30, 0);
	exit(0);
	return 0;
}
