#include "Event.h"

void EVENT_INIT(EVENT *event)
{
#ifdef WIN32
	*event = CreateEvent(NULL, TRUE, FALSE, NULL);
#else
	pthread_mutex_init(&event->mutex, 0);
	pthread_cond_init(&event->cond, 0);
	event->triggered = false;
#endif
}

void EVENT_DESTROY(EVENT *event)
{
#ifdef WIN32
	CloseHandle(*event);
#else
	pthread_mutex_destroy(&event->mutex);
	pthread_cond_destroy(&event->cond);
#endif
}

void EVENT_WAIT(EVENT *event)
{
#ifdef WIN32
	WaitForSingleObject(*event, INFINITE);
#else
	pthread_mutex_lock(&event->mutex);
	while (!event->triggered)
		pthread_cond_wait(&event->cond, &event->mutex);
	pthread_mutex_unlock(&event->mutex);
#endif
}

void EVENT_SET(EVENT *event)
{
#ifdef WIN32
	SetEvent(*event);
#else
    pthread_mutex_lock(&event->mutex);
    event->triggered = true;
    pthread_cond_signal(&event->cond);
    pthread_mutex_unlock(&event->mutex);
#endif
}

void EVENT_RESET(EVENT *event)
{
#ifdef WIN32
	ResetEvent(*event);
#else
	pthread_mutex_lock(&event->mutex);
	event->triggered = false;
	pthread_mutex_unlock(&event->mutex);
#endif
}
