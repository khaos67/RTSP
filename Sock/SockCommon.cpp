#include "SockCommon.h"
#include "RTSPCommonEnv.h"
#include <stdio.h>

#ifdef WIN32
#pragma comment(lib, "ws2_32.lib")
#elif defined(LINUX)
#include <string.h>
#endif

#define MAKE_SOCKADDR_IN(var,adr,prt) /*adr,prt must be in network order*/\
    struct sockaddr_in var;\
    var.sin_family = AF_INET;\
    var.sin_addr.s_addr = (adr);\
    var.sin_port = htons(prt);\

#ifdef WIN32
#define WS_VERSION_CHOICE1 0x202/*MAKEWORD(2,2)*/
#define WS_VERSION_CHOICE2 0x101/*MAKEWORD(1,1)*/
int initializeWinsockIfNecessary(void) {
	/* We need to call an initialization routine before
	* we can do anything with winsock.  (How fucking lame!):
	*/
	static int _haveInitializedWinsock = 0;
	WSADATA	wsadata;

	if (!_haveInitializedWinsock) {
		if ((WSAStartup(WS_VERSION_CHOICE1, &wsadata) != 0)
			&& ((WSAStartup(WS_VERSION_CHOICE2, &wsadata)) != 0)) {
				return 0; /* error in initialization */
		}
		if ((wsadata.wVersion != WS_VERSION_CHOICE1)
			&& (wsadata.wVersion != WS_VERSION_CHOICE2)) {
				WSACleanup();
				return 0; /* desired Winsock version was not available */
		}
		_haveInitializedWinsock = 1;
	}

	return 1;
}
#else
#define initializeWinsockIfNecessary()	1
#endif

void socketErr(char *lpszFormat,...)
{
	va_list args;
	int len;
	char *buffer;

	va_start(args, lpszFormat);

	len = _vscprintf(lpszFormat, args) + 32;
	buffer = (char *)malloc(len * sizeof(char));

	vsprintf(buffer, lpszFormat, args);

#ifdef WIN32
	if (RTSPCommonEnv::nDebugPrint == 0) {
		fprintf(stdout, buffer);
		fprintf(stdout, "%d\n", WSAGetLastError());
	} else if (RTSPCommonEnv::nDebugPrint == 1) {
		OutputDebugString(buffer);
		char tmp[16] = {0};
		sprintf(tmp, "%d\n", WSAGetLastError());
		OutputDebugString(tmp);
	}
#elif defined(ANDROID)
	DPRINTF0(buffer);
	char tmp[16] = {0};
	sprintf(tmp, "%d\n", WSAGetLastError());
	DPRINTF0(tmp);
#else
	fprintf(stdout, buffer);
	fprintf(stdout, "%d\n", WSAGetLastError());
#endif

	free(buffer);
}

static int reuseFlag = 1;

int setupStreamSock(short port, int makeNonBlocking)
{
	if (!initializeWinsockIfNecessary()) {
		socketErr("[%s] Failed to initialize 'winsock': ", __FUNCTION__);
		return -1;
	}

	int newSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (newSocket < 0) {
		DPRINTF("%s:%d\n",__FUNCTION__,__LINE__);
		socketErr("[%s] unable to create stream socket: ", __FUNCTION__);
		return newSocket;
	}
#if 0
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&reuseFlag, sizeof reuseFlag) != 0) {
			socketErr("[%s] setsockopt(SO_REUSEADDR) error: ", __FUNCTION__);
			closeSocket(newSocket);
			return -1;
	}
#endif
	struct sockaddr_in c_addr;
	memset(&c_addr, 0, sizeof(c_addr));
	c_addr.sin_addr.s_addr = INADDR_ANY;
	c_addr.sin_family = AF_INET;
	c_addr.sin_port = htons(port);

	if (bind(newSocket, (struct sockaddr*)&c_addr, sizeof c_addr) != 0) {
		socketErr("[%s] bind() error (port number: %d): ", __FUNCTION__, port);
		closeSocket(newSocket);
		return -1;
	}

	if (makeNonBlocking) {
		if (!makeSocketNonBlocking(newSocket)) {
			socketErr("[%s] failed to make non-blocking: ", __FUNCTION__);
			closeSocket(newSocket);
			return -1;
		}
	}

	return newSocket;
}

int setupDatagramSock(short port, int makeNonBlocking)
{
	if (!initializeWinsockIfNecessary()) {
		socketErr("[%s] Failed to initialize 'winsock': ", __FUNCTION__);
		return -1;
	}

	int newSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (newSocket < 0) {
		socketErr("[%s] unable to create datagram socket: ", __FUNCTION__);
		return newSocket;
	}

	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&reuseFlag, sizeof reuseFlag) < 0) {
			socketErr("setsockopt(SO_REUSEADDR) error: ", __FUNCTION__);
			closeSocket(newSocket);
			return -1;
	}

	struct sockaddr_in c_addr;
	memset(&c_addr, 0, sizeof(c_addr));
	c_addr.sin_addr.s_addr = INADDR_ANY;
	c_addr.sin_family = AF_INET;
	c_addr.sin_port = htons(port);

	if (bind(newSocket, (struct sockaddr*)&c_addr, sizeof c_addr) != 0) {
		socketErr("[%s] bind() error (port number: %d): ", __FUNCTION__, port);
		closeSocket(newSocket);
		return -1;
	}

	if (makeNonBlocking) {
		if (!makeSocketNonBlocking(newSocket)) {
			socketErr("[%s] failed to make non-blocking: ", __FUNCTION__);
			closeSocket(newSocket);
			return -1;
		}
	}

	return newSocket;
}

#define LISTEN_BACKLOG_SIZE 20

int setupServerSock(short port, int makeNonBlocking)
{
	int sock = setupStreamSock(port, makeNonBlocking);
	if (sock < 0) return sock;

	if (listen(sock, LISTEN_BACKLOG_SIZE) != 0) {
		socketErr("[%s] failed to listen sock: ", __FUNCTION__);
		closeSocket(sock);
		return -1;
	}

	return sock;
}

int setupClientSock(int serverSock, int makeNonBlocking, struct sockaddr_in& clientAddr)
{
	socklen_t clientAddrLen = sizeof clientAddr;
	int clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
	if (clientSock < 0) {
		int err = WSAGetLastError();
		if (err != EWOULDBLOCK) {
			socketErr("[%s] accept() failed: ", __FUNCTION__);
			closeSocket(clientSock);
			return -1;
		}
		return 0;
	}
	makeSocketNonBlocking(clientSock);

	return clientSock;
}

int makeSocketNonBlocking(int sock)
{
#ifdef WIN32
	unsigned long arg = 1;
	return ioctlsocket(sock, FIONBIO, &arg) == 0;
#else
	int curFlags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, curFlags|O_NONBLOCK) >= 0;
#endif
}

int makeTCP_NoDelay(int sock)
{
	int flag = 1;
	int err = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

	if(err != 0)
		socketErr("[%s] setsocket TCPNODELAY error: ", __FUNCTION__);

	return 0;
}

unsigned setSendBufferTo(int sock, unsigned requestedSize)
{
	return setBufferSizeTo(SO_SNDBUF, sock, requestedSize);
}

unsigned setReceiveBufferTo(int sock, unsigned requestedSize)
{
	return setBufferSizeTo(SO_RCVBUF, sock, requestedSize);
}

unsigned getSendBufferSize(int sock)
{
	return getBufferSize(SO_SNDBUF, sock);
}

unsigned getReceiveBufferSize(int sock)
{
	return getBufferSize(SO_RCVBUF, sock);
}

unsigned getBufferSize(int bufOptName, int sock)
{
	unsigned curSize;
	socklen_t sizeSize = sizeof curSize;
	if (getsockopt(sock, SOL_SOCKET, bufOptName,
		(char*)&curSize, &sizeSize) < 0) {
			socketErr("getBufferSize() error: ", __FUNCTION__);
			return 0;
	}

	return curSize;
}

unsigned setBufferSizeTo(int bufOptName, int sock, int requestedSize)
{
	socklen_t sizeSize = sizeof requestedSize;
	if (setsockopt(sock, SOL_SOCKET, bufOptName, (char*)&requestedSize, sizeSize) != 0)
		socketErr("setBufferSizeTo() error: ", __FUNCTION__);
	return getBufferSize(bufOptName, sock);
}

int blockUntilReadable(int sock, timeval *timeout)
{
	int result = -1;

	do {
		fd_set rd_set;
		FD_ZERO(&rd_set);
		if (sock < 0) break;

		FD_SET((unsigned) sock, &rd_set);
		const unsigned numFds = sock+1;

		result = select(numFds, &rd_set, NULL, NULL, timeout);
		if (timeout != NULL && result == 0) {
			break; // this is OK - timeout occurred
		} else if (result <= 0) {
			int err = WSAGetLastError();
			if (err == EINTR || err == EWOULDBLOCK) continue;
			socketErr("[%s] select() error: ", __FUNCTION__);
			break;
		}

		if (!FD_ISSET(sock, &rd_set)) {
			socketErr("[%s] select() error - !FD_ISSET", __FUNCTION__);
			break;
		}
	} while (0);

	return result;
}

int readSocket1(int sock, char *buffer, unsigned bufferSize, struct sockaddr_in &fromAddress)
{
	int bytesRead;
	socklen_t addressSize = sizeof fromAddress;

	bytesRead = recvfrom(sock, buffer, bufferSize, 0, (struct sockaddr*)&fromAddress, &addressSize);

	return bytesRead;
}

int readSocket(int sock, char *buffer, unsigned int bufferSize, sockaddr_in &fromAddress, timeval *timeout)
{
	int bytesRead = -1;

	do {
		int result = blockUntilReadable(sock, timeout);
		if (timeout != NULL && result == 0) {
			bytesRead = 0;
			break;
		} else if (result <= 0) {
			break;
		}

		socklen_t addressSize = sizeof fromAddress;
		bytesRead = recvfrom(sock, buffer, bufferSize, 0,
			(struct sockaddr*)&fromAddress,
			&addressSize);
		if (bytesRead < 0) {
			int err = WSAGetLastError();
			if (err == 111 /*ECONNREFUSED (Linux)*/
				// What a piece of crap Windows is.  Sometimes
				// recvfrom() returns -1, but with an 'errno' of 0.
				// This appears not to be a real error; just treat
				// it as if it were a read of zero bytes, and hope
				// we don't have to do anything else to 'reset'
				// this alleged error:
				|| err == 0 || err == EWOULDBLOCK
				|| err == 113 /*EHOSTUNREACH (Linux)*/) {
				//Why does Linux return this for datagram sock?
				fromAddress.sin_addr.s_addr = 0;
				return 0;
			}
			socketErr("[%s] recvfrom() error: ", __FUNCTION__);
			break;
		}
	} while (0);

	return bytesRead;
}

int readSocketExact(int sock, char *buffer, unsigned bufferSize, struct sockaddr_in& fromAddress, struct timeval* timeout)
{
	int bsize = bufferSize;
	int bytesRead = 0;
	int totBytesRead = 0;
	do 
	{
		bytesRead = readSocket (sock, buffer + totBytesRead, bsize, fromAddress, timeout);
		if (bytesRead <= 0) break;
		totBytesRead += bytesRead;
		bsize -= bytesRead;
	} while (bsize != 0);

	return totBytesRead;
}

int writeSocket(int sock, char *buffer, unsigned bufferSize)
{
	return send(sock, buffer, bufferSize, 0);
}

int writeSocket(int sock, char *buffer, unsigned int bufferSize, sockaddr_in &toAddress)
{
	return sendto(sock, buffer, bufferSize, 0, (struct sockaddr *)&toAddress, sizeof(struct sockaddr_in));
}

bool writeSocket(int socket, struct in_addr address, unsigned short port,
				 unsigned char* buffer, unsigned bufferSize) 
{
  do {
	  MAKE_SOCKADDR_IN(dest, address.s_addr, port);
	  int bytesSent = sendto(socket, (char*)buffer, bufferSize, 0, (struct sockaddr*)&dest, sizeof dest);
	  if (bytesSent != (int)bufferSize) {
		  char tmpBuf[100];
		  sprintf(tmpBuf, "writeSocket(%d), sendTo() error: wrote %d bytes instead of %u: ", socket, bytesSent, bufferSize);
		  socketErr(tmpBuf);
		  break;
	  }

	  return true;
  } while (0);

  return false;
}

bool writeSocket(int socket, struct in_addr address, unsigned short port,
				 unsigned char ttlArg,
				 unsigned char* buffer, unsigned bufferSize) 
{
	// Before sending, set the socket's TTL:
#if defined(__WIN32__) || defined(_WIN32)
#define TTL_TYPE int
#else
#define TTL_TYPE u_int8_t
#endif
	TTL_TYPE ttl = (TTL_TYPE)ttlArg;
	if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof ttl) < 0) {
		socketErr("setsockopt(IP_MULTICAST_TTL) error: ");
		return false;
	}

	return writeSocket(socket, address, port, buffer, bufferSize);
}

int sendRTPOverTCP(int sock, char *buffer, int len, unsigned char streamChannelId)
{
	char const dollar = '$';

	if (send(sock, &dollar, 1, 0) != 1) return -1;
	if (send(sock, (char*)&streamChannelId, 1, 0) != 1) return -1;

	char sz[2];
	sz[0] = (char)((len&0xFF00)>>8);
	sz[1] = (char)(len&0x00FF);
	if (send(sock, sz, 2, 0) != 2) return -1;

	if (send(sock, buffer, len, 0) != len) return -1;

	return 0;
}

void shutdown(int sock)
{
#ifdef WIN32
	if (shutdown(sock, SD_SEND) != 0)
		socketErr("shutdown error: ");
#else
	if (shutdown(sock, SHUT_RD) != 0)
		socketErr("shutdown error: ");
#endif
}

bool isMulticastAddress(unsigned int address)
{
	// Note: We return False for addresses in the range 224.0.0.0
	// through 224.0.0.255, because these are non-routable
	// Note: IPv4-specific #####
	unsigned int addressInHostOrder = ntohl(address);
	return addressInHostOrder >  0xE00000FF &&
		addressInHostOrder <= 0xEFFFFFFF;
}

bool socketJoinGroupSSM(int sock, unsigned int groupAddress, unsigned int sourceFilterAddr)
{
	if (!isMulticastAddress(groupAddress)) return true; // ignore this case

	struct ip_mreq_source imr;
#ifdef ANDROID
    imr.imr_multiaddr = groupAddress;
    imr.imr_sourceaddr = sourceFilterAddr;
    imr.imr_interface = INADDR_ANY;
#else
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_sourceaddr.s_addr = sourceFilterAddr;
	imr.imr_interface.s_addr = INADDR_ANY;
#endif
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (const char*)&imr, sizeof (struct ip_mreq_source)) < 0) {
		socketErr("setsockopt(IP_ADD_SOURCE_MEMBERSHIP) error: ", __FUNCTION__);
		return false;
	}

	return true;
}

bool socketLeaveGroupSSM(int sock, unsigned int groupAddress, unsigned int sourceFilterAddr)
{
	if (!isMulticastAddress(groupAddress)) return true; // ignore this case

	struct ip_mreq_source imr;
#ifdef ANDROID
    imr.imr_multiaddr = groupAddress;
    imr.imr_sourceaddr = sourceFilterAddr;
    imr.imr_interface = INADDR_ANY;
#else
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_sourceaddr.s_addr = sourceFilterAddr;
	imr.imr_interface.s_addr = INADDR_ANY;
#endif
	if (setsockopt(sock, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (const char*)&imr, sizeof (struct ip_mreq_source)) < 0) {
		return false;
	}

	return true;
}

bool socketJoinGroup(int sock, unsigned int groupAddress)
{
	if (!isMulticastAddress(groupAddress)) return true; // ignore this case

	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&imr, sizeof (struct ip_mreq)) < 0) {
#if defined(__WIN32__) || defined(_WIN32)
		if (WSAGetLastError() != 0) {
			// That piece-of-shit toy operating system (Windows) sometimes lies
			// about setsockopt() failing!
#endif
			socketErr("setsockopt(IP_ADD_MEMBERSHIP) error: ", __FUNCTION__);
			return false;
#if defined(__WIN32__) || defined(_WIN32)
		}
#endif
	}

	return true;
}

bool socketLeaveGroup(int sock, unsigned int groupAddress)
{
	if (!isMulticastAddress(groupAddress)) return true; // ignore this case

	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char*)&imr, sizeof (struct ip_mreq)) < 0) {
		return false;
	}

	return true;
}

typedef unsigned int	u_int32_t;
u_int32_t ReceivingInterfaceAddr = INADDR_ANY;
static bool loopbackWorks = 1;

static bool badAddressForUs(u_int32_t addr) 
{
	// Check for some possible erroneous addresses:
	u_int32_t nAddr = htonl(addr);
	return (nAddr == 0x7F000001 /* 127.0.0.1 */
		|| nAddr == 0
		|| nAddr == (u_int32_t)(~0));
}

u_int32_t ourIPAddress() 
{
	static u_int32_t ourAddress = 0;
	int sock = -1;
	struct in_addr testAddr;

	if (ReceivingInterfaceAddr != INADDR_ANY) {
		// Hack: If we were told to receive on a specific interface address, then 
		// define this to be our ip address:
		ourAddress = ReceivingInterfaceAddr;
	}

	if (ourAddress == 0) {
		// We need to find our source address
		struct sockaddr_in fromAddr;
		fromAddr.sin_addr.s_addr = 0;

		// Get our address by sending a (0-TTL) multicast packet,
		// receiving it, and looking at the source address used.
		// (This is kinda bogus, but it provides the best guarantee
		// that other nodes will think our address is the same as we do.)
		do {
			loopbackWorks = 0; // until we learn otherwise

			testAddr.s_addr = inet_addr("228.67.43.91"); // arbitrary
			unsigned short testPort = 15947; // ditto

			sock = setupDatagramSock(testPort, false);
			if (sock < 0) break;

			if (!socketJoinGroup(sock, testAddr.s_addr)) break;

			unsigned char testString[] = "hostIdTest";
			unsigned testStringLength = sizeof testString;

			if (!writeSocket(sock, testAddr, testPort, 0, testString, testStringLength)) 
				break;

			// Block until the socket is readable (with a 5-second timeout):
			fd_set rd_set;
			FD_ZERO(&rd_set);
			FD_SET((unsigned)sock, &rd_set);
			const unsigned numFds = sock+1;
			struct timeval timeout;
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			int result = select(numFds, &rd_set, NULL, NULL, &timeout);
			if (result <= 0) break;

			unsigned char readBuffer[20];
			int bytesRead = readSocket1(sock, (char *)readBuffer, sizeof readBuffer, fromAddr);
			if (bytesRead != (int)testStringLength
				|| strncmp((char*)readBuffer, (char*)testString, testStringLength) != 0) {
					break;
			}

			// We use this packet's source address, if it's good:
			loopbackWorks = !badAddressForUs(fromAddr.sin_addr.s_addr);
		} while (0);

		if (sock >= 0) {
			socketLeaveGroup(sock, testAddr.s_addr);
			closeSocket(sock);
		}

		// Make sure we have a good address:
		u_int32_t from = fromAddr.sin_addr.s_addr;
		if (badAddressForUs(from)) {
			DPRINTF("This computer has an invalid IP address\n");
			from = 0;
		}

		ourAddress = from;
	}

	return ourAddress;
}
