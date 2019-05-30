#include "RTPSource.h"
#include "RTSPCommon.h"
#include "MediaSession.h"
#include "util.h"
#include "rtcp_from_spec.h"
#include "RTSPCommonEnv.h"

#include <stdio.h>
#include <time.h>

RTPSource::RTPSource(int streamType, MediaSubsession &subsession, TaskScheduler &task)
: fStreamType(streamType), fRecvBuf(NULL), fRTPPayloadFormat(subsession.rtpPayloadFormat()), fTimestampFrequency(subsession.rtpTimestampFrequency()),
fSSRC(rand()), fTask(&task), fSvrAddr(0), fRtspSock(NULL), fRtcpChannelId(subsession.rtcpChannelId), fCodecName(NULL),
fReceptionStatsDB(NULL), fRtcpInstance(NULL),
fFrameHandlerFunc(NULL), fFrameHandlerFuncData(NULL), fIsStartFrame(false), fBeginFrame(false), fExtraData(NULL), fExtraDataSize(0),
fRtpHandlerFunc(NULL), fRtpHandlerFuncData(NULL), fRtcpHandlerFunc(NULL), fRtcpHandlerFuncData(NULL), fFrameType(FRAME_TYPE_ETC)
{
	fReorderingBuffer = new ReorderingPacketBuffer();

	const int cnamelen = 100;
	char cname[cnamelen+1] = {0};
	gethostname(cname, cnamelen);

	fReceptionStatsDB = new RTPReceptionStatsDB();
	fRtcpInstance = new RTCPInstance(25, (unsigned char const*)cname, this);
	fLastRtcpSendTime = time(NULL);

	fCodecName = strDup(subsession.codecName());

	fTrackId = strDup(subsession.controlPath());

	if (!strcmp(subsession.mediumName(), "video"))
		fFrameType = FRAME_TYPE_VIDEO;
	else if (!strcmp(subsession.mediumName(), "audio"))
		fFrameType = FRAME_TYPE_AUDIO;

	fFrameBuf = new uint8_t[FRAME_BUFFER_SIZE];
	fFrameBufPos = 0;

	fLastSeqNum = fLastSeqNum2 = 0;
	fLastTimestamp = 0;

	if (streamType == STREAM_TYPE_UDP || streamType == STREAM_TYPE_MULTICAST)
	{
		fRtpSock.setupDatagramSock(subsession.clientPortNum(), true);
		unsigned requestSize = 1024*1024;
		if (fRtpSock.setReceiveBufferTo(requestSize) != requestSize)
			DPRINTF("RTPSource failed to setReceiveBufferTo(%d)\n", requestSize);
		
		fRtcpSock.setupDatagramSock(subsession.clientPortNum()+1, true);
		fRtcpHisPort = subsession.serverPortNum+1;
		
		fRecvBuf = new char[MAX_RTP_SIZE];	

		struct in_addr tempAddr;
		tempAddr.s_addr = subsession.connectionEndpointAddress();
		if (subsession.isSSM()) {
			if (!fRtpSock.joinGroupSSM(tempAddr.s_addr, subsession.parentSession().sourceFilterAddr().s_addr)) {
				DPRINTF("SSM join failed\n");
				if (!fRtpSock.joinGroup(tempAddr.s_addr)) {
					DPRINTF("failed to join group\n");
				}
			}

			if (!fRtcpSock.joinGroupSSM(tempAddr.s_addr, subsession.parentSession().sourceFilterAddr().s_addr)) {
				DPRINTF("RTCP SSM join failed\n");
				if (!fRtcpSock.joinGroup(tempAddr.s_addr)) {
					DPRINTF("RTCP failed to join group\n");
				}
			}
		} else {
			if (!fRtpSock.joinGroup(tempAddr.s_addr)) {
				DPRINTF("failed to join group\n");
			}
			if (!fRtcpSock.joinGroup(tempAddr.s_addr)) {
				DPRINTF("RTCP failed to join group\n");
			}
		}
	}
}

RTPSource::~RTPSource()
{
	stopNetworkReading();
	fRtpSock.closeSock();
	fRtcpSock.closeSock();

	DELETE_OBJECT(fReceptionStatsDB);
	DELETE_OBJECT(fRtcpInstance);

	DELETE_ARRAY(fRecvBuf);
	DELETE_ARRAY(fFrameBuf);
	DELETE_ARRAY(fCodecName);
	DELETE_ARRAY(fExtraData);
	DELETE_ARRAY(fTrackId);

	DELETE_OBJECT(fReorderingBuffer);
}

void RTPSource::startNetworkReading(FrameHandlerFunc frameHandler, void *frameHandlerData, 
									RTPHandlerFunc rtpHandler, void *rtpHandlerData,
									RTPHandlerFunc rtcpHandler, void *rtcpHandlerData)
{
	fFrameHandlerFunc = frameHandler;
	fFrameHandlerFuncData = frameHandlerData;

	fRtpHandlerFunc = rtpHandler;
	fRtpHandlerFuncData = rtpHandlerData;

	fRtcpHandlerFunc = rtcpHandler;
	fRtcpHandlerFuncData = rtcpHandlerData;

	if (fRtpSock.isOpened())
		fTask->turnOnBackgroundReadHandling(fRtpSock.sock(), &incomingRtpPacketHandler, this);

	if (fRtcpSock.isOpened())
		fTask->turnOnBackgroundReadHandling(fRtcpSock.sock(), &incomingRtcpPacketHandler, this);
}

void RTPSource::stopNetworkReading()
{
	if (fRtpSock.isOpened())
		fTask->turnOffBackgroundReadHandling(fRtpSock.sock());

	if (fRtcpSock.isOpened())
		fTask->turnOffBackgroundReadHandling(fRtcpSock.sock());

	fFrameHandlerFunc = NULL;
	fFrameHandlerFuncData = NULL;

	fRtpHandlerFunc = NULL;
	fRtpHandlerFuncData = NULL;

	fRtcpHandlerFunc = NULL;
	fRtcpHandlerFuncData = NULL;

	fReorderingBuffer->reset();
}

void RTPSource::rtpReadHandler(char *buf, int len, struct sockaddr_in &fromAddress)
{
	bool readSuccess = false;

	if (len < sizeof(RTP_HEADER))
		return;

	if (fSvrAddr == 0)
		fSvrAddr = fromAddress.sin_addr.s_addr;

	RTPPacketBuffer *packet = fReorderingBuffer->getFreePacket();
	packet->reset();

	if (!packet->packetHandler((uint8_t *)buf, len)) {
		DPRINTF("invalid rtp packet, discard this packet\n");
		delete packet;
		return;
	}

	unsigned short pt = packet->payloadType();
	unsigned short mk = packet->markerBit();
	unsigned short seqnum = packet->sequenceNum();
	unsigned int ts = packet->timestamp();
	unsigned int rtpSSRC = packet->ssrc();

	if (fRTPPayloadFormat != pt) {
		DPRINTF("rtp payload type error, pt: %d, expected pt: %d\n", pt, fRTPPayloadFormat);
		goto skip;
	}

	if (RTSPCommonEnv::nDebugFlag&DEBUG_FLAG_RTP) {
		if (ts == fLastTimestamp)
			DPRINTF("pt: %d, seqnum: %u, ts: %u, mk: %u, len: %d\n", pt, seqnum, ts, mk, len);
		else
			DPRINTF("pt: %d, seqnum: %u, ts: %u, mk: %u, ts diff: %u, len: %d\n", pt, seqnum, ts, mk, ts-fLastTimestamp, len);
	}	

	struct timeval presentationTime;
	bool hasBeenSyncedUsingRTCP;

	if (fReceptionStatsDB)
		fReceptionStatsDB->noteIncomingPacket(rtpSSRC, seqnum, ts, fTimestampFrequency, true, presentationTime, hasBeenSyncedUsingRTCP, len);

	readSuccess = fReorderingBuffer->storePacket(packet);

skip:
	if (!readSuccess)
		fReorderingBuffer->freePacket(packet);

	processNextPacket();

	fLastTimestamp = ts;
}

void RTPSource::processNextPacket()
{
	while (1)
	{
		bool packetLossPrecededThis;
		RTPPacketBuffer *nextPacket = fReorderingBuffer->getNextCompletedPacket(packetLossPrecededThis);
		if (nextPacket == NULL) break;

		// check sequence number
		unsigned short seqnum = nextPacket->sequenceNum();
		if (!nextPacket->isFirstPacket()) {
			if (fLastSeqNum == 0xFFFF && seqnum == 0) ;
			else if (fLastSeqNum+1 == seqnum) ;
			else {
				DPRINTF("pt: %d, rtp sequence error: %u, prev: %u\n", nextPacket->payloadType(), seqnum, fLastSeqNum);
			}
		}

		fLastSeqNum = seqnum;

		if (fRtpHandlerFunc)
			fRtpHandlerFunc(fRtpHandlerFuncData, fTrackId, (char *)nextPacket->buf(), nextPacket->length());

		if (fFrameHandlerFunc)
			processFrame(nextPacket);

		fReorderingBuffer->releaseUsedPacket(nextPacket);
	}
}

void RTPSource::setRtspSock(MySock *rtspSock)
{
	fRtspSock = rtspSock;
}

void RTPSource::setServerPort(uint16_t serverPort)
{
	fRtcpHisPort = serverPort+1;
}

void RTPSource::setRtcpChannelId(unsigned char rtcpChannelId)
{
	fRtcpChannelId = rtcpChannelId;
}

void RTPSource::processFrame(RTPPacketBuffer *packet)
{
	uint8_t *buf = (uint8_t *)packet->payload();
	int len = packet->payloadLen();
	int64_t media_timestamp = packet->extTimestamp() == 0 ? getMediaTimestamp(packet->timestamp()) : packet->extTimestamp();

	copyToFrameBuffer(buf, len);

	if (packet->markerBit() == 1 || fLastTimestamp != packet->timestamp()) {
		if (fFrameHandlerFunc) {
			fFrameHandlerFunc(fFrameHandlerFuncData, fFrameType, media_timestamp, fFrameBuf, fFrameBufPos);
		}
		resetFrameBuf();
	}
}

void RTPSource::incomingRtpPacketHandler(void *instance, int)
{
	RTPSource *client = (RTPSource*)instance;
	client->incomingRtpPacketHandler1();
}

void RTPSource::incomingRtpPacketHandler1()
{
	struct sockaddr_in fromAddress;
	int len = MAX_RTP_SIZE;
	int addressSize = sizeof(fromAddress);
	int bytesRead;

	bytesRead = fRtpSock.readSocket1(fRecvBuf, len, fromAddress);

	if(bytesRead <= 0)
	{
		DPRINTF("rtp recvfrom error %d\n", WSAGetLastError());
		fTask->turnOffBackgroundReadHandling(fRtpSock.sock());
		return;
	}

	rtpReadHandler(fRecvBuf, bytesRead, fromAddress);
}

void RTPSource::incomingRtcpPacketHandler(void *instance, int)
{
	RTPSource *client = (RTPSource *)instance;
	client->incomingRtcpPacketHandler1();
}

void RTPSource::incomingRtcpPacketHandler1()
{
	struct sockaddr_in fromAddress;
	int len = MAX_RTP_SIZE;
	int addressSize = sizeof(fromAddress);
	int bytesRead;

	bytesRead = fRtcpSock.readSocket1(fRecvBuf, len, fromAddress);

	if(bytesRead <= 0)
	{
		DPRINTF("rtcp recvfrom error %d\n", WSAGetLastError());
		fTask->turnOffBackgroundReadHandling(fRtcpSock.sock());
		return;
	}

	rtcpReadHandler(fRecvBuf, bytesRead, fromAddress);
}

void RTPSource::rtcpReadHandler(char *buf, int len, struct sockaddr_in &fromAddress)
{
	if (len < sizeof(RTCP_HEADER))
		return;

	unsigned rtcpHdr = ntohl(*(unsigned*)buf);

	RTCP_HEADER *p = (RTCP_HEADER *)buf;

	unsigned char pt = p->pt;
	unsigned short length = 4*ntohs(p->length);

	if (fRtcpInstance) {
		fRtcpInstance->rtcpPacketHandler(buf, len);

		if (time(NULL) - fLastRtcpSendTime >= RTCP_SEND_DURATION)
			RTCPInstance::onExpire(fRtcpInstance);
	}

	if (fRtcpHandlerFunc)
		fRtcpHandlerFunc(fRtcpHandlerFuncData, fTrackId, buf, len);
}

void RTPSource::sendRtcpReport(char *buf, int len)
{
	int ret = 0;

	if (fStreamType == STREAM_TYPE_UDP || fStreamType == STREAM_TYPE_MULTICAST)
	{
		struct sockaddr_in toAddress;
		memset(&toAddress, 0, sizeof(toAddress));
		toAddress.sin_family = AF_INET;
		toAddress.sin_addr.s_addr = fSvrAddr;
		toAddress.sin_port = htons(fRtcpHisPort);

		ret = fRtcpSock.writeSocket(buf, len, toAddress);
	}
	else
	{
		if (fRtspSock)
			ret = fRtspSock->sendRTPOverTCP(buf, len, fRtcpChannelId);
	}

	fLastRtcpSendTime = time(NULL);
}

void RTPSource::copyToFrameBuffer(uint8_t *buf, int len)
{
	if (fFrameBufPos+len >= FRAME_BUFFER_SIZE) {
		DPRINTF("RTP Frame Buffer overflow %s\n", fCodecName);
		fFrameBufPos = 0;
	}
	memmove(&fFrameBuf[fFrameBufPos], buf, len);
	fFrameBufPos += len;
}

void RTPSource::resetFrameBuf()
{
	fFrameBufPos = 0;
}

uint64_t RTPSource::getMediaTimestamp(uint32_t timestamp)
{
	uint64_t msec = 1000;
	uint64_t time_msec = timestamp*msec/fTimestampFrequency;
	return time_msec;
}

void RTPSource::changeDestination(const in_addr &newDestAddr, short newDestPort)
{
	if (fRtpSock.isOpened())
		fRtpSock.changeDestination(newDestAddr, newDestPort);
	if (fRtcpSock.isOpened())
		fRtcpSock.changeDestination(newDestAddr, newDestPort+1);
}
