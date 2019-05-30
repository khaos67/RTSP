#include "Thread.h"

int THREAD_CREATE(THREAD *thread, THREAD_FUNC func(void *), void *param)
{
#ifdef WIN32
	*thread = (HANDLE)_beginthreadex(NULL, 0, func, param, 0, NULL);
	return *thread == NULL ? -1 : 0;
#else
	return pthread_create(thread, NULL, func, param);
#endif
}

int THREAD_JOIN(THREAD *thread)
{
#ifdef WIN32
	return WaitForSingleObject(*thread, INFINITE) == WAIT_FAILED ? -1 : 0;
#else
	int status;
	return pthread_join(*thread, (void **)&status);
#endif
}

void THREAD_DESTROY(THREAD *thread)
{
#ifdef WIN32
	if (*thread)
		CloseHandle(*thread);
#endif
	*thread = NULL;
}
