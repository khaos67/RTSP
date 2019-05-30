#ifndef __MY_SEMAPHORE_H__
#define __MY_SEMAPHORE_H__

#ifdef WIN32
#include <windows.h>
#define SEMAPHORE	HANDLE
#else
#include <semaphore.h>
#define SEMAPHORE	sem_t
#endif

int SEM_INIT(SEMAPHORE *sem, int init, int max);
int SEM_WAIT(SEMAPHORE *sem);
int SEM_POST(SEMAPHORE *sem);
int SEM_DESTROY(SEMAPHORE *sem);

#endif
