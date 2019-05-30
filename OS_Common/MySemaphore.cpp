#include "MySemaphore.h"

int SEM_INIT(SEMAPHORE *sem, int init, int max)
{
#ifdef WIN32
	*sem = CreateSemaphore(NULL, init, max, NULL);
	return *sem == NULL ? -1 : 0;
#else
	return sem_init(sem, 0, init);
#endif
}

int SEM_DESTROY(SEMAPHORE *sem)
{
#ifdef WIN32
	if (*sem) {
		CloseHandle(*sem);
		*sem = NULL;
	}
	return 0;
#else
	return sem_destroy(sem);
#endif
}

int SEM_WAIT(SEMAPHORE *sem)
{
#ifdef WIN32
	return WaitForSingleObject(*sem, INFINITE);
#else
	return sem_wait(sem);
#endif
}

int SEM_POST(SEMAPHORE *sem)
{
#ifdef WIN32
	return ReleaseSemaphore(*sem, 1, NULL) == TRUE ? 0 : -1;
#else
	return sem_post(sem);
#endif
}
