#include "ClientSocket.h"

ClientSocket::ClientSocket(MySock& rtspSock, unsigned char rtpChannelId, unsigned char rtcpChannelId) 
: fRtpSock(&rtspSock), fRtcpSock(&rtspSock), fRtpChannelId(rtpChannelId), fRtcpChannelId(rtcpChannelId), fIsTCP(true), fActive(false)
{
}

ClientSocket::ClientSocket(MySock& rtpSock, sockaddr_in& rtpDestAddr, MySock& rtcpSock, sockaddr_in& rtcpDestAddr) 
: fRtpSock(&rtpSock), fRtpDestAddr(rtpDestAddr), fRtcpSock(&rtcpSock), fRtcpDestAddr(rtcpDestAddr), fIsTCP(false), fActive(false)
{
}

ClientSocket::~ClientSocket()
{
	if (!fIsTCP) {
		if (fRtpSock) {
			fRtpSock->closeSock();
			delete fRtpSock;
		}
		if (fRtcpSock) {
			fRtcpSock->closeSock();
			delete fRtcpSock;
		}
	}
}

int ClientSocket::sendRTP(char *buf, int len)
{
	if (fIsTCP) {
		return fRtpSock->sendRTPOverTCP(buf, len, fRtpChannelId);
	} else {
		return fRtpSock->writeSocket(buf, len, fRtpDestAddr);
	}
}

int ClientSocket::sendRTCP(char *buf, int len)
{
	if (fIsTCP) {
		return fRtcpSock->sendRTPOverTCP(buf, len, fRtcpChannelId);
	} else {
		return fRtcpSock->writeSocket(buf, len, fRtcpDestAddr);
	}
}

void ClientSocket::activate()
{
	fActive = true;
}
