#ifndef __pthread_h__
#define __pthread_h__

#include "types.h"

struct pthread_t
{
   uint32_t* p; // record the address of tcb in kernel 
   uint32_t x;
};
typedef struct pthread_t pthread_t;

int pthread_create(pthread_t tid, void* func, uint32_t argNum, ...);

int pthread_exit(void  *retval);

int pthread_join(pthread_t tid, void** value_ptr);

#endif