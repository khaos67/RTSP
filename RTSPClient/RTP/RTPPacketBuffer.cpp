#include "RTPPacketBuffer.h"
#include "RTSPCommonEnv.h"
#include "util.h"

RTPPacketBuffer::RTPPacketBuffer() : fBuf(NULL), fLength(0), fVersion(0), fPadding(0), fExtension(0), fCSRCCount(0),
fMarkerBit(0), fPayloadType(0), fSequenceNum(0), fTimestamp(0), fSSRC(0), fNextPacket(NULL), fIsFirstPacket(false), fExtTimestamp(0)
{
	fBuf = new uint8_t[MAX_RTP_PACKET_SIZE];
	fCurPtr = fBuf;
}

RTPPacketBuffer::~RTPPacketBuffer()
{
	DELETE_OBJECT(fNextPacket);
	DELETE_ARRAY(fBuf);
}

uint8_t* RTPPacketBuffer::payload()
{
	return fCurPtr;
}

int RTPPacketBuffer::payloadLen()
{
	uint8_t *ptrLast = &fBuf[fLength-1];
	return ptrLast-fCurPtr+1;
}

bool RTPPacketBuffer::packetHandler(uint8_t *buf, int len)
{
	if (len < sizeof(RTP_HEADER) || len > MAX_RTP_PACKET_SIZE) {
		DPRINTF("invalid rtp length %u\n", len);
		return false;
	}

	memcpy(fBuf, buf, len);
	fCurPtr = fBuf;
	fLength = len;

	RTP_HEADER *p = (RTP_HEADER *)fBuf;
	fCSRCCount = p->cc;
	fExtension = p->ext;
	fPadding = p->pad;
	fVersion = p->ver;
	fPayloadType = p->pt;
	fMarkerBit = p->mk;
	fSequenceNum = ntohs(p->seq);
	fTimestamp = ntohl(p->ts);
	fSSRC = ntohl(p->ssrc);

	fCurPtr += sizeof(RTP_HEADER);

	// check RTP version (it must be 2)
	if (fVersion != 2)
		DPRINTF("invalid rtp version %u\n", fVersion);	

	// skip CSRC
	if (fCSRCCount > 0) {
		if (payloadLen() <= fCSRCCount*4) {
			DPRINTF("invalid rtp header, CSRC count error %u\n", fCSRCCount);
			return false;
		} else {
			fCurPtr += (fCSRCCount*4);
		}
	}

	// skip Extension field
	if (fExtension) {
		if (payloadLen() <= 4) {
			DPRINTF("invalid rtp header, extension length error\n");
			return false;
		} else {
			EXT_HEADER *dxmHdr = (EXT_HEADER *)fCurPtr;
			unsigned extHdr = ntohl(*(unsigned *)fCurPtr); fCurPtr += 4;
			unsigned remExtSize = 4*(extHdr&0xFFFF);
			if (payloadLen() <= remExtSize) {
				DPRINTF("invalid rtp header, extension size error %u\n", remExtSize);
				return false;
			} else {
				// process dxm extension header;
				if (dxmHdr->profile == 0x8110) {
					dxmHdr->length = ntohs(dxmHdr->length);
					dxmHdr->timestamp = checkEndian() == 0 ? ntohll(dxmHdr->timestamp) : dxmHdr->timestamp;				
					//DPRINTF("%d ext : %d, %lld\n", fPayloadType, dxmHdr->length, dxmHdr->timestamp);
					fExtTimestamp = dxmHdr->timestamp;
				}
				fCurPtr += remExtSize;
			}
		}
	}

	// remove padding
	if (fPadding) {
		if (payloadLen() <= 0) {
			DPRINTF("invalid rtp header, padding error\n");
			return false;
		} else {
			unsigned numPaddingBytes = (unsigned)fBuf[fLength-1];
			if (payloadLen() <= numPaddingBytes) {
				DPRINTF("invalid rtp header, padding length error\n");
				return false;
			} else {
				fLength -= numPaddingBytes;
				fPadding = p->pad = 0;
			}
		}
	}

	gettimeofday(&fTimeReceived, NULL);

	return true;
}

void RTPPacketBuffer::reset()
{
	fCurPtr = fBuf;
	fLength = 0;
	fIsFirstPacket = false;
}

ReorderingPacketBuffer::ReorderingPacketBuffer() : fThresholdTime(100000), fHaveSeenFirstPacket(false), fHeadPacket(NULL), fTailPacket(NULL),
fSavedPacket(NULL), fSavedPacketFree(true)
{
}

ReorderingPacketBuffer::~ReorderingPacketBuffer()
{
	reset();
}

void ReorderingPacketBuffer::reset()
{
	if (fSavedPacketFree) {
		DELETE_OBJECT(fSavedPacket);
		fSavedPacketFree = false;
	}	
	delete fHeadPacket;
	fHaveSeenFirstPacket = false;
	fHeadPacket = fTailPacket = NULL;
}

RTPPacketBuffer* ReorderingPacketBuffer::getFreePacket()
{
	if (fSavedPacket == NULL) {
		fSavedPacket = new RTPPacketBuffer();
		fSavedPacketFree = true;
	}

	if (fSavedPacketFree == true) {
		fSavedPacketFree = false;
		return fSavedPacket;
	} else {
		return new RTPPacketBuffer;
	}
}

void ReorderingPacketBuffer::freePacket(RTPPacketBuffer *packet)
{
	if (packet != fSavedPacket) {
		delete packet;
	} else {
		fSavedPacketFree = true;
	}
}

bool ReorderingPacketBuffer::storePacket(RTPPacketBuffer* packet) 
{
	unsigned short rtpSeqNo = packet->sequenceNum();

	if (!fHaveSeenFirstPacket) {
		fNextExpectedSeqNo = rtpSeqNo; // initialization
		packet->isFirstPacket() = true;
		fHaveSeenFirstPacket = true;
	}

	// Ignore this packet if its sequence number is less than the one
	// that we're looking for (in this case, it's been excessively delayed).
	if (seqNumLT(rtpSeqNo, fNextExpectedSeqNo)) return false;

	if (fTailPacket == NULL) {
		// Common case: There are no packets in the queue; this will be the first one:
		packet->nextPacket() = NULL;
		fHeadPacket = fTailPacket = packet;
		return true;
	}

	if (seqNumLT(fTailPacket->sequenceNum(), rtpSeqNo)) {
		// The next-most common case: There are packets already in the queue; this packet arrived in order => put it at the tail:
		packet->nextPacket() = NULL;
		fTailPacket->nextPacket() = packet;
		fTailPacket = packet;
		return true;
	} 

	if (rtpSeqNo == fTailPacket->sequenceNum()) {
		// This is a duplicate packet - ignore it
		return false;
	}

	// Rare case: This packet is out-of-order.  Run through the list (from the head), to figure out where it belongs:
	RTPPacketBuffer* beforePtr = NULL;
	RTPPacketBuffer* afterPtr = fHeadPacket;
	while (afterPtr != NULL) {
		if (seqNumLT(rtpSeqNo, afterPtr->sequenceNum())) break; // it comes here
		if (rtpSeqNo == afterPtr->sequenceNum()) {
			// This is a duplicate packet - ignore it
			return false;
		}

		beforePtr = afterPtr;
		afterPtr = afterPtr->nextPacket();
	}

	// Link our new packet between "beforePtr" and "afterPtr":
	packet->nextPacket() = afterPtr;
	if (beforePtr == NULL) {
		fHeadPacket = packet;
	} else {
		beforePtr->nextPacket() = packet;
	}

	return true;
}

void ReorderingPacketBuffer::releaseUsedPacket(RTPPacketBuffer* packet) 
{
	// ASSERT: packet == fHeadPacket
	// ASSERT: fNextExpectedSeqNo == packet->rtpSeqNo()
	++fNextExpectedSeqNo; // because we're finished with this packet now

	fHeadPacket = fHeadPacket->nextPacket();
	if (!fHeadPacket) { 
		fTailPacket = NULL;
	}
	packet->nextPacket() = NULL;

	freePacket(packet);
}

RTPPacketBuffer* ReorderingPacketBuffer::getNextCompletedPacket(bool& packetLossPreceded)
{
	if (fHeadPacket == NULL) 
		return NULL;

	// Check whether the next packet we want is already at the head
	// of the queue:
	// ASSERT: fHeadPacket->rtpSeqNo() >= fNextExpectedSeqNo
	if (fHeadPacket->sequenceNum() == fNextExpectedSeqNo) {
		packetLossPreceded = fHeadPacket->isFirstPacket();
		// (The very first packet is treated as if there was packet loss beforehand.)
		return fHeadPacket;
	}

	// We're still waiting for our desired packet to arrive.  However, if
	// our time threshold has been exceeded, then forget it, and return
	// the head packet instead:
	bool timeThresholdHasBeenExceeded;
	if (fThresholdTime == 0) {
		timeThresholdHasBeenExceeded = true; // optimization
	} else {
		struct timeval timeNow;
		gettimeofday(&timeNow, NULL);
		unsigned uSecondsSinceReceived
			= (timeNow.tv_sec - fHeadPacket->timeReceived().tv_sec)*1000000
			+ (timeNow.tv_usec - fHeadPacket->timeReceived().tv_usec);
		timeThresholdHasBeenExceeded = uSecondsSinceReceived > fThresholdTime;
	}
	if (timeThresholdHasBeenExceeded) {
		fNextExpectedSeqNo = fHeadPacket->sequenceNum();
		// we've given up on earlier packets now
		packetLossPreceded = true;
		return fHeadPacket;
	}

	// Otherwise, keep waiting for our desired packet to arrive:
	return NULL;
}
