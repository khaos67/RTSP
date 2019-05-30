#ifndef __CLIENT_SOCKET_H__
#define __CLIENT_SOCKET_H__

#include "MySock.h"

class ClientSocket
{
public:
	ClientSocket(MySock& rtspSock, unsigned char rtpChannelId, unsigned char rtcpChannelId);
	ClientSocket(MySock& rtpSock, struct sockaddr_in& rtpDestAddr, MySock& rtcpSock, struct sockaddr_in& rtcpDestAddr);
	virtual ~ClientSocket();

	int sendRTP(char *buf, int len);
	int sendRTCP(char *buf, int len);
	void activate();

	bool isActivated() { return fActive; }

protected:
	MySock*				fRtpSock;
	struct sockaddr_in	fRtpDestAddr;
	MySock*				fRtcpSock;
	struct sockaddr_in	fRtcpDestAddr;

	unsigned char		fRtpChannelId;
	unsigned char		fRtcpChannelId;
	bool				fIsTCP;
	bool				fActive;
};

#endif