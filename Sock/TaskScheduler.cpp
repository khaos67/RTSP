#include "TaskScheduler.h"
#include "RTSPCommonEnv.h"
#include <stdio.h>

THREAD_FUNC DoEventThread(void* lpParam)
{
	TaskScheduler *scheduler = (TaskScheduler *)lpParam;
	scheduler->doEventLoop();
	return 0;
}

TaskScheduler::TaskScheduler()
{
	fTaskLoop = 0;
	MUTEX_INIT(&fMutex);
	FD_ZERO(&fReadSet);
	fMaxNumSockets = 0;
	fThread = NULL;
	fReadHandlers = new HandlerSet();
}

TaskScheduler::~TaskScheduler()
{
	stopEventLoop();

	delete fReadHandlers;

	THREAD_DESTROY(&fThread);

	MUTEX_DESTROY(&fMutex);
}

void TaskScheduler::taskLock()
{
	MUTEX_LOCK(&fMutex);
}

void TaskScheduler::taskUnlock()
{
	MUTEX_UNLOCK(&fMutex);
}

void TaskScheduler::turnOnBackgroundReadHandling(int socketNum, BackgroundHandlerProc* handlerProc, void *clientData) 
{
	taskLock();

	if (socketNum < 0) goto exit;

	FD_SET((unsigned)socketNum, &fReadSet);
	fReadHandlers->assignHandler(socketNum, handlerProc, clientData);

	if (socketNum+1 > fMaxNumSockets) {
		fMaxNumSockets = socketNum+1;
	}

exit:
	taskUnlock();
}

void TaskScheduler::turnOffBackgroundReadHandling(int socketNum) 
{
	taskLock();

	if (socketNum < 0) goto exit;

	FD_CLR((unsigned)socketNum, &fReadSet);
	fReadHandlers->removeHandler(socketNum);

	if (socketNum+1 == fMaxNumSockets) {
		--fMaxNumSockets;
	}

exit:
	taskUnlock();
}

int TaskScheduler::startEventLoop()
{
	if (fTaskLoop != 0)
		return -1;

	fTaskLoop = 1;
	THREAD_CREATE(&fThread, DoEventThread, this);
	if (!fThread) {
		DPRINTF("failed to create event loop thread\n");
		fTaskLoop = 0;
		return -1;
	}

	return 0;
}

void TaskScheduler::stopEventLoop()
{
	fTaskLoop = 0;

	THREAD_JOIN(&fThread);
	THREAD_DESTROY(&fThread);
}

void TaskScheduler::doEventLoop() 
{
	while (fTaskLoop)
	{
		SingleStep();
	}
}

void TaskScheduler::SingleStep()
{
	taskLock();

	fd_set readSet = fReadSet;

	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	int selectResult = select(fMaxNumSockets, &readSet, NULL, NULL, &timeout);
	if (selectResult < 0) {
		int err = WSAGetLastError();
#ifdef WIN32
		// For some unknown reason, select() in Windoze sometimes fails with WSAEINVAL if
		// it was called with no entries set in "readSet".  If this happens, ignore it:
		if (err == WSAEINVAL && readSet.fd_count == 0) {
			err = 0;
			// To stop this from happening again, create a dummy readable socket:
			int dummySocketNum = socket(AF_INET, SOCK_DGRAM, 0);
			FD_SET((unsigned)dummySocketNum, &fReadSet);
		}
#endif
		if (err != 0) {
			// Unexpected error - treat this as fatal:
			//DPRINTF("TaskScheduler::SingleStep(): select() fails");
			//	exit(0);
		}
	}

	// Call the handler function for one readable socket:
	HandlerIterator iter(*fReadHandlers);
	HandlerDescriptor* handler;
	// To ensure forward progress through the handlers, begin past the last
	// socket number that we handled:
	if (fLastHandledSocketNum >= 0) {
		while ((handler = iter.next()) != NULL) {
			if (handler->socketNum == fLastHandledSocketNum) break;
		}
		if (handler == NULL) {
			fLastHandledSocketNum = -1;
			iter.reset(); // start from the beginning instead
		}
	}

	while ((handler = iter.next()) != NULL) {
		if (FD_ISSET(handler->socketNum, &readSet) &&
			FD_ISSET(handler->socketNum, &fReadSet) /* sanity check */ &&
			handler->handlerProc != NULL) {
				fLastHandledSocketNum = handler->socketNum;
				// Note: we set "fLastHandledSocketNum" before calling the handler,
				// in case the handler calls "doEventLoop()" reentrantly.
				(*handler->handlerProc)(handler->clientData, SOCKET_READABLE);
				break;
		}
	}

	if (handler == NULL && fLastHandledSocketNum >= 0) {
		// We didn't call a handler, but we didn't get to check all of them,
		// so try again from the beginning:
		iter.reset();
		while ((handler = iter.next()) != NULL) {
			if (FD_ISSET(handler->socketNum, &readSet) &&
				FD_ISSET(handler->socketNum, &fReadSet) /* sanity check */ &&
				handler->handlerProc != NULL) {
					fLastHandledSocketNum = handler->socketNum;
					// Note: we set "fLastHandledSocketNum" before calling the handler,
					// in case the handler calls "doEventLoop()" reentrantly.
					(*handler->handlerProc)(handler->clientData, SOCKET_READABLE);
					break;
			}
		}
		if (handler == NULL) fLastHandledSocketNum = -1;//because we didn't call a handler
	}

	taskUnlock();
#ifndef WIN32
	if (fLastHandledSocketNum == -1) usleep(1);
#endif
}


HandlerDescriptor::HandlerDescriptor(HandlerDescriptor* nextHandler)
: handlerProc(NULL) {
	// Link this descriptor into a doubly-linked list:
	if (nextHandler == this) { // initialization
		fNextHandler = fPrevHandler = this;
	} else {
		fNextHandler = nextHandler;
		fPrevHandler = nextHandler->fPrevHandler;
		nextHandler->fPrevHandler = this;
		fPrevHandler->fNextHandler = this;
	}
}

HandlerDescriptor::~HandlerDescriptor() {
	// Unlink this descriptor from a doubly-linked list:
	fNextHandler->fPrevHandler = fPrevHandler;
	fPrevHandler->fNextHandler = fNextHandler;
}

HandlerSet::HandlerSet()
: fHandlers(&fHandlers) {
	fHandlers.socketNum = -1; // shouldn't ever get looked at, but in case...
}

HandlerSet::~HandlerSet() {
	// Delete each handler descriptor:
	while (fHandlers.fNextHandler != &fHandlers) {
		delete fHandlers.fNextHandler; // changes fHandlers->fNextHandler
	}
}

void HandlerSet
::assignHandler(int socketNum, TaskScheduler::BackgroundHandlerProc* handlerProc, void* clientData) {
	// First, see if there's already a handler for this socket:
	HandlerDescriptor* handler = lookupHandler(socketNum);
	if (handler == NULL) { // No existing handler, so create a new descr:
		handler = new HandlerDescriptor(fHandlers.fNextHandler);
		handler->socketNum = socketNum;
	}

	handler->handlerProc = handlerProc;
	handler->clientData = clientData;
}

void HandlerSet::removeHandler(int socketNum) {
	HandlerDescriptor* handler = lookupHandler(socketNum);
	delete handler;
}

void HandlerSet::moveHandler(int oldSocketNum, int newSocketNum) {
	HandlerDescriptor* handler = lookupHandler(oldSocketNum);
	if (handler != NULL) {
		handler->socketNum = newSocketNum;
	}
}

HandlerDescriptor* HandlerSet::lookupHandler(int socketNum) {
	HandlerDescriptor* handler;
	HandlerIterator iter(*this);
	while ((handler = iter.next()) != NULL) {
		if (handler->socketNum == socketNum) break;
	}
	return handler;
}

HandlerIterator::HandlerIterator(HandlerSet& handlerSet)
: fOurSet(handlerSet) {
	reset();
}

HandlerIterator::~HandlerIterator() {
}

void HandlerIterator::reset() {
	fNextPtr = fOurSet.fHandlers.fNextHandler;
}

HandlerDescriptor* HandlerIterator::next() {
	HandlerDescriptor* result = fNextPtr;
	if (result == &fOurSet.fHandlers) { // no more
		result = NULL;
	} else {
		fNextPtr = fNextPtr->fNextHandler;
	}

	return result;
}
