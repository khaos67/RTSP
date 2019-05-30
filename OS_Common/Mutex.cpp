#include "Mutex.h"

int MUTEX_INIT(MUTEX *mutex)
{
#ifdef WIN32
	*mutex = CreateMutex(NULL, FALSE, NULL);
	return *mutex == NULL ? -1 : 0;
#else
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	return pthread_mutex_init(mutex, &attr);
#endif
}

int MUTEX_DESTROY(MUTEX *mutex)
{
#ifdef WIN32
	if (*mutex) {
		CloseHandle(*mutex);
		*mutex = NULL;
	}
	return 0;
#else
	return pthread_mutex_destroy(mutex);
#endif
}

int MUTEX_LOCK(MUTEX *mutex)
{
#ifdef WIN32
	return WaitForSingleObject(*mutex, INFINITE) == WAIT_FAILED ? -1 : 0;
#else
	return pthread_mutex_lock(mutex);
#endif
}

int MUTEX_UNLOCK(MUTEX *mutex)
{
#ifdef WIN32
	return ReleaseMutex(*mutex) == 0 ? -1 : 1;
#else
	return pthread_mutex_unlock(mutex);
#endif
}
