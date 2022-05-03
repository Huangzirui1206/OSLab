#include "lib.h"
#include "types.h"
#define N 5

//#define test0
//#define test_Phi
//#define test_PC
/*default read-first, set WRITE_FIRST can change to write first*/
#define test_RW

#ifdef test_RW
//#define WRITER_FIRST
#endif

#ifdef test_Phi
void philosopher(int cnt,int i,sem_t* forks){
	int ori = cnt;
	while(cnt){
		printf("Philosopher %d: think\n", i);
		sleep(128);
		if(i%2){
			sem_wait(forks+i);
			sleep(128);
			sem_wait(forks+(i+1)%5);
			sleep(128);
			printf("Philosopher %d: eat (%d/%d)\n", i,ori- cnt+1,ori);
			sleep(128);
			sem_post(forks+i);
			sleep(128);
			sem_post(forks+(i+1)%5);
			sleep(128);
		}
		else{
			sem_wait(forks+(i+1)%5);
			sleep(128);
			sem_wait(forks+i);
			sleep(128);
			printf("Philosopher %d: eat %d\n", i, cnt);
			sleep(128);
			sem_post(forks+(i+1)%5);
			sleep(128);
			sem_post(forks+i);
			sleep(128);
		}
		cnt--;
	}
}
#elif defined test_PC
void consumer(int i,int cnt, sem_t* mutex, sem_t* full,sem_t* empty){
	int ori = cnt;
	while(cnt){
		sem_wait(full);
		sleep(128);
		sem_wait(mutex);
		sleep(128);
		printf("Consumer %d : consume (%d/%d)\n",i,ori - cnt + 1, ori);
		sleep(128);
		sem_post(mutex);
		sleep(128);
		sem_post(empty);
		sleep(128);
		cnt--;
	}
}
void producer(int i,int cnt,sem_t* mutex,sem_t* full, sem_t* empty){
	int ori = cnt;
	while(cnt){
		sem_wait(empty);
		sleep(128);
		sem_wait(mutex);
		sleep(128);
		printf("Producer %d: produce  (%d/%d)\n", i,ori - cnt + 1, ori);
		sleep(128);
		sem_post(mutex);
		sleep(128);
		sem_post(full);
		sleep(128);
		cnt--;
	}
}
#elif defined test_RW
char file[128] = {'\0'};
int readCount = 0;
sem_t writeblock = 0,mutex = 0;
#ifdef WRITER_FIRST
sem_t queue = 0;
int writeCount = 0;
#endif
void initFile(){
	char* str = "Writer and Reader problem.";
	int i = 0;
	while(str[i]){
		file[i] = str[i];	
		i++;
	}
	file[i] = '\0';
	write_file(file);
}
void writeFile(char* str, int idx){
	int i = 0;
	while(str[i]){
		file[i] = str[i];	
		i++;
	}
	file[i] = '0'+idx;
	i++;
	file[i] = '\0';
	write_file(file);
}
void reader(int i,int cnt){
	int ori = cnt;
	while(cnt){
#ifdef WRITER_FIRST
		sem_wait(&queue);
#else
		sem_wait(&mutex);
#endif
		sleep(36);
		if(read_rcnt() == 0){
			sem_wait(&writeblock);
		}
		readCount = inc_rcnt();
		sleep(36);
#ifdef WRITER_FIRST
		sem_post(&queue);
#else
		sem_post(&mutex);
#endif
		sleep(36);
		read_file(file);
		printf("Reader %d(%d/%d): read, total %d reader, file: %s\n", i,ori-cnt+1,ori, readCount,file);
		sleep(36);
		sem_wait(&mutex);
		sleep(36);
		readCount = dec_rcnt();
		if(readCount == 0){
			sem_post(&writeblock);
		}
		sleep(36);
		sem_post(&mutex);
		sleep(36);
		cnt--;
	}
}
void writer(int i,int cnt){
	int ori = cnt;
	while(cnt){
#ifdef WRITER_FIRST
		sem_wait(&mutex);
		if(read_wcnt() == 0){
			//printf("queue %d mutex %d\n",queue, mutex);
			sem_wait(&queue);
		}
		writeCount = inc_wcnt();
		sem_post(&mutex);
#endif
		sem_wait(&writeblock);
		sleep(36);
		if((ori-cnt+1)%2)writeFile("cnt%%2==1, file changed by writer ", i);
		else writeFile("cnt%%2==0, file changed by writer ", i);
		printf("Writer %d(%d/%d): write -- %s\n", i,ori-cnt+1,ori, file);
		sleep(36);
		sem_post(&writeblock);
		sleep(36);
#ifdef WRITER_FIRST
		sem_wait(&mutex);
		writeCount = dec_wcnt();
		if(writeCount == 0){
			sem_post(&queue);
		}
		sem_post(&mutex);
#endif
		cnt--;
	}
}
#endif

int uEntry(void) {
#ifdef test0
	// 测试scanf	
	int dec = 0;
	int hex = 0;
	char str[6];
	char cha = 0;
	int ret = 0;
	while(1){
		printf("Input:\" Test %%c Test %%6s %%d %%x\"\n");
		ret = scanf(" Test %c Test %6s %d %x", &cha, str, &dec, &hex);
		printf("Ret: %d; %c, %s, %d, %x.\n", ret, cha, str, dec, hex);
		if (ret == 4)
			break;
	}
	
	// 测试信号量
	int i = 4;
	sem_t sem;
	printf("Father Process: Semaphore Initializing.\n");
	ret = sem_init(&sem, 0);
	if (ret == -1) {
		printf("Father Process: Semaphore Initializing Failed.\n");
		exit();
	}

	ret = fork();
	if (ret == 0) {
		while( i != 0) {
			i --;
			printf("Child Process: Semaphore Waiting.\n");
			sem_wait(&sem);
			printf("Child Process: In Critical Area.\n");
		}
		printf("Child Process: Semaphore Destroying.\n");
		sem_destroy(&sem);
		exit();
	}
	else if (ret != -1) {
		while( i != 0) {
			i --;
			printf("Father Process: Sleeping.\n");
			sleep(128);
			printf("Father Process: Semaphore Posting.\n");
			sem_post(&sem);
		}
		printf("Father Process: Semaphore Destroying.\n");
		sem_destroy(&sem);
		exit();
	}

	
#elif defined test_Phi
	// For lab4.3
	//哲学家
	int i = 0;
	int cnt = 2;
	sem_t forks[N];
	for(int j = 0;j<N;j++)sem_init(forks+j,1);
	int ret = fork();
	if(ret == 0){
		i++;
		ret = fork();
		if(ret == 0){
			i++; 
			ret = fork();
			if(ret == 0){
				i++;
				ret = fork();
				if(ret == 0){
					i++;
				}
			}
		}
	}
	printf("Philosopher %d is ready.\n", i); 
	sleep(128);
	philosopher(cnt,i,forks);
	printf("Philosopher %d left table.\n", i); 
	exit();
	//生产者消费者问题
#elif defined test_PC
	int i = 0;
	int k = 1;
	int cnt = 2;
	sem_t empty,full,mutex;
	sem_init(&empty,k); // spare space is k, default 1
	sem_init(&full, 0);
	sem_init(&mutex,1);
	int ret = fork();
	if(ret == 0){
		i++;
		ret = fork();
		if(ret == 0){
			i++; 
			ret = fork();
			if(ret == 0){
				i++;
				ret = fork();
				if(ret == 0){
					i++;
				}
			}
		}
		// producer
		printf("Producer %d is ready\n",i);
		sleep(128);
		producer(i,cnt,&mutex,&full,&empty);
		printf("Producer %d finished work.\n",i);
	}
	else if(ret!=-1){
		// consumer
		printf("Consumer %d is ready\n",i);
		sleep(128);
		consumer(i,cnt*(N-1),&mutex,&full,&empty);
		printf("Consumer finished work.\n");
	}
	exit();
	//读者写者问题
#elif defined test_RW
	int i = 0;
	int cnt = 3;
#ifdef WRITER_FIRST
	sem_init(&queue,1);
#endif
	sem_init(&writeblock, 1);
	sem_init(&mutex,1);
	//printf("queue %d, mutex %d, writeblock %d\n", queue, mutex,writeblock);
	initFile();
	printf("File init:  %s\n",file);
	printf("Rcnt init: %d\n",read_rcnt());
#ifdef WRITER_FIRST
	printf("Wcnt init: %d\n", read_wcnt());
	printf(" ------ Writer First: ------\n");
#else
	printf(" ------ Reader First ------\n");
#endif
	int ret = fork();
	if(ret == 0){
		i++;
		ret = fork();
		if(ret == 0){
			i++; 
			ret = fork();
			if(ret == 0){
				i++;
				ret = fork();
				if(ret == 0){
					i++;
				}
			}
			// reader
			printf("Reader %d is ready\n",i);
			sleep(16);
			reader(i,cnt);
			printf("Reader %d finished work.\n",i);
		}
		else{
			// writer
			printf("Writer %d is ready\n",i);
			sleep(16*N);	
			writer(i,cnt*2);
			printf("Writer %d finished work.\n", i);
		}
	}
	else if(ret!=-1){
		// writer
		printf("Writer %d is ready\n",i);
		sleep(16*N);	
		writer(i,cnt*2);
		printf("Writer %d finished work.\n", i);
	}
	exit();
#endif

	exit(0);
	return 0;
}
