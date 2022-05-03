#ifndef __lib_h__
#define __lib_h__

#include "types.h"

#define SYS_WRITE 0
#define SYS_READ 1
#define SYS_FORK 2
#define SYS_EXEC 3
#define SYS_SLEEP 4
#define SYS_EXIT 5
#define SYS_SEM 6
#define SYS_CRI_MEM 7

#define STD_OUT 0
#define STD_IN 1

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

// For writer and reader problem, sitimulate the critical area.
#define CR_RCNT_R 0
#define CR_RCNT_INC 1
#define CR_RCNT_DEC 2
#define CR_FILE_W 3
#define CR_FILE_R 4
#define CR_WCNT_R 5
#define CR_WCNT_INC 6
#define CR_WCNT_DEC 7


#define MAX_BUFFER_SIZE 256

int printf(const char *format,...);

int scanf(const char *format,...);

pid_t fork_share();

pid_t fork();

int exec(void (*func)(void));

int sleep(uint32_t time);

int exit();

int sem_init(sem_t *sem, uint32_t value);

int sem_wait(sem_t *sem);

int sem_post(sem_t *sem);

int sem_destroy(sem_t *sem);


// For writer and reader problem -- visit critical memory
// Visit critical area
int read_rcnt();
int inc_rcnt();
int dec_rcnt();
int read_wcnt();
int inc_wcnt();
int dec_wcnt();
int write_file(const char* src);
int read_file(char* dst);
#endif
