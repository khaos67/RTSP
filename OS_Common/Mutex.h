#ifndef __MUTEX_H__
#define __MUTEX_H__

#ifdef WIN32
#include <windows.h>
#include <process.h>

#define MUTEX	HANDLE
#define PTHREAD_MUTEX_INITIALIZER	CreateMutex(NULL, FALSE, NULL)
#else
#include <pthread.h>

#define MUTEX	pthread_mutex_t
#endif

int MUTEX_INIT(MUTEX *mutex);
int MUTEX_LOCK(MUTEX *mutex);
int MUTEX_UNLOCK(MUTEX *mutex);
int MUTEX_DESTROY(MUTEX *mutex);

#endif
