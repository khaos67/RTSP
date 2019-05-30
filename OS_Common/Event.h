#ifndef __EVENT_H__
#define __EVENT_H__

#ifdef WIN32
#include <windows.h>
#define	EVENT	HANDLE
#else
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool triggered;
} mrevent;

#define EVENT	mrevent
#endif

void EVENT_INIT(EVENT *event);
void EVENT_DESTROY(EVENT *event);
void EVENT_WAIT(EVENT *event);
void EVENT_SET(EVENT *event);
void EVENT_RESET(EVENT *event);

#endif
