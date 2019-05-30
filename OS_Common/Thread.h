#ifndef __THREAD_H__
#define __THREAD_H__

#ifdef WIN32
#include <windows.h>
#include <process.h>

#define THREAD		HANDLE
#define THREAD_FUNC	unsigned __stdcall
#else
#include <pthread.h>

#define THREAD		pthread_t
#define THREAD_FUNC	void*
#endif

int THREAD_CREATE(THREAD *thread, THREAD_FUNC func(void *), void *param);
int THREAD_JOIN(THREAD *thread);
void THREAD_DESTROY(THREAD *thread);

#endif
