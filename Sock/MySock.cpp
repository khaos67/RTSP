#include "MySock.h"
#ifdef LINUX
#include <string.h>
#endif

MySock::MySock()
{
	fSock = -1;
	fPort = 0;
	memset(&fClientAddr, 0, sizeof(struct sockaddr_in));
	fIsSSM = false;
	fGroupAddress = fSourceFilterAddr = 0;
	MUTEX_INIT(&fMutex);
}

MySock::~MySock()
{
	closeSock();
	MUTEX_DESTROY(&fMutex);
}

void MySock::closeSock()
{
	if (fIsSSM) {
		if (!leaveGroupSSM(fGroupAddress, fSourceFilterAddr)) {
			leaveGroup(fGroupAddress);
		}
	}

	if (fSock >= 0) {
		closeSocket(fSock);
		fSock = -1;
		memset(&fClientAddr, 0, sizeof(struct sockaddr_in));
		fGroupAddress = fSourceFilterAddr = 0;
	}
}

void MySock::shutdown()
{
	::shutdown(fSock);
}

int MySock::setupStreamSock(short port, int makeNonBlocking)
{
	int sock = ::setupStreamSock(port, makeNonBlocking);
	if (sock > 0) {
		fSock = sock;
		fPort = port;
	}

	return sock;
}

int MySock::setupDatagramSock(short port, int makeNonBlocking)
{
	int sock = ::setupDatagramSock(port, makeNonBlocking);
	if (sock > 0) {
		fSock = sock;
		fPort = port;
	}

	return sock;
}

int MySock::setupServerSock(short port, int makeNonBlocking)
{
	int sock = ::setupServerSock(port, makeNonBlocking);
	if (sock > 0) {
		fSock = sock;
		fPort = port;
	}

	return sock;
}

int MySock::setupClientSock(int serverSock, int makeNonBlocking)
{
	int sock = ::setupClientSock(serverSock, makeNonBlocking, fClientAddr);
	if (sock > 0) {
		fSock = sock;
		fPort = ntohs(fClientAddr.sin_port);
	}

	return sock;
}

int MySock::writeSocket(char *buffer, unsigned bufferSize) 
{
	MUTEX_LOCK(&fMutex);
	int err = ::writeSocket(fSock, buffer, bufferSize); 
	MUTEX_UNLOCK(&fMutex);
	return err;
}

int MySock::writeSocket(char *buffer, unsigned bufferSize, struct sockaddr_in &toAddress) 
{
	MUTEX_LOCK(&fMutex);
	int err = ::writeSocket(fSock, buffer, bufferSize, toAddress);
	MUTEX_UNLOCK(&fMutex);
	return err;
}

int MySock::sendRTPOverTCP(char *buffer, int len, unsigned char streamChannelId)
{
	MUTEX_LOCK(&fMutex);
	int err = ::sendRTPOverTCP(fSock, buffer, len, streamChannelId);
	MUTEX_UNLOCK(&fMutex);
	return err;
}

bool MySock::changePort(short port)
{
	closeSocket(fSock);
	fSock = setupDatagramSock(port, true);
	return fSock >= 0;
}

void MySock::changeDestination(struct in_addr const& newDestAddr, short newDestPort)
{
	if (newDestAddr.s_addr != 0) {
		if (newDestAddr.s_addr != fGroupAddress && isMulticastAddress(newDestAddr.s_addr)) {
			socketLeaveGroup(fSock, fGroupAddress);
			socketJoinGroup(fSock, newDestAddr.s_addr);
		}
		fGroupAddress = newDestAddr.s_addr;
	}

	if (newDestPort != 0) {
		if (newDestPort != fPort && isMulticastAddress(fGroupAddress)) {
			changePort(newDestPort);
			socketJoinGroup(fSock, fGroupAddress);
		}
	}
}

bool MySock::joinGroupSSM(unsigned int groupAddress, unsigned int sourceFilterAddr) 
{
	fGroupAddress = groupAddress;
	fSourceFilterAddr = sourceFilterAddr;
	fIsSSM = socketJoinGroupSSM(fSock, groupAddress, sourceFilterAddr);
	return fIsSSM;
}

bool MySock::leaveGroupSSM(unsigned int groupAddress, unsigned int sourceFilterAddr) 
{
	fIsSSM = !socketLeaveGroupSSM(fSock, groupAddress, sourceFilterAddr);
	return !fIsSSM;
}

bool MySock::joinGroup(unsigned int groupAddress) 
{
	fGroupAddress = groupAddress;
	fIsSSM = socketJoinGroup(fSock, groupAddress);
	return fIsSSM;
}
	
bool MySock::leaveGroup(unsigned int groupAddress) 
{
	fIsSSM = !socketLeaveGroup(fSock, groupAddress);
	return !fIsSSM;
}
