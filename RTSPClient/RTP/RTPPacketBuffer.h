#ifndef __RTP_PACKET_BUFFER_H__
#define __RTP_PACKET_BUFFER_H__

#include "NetCommon.h"
#include "RTSPCommon.h"

#define MAX_RTP_PACKET_SIZE	(1024*1024)

class RTPPacketBuffer {
public:
	RTPPacketBuffer();
	virtual ~RTPPacketBuffer();

	uint8_t* buf() { return fBuf; }
	uint8_t* payload();
	int length() { return fLength; }
	int payloadLen();

	uint16_t	version() { return fVersion; }
	uint16_t	padding() { return fPadding; }
	uint16_t	extension() { return fExtension; }
	uint16_t	csrcCount() { return fCSRCCount; }
	uint16_t	markerBit()	{ return fMarkerBit; }
	uint16_t	payloadType() { return fPayloadType; }
	uint16_t	sequenceNum() { return fSequenceNum; }
	uint32_t	timestamp() { return fTimestamp; }
	uint32_t	ssrc() { return fSSRC; }

	int64_t		extTimestamp() { return fExtTimestamp; }

	bool packetHandler(uint8_t *buf, int len);
	void reset();

	struct timeval const& timeReceived() const { return fTimeReceived; }
	bool& isFirstPacket() { return fIsFirstPacket; }
	RTPPacketBuffer*& nextPacket() { return fNextPacket; }

private:
	uint8_t*	fBuf;
	uint8_t*	fCurPtr;
	int			fLength;
	uint16_t	fVersion;
	uint16_t	fPadding;
	uint16_t	fExtension;
	uint16_t	fCSRCCount;
	uint16_t	fMarkerBit;
	uint16_t	fPayloadType;
	uint16_t	fSequenceNum;
	uint32_t	fTimestamp;
	uint32_t	fSSRC;
	int64_t		fExtTimestamp;

	struct timeval	fTimeReceived;
	bool			fIsFirstPacket;

	RTPPacketBuffer	*fNextPacket;
};

class ReorderingPacketBuffer {
public:
	ReorderingPacketBuffer();
	virtual ~ReorderingPacketBuffer();
	void reset();

	RTPPacketBuffer* getFreePacket();
	void freePacket(RTPPacketBuffer *packet);
	bool storePacket(RTPPacketBuffer *packet);
	void releaseUsedPacket(RTPPacketBuffer *packet);
	RTPPacketBuffer* getNextCompletedPacket(bool& packetLostPreceded);

private:
	unsigned	fThresholdTime;	// useconds
	bool		fHaveSeenFirstPacket;
	unsigned short	fNextExpectedSeqNo;

	RTPPacketBuffer*	fHeadPacket;
	RTPPacketBuffer*	fTailPacket;
	RTPPacketBuffer*	fSavedPacket;
	bool				fSavedPacketFree;
};

#endif
