#ifndef __OUT_PACKET_BUFFER_H__
#define __OUT_PACKET_BUFFER_H__

#include "util.h"

// A data structure that a sink may use for an output packet:
class OutPacketBuffer 
{
public:
	OutPacketBuffer(unsigned preferredPacketSize, unsigned maxPacketSize);
	~OutPacketBuffer();

	static unsigned maxSize;

	unsigned char* curPtr() const {return &fBuf[fPacketStart + fCurOffset];}
	unsigned totalBytesAvailable() const {
		return fLimit - (fPacketStart + fCurOffset);
	}
	unsigned totalBufferSize() const { return fLimit; }
	unsigned char* packet() const {return &fBuf[fPacketStart];}
	unsigned curPacketSize() const {return fCurOffset;}

	void increment(unsigned numBytes) {fCurOffset += numBytes;}

	void enqueue(unsigned char const* from, unsigned numBytes);
	void enqueueWord(unsigned word);
	void insert(unsigned char const* from, unsigned numBytes, unsigned toPosition);
	void insertWord(unsigned word, unsigned toPosition);
	void extract(unsigned char* to, unsigned numBytes, unsigned fromPosition);
	unsigned extractWord(unsigned fromPosition);

	void skipBytes(unsigned numBytes);

	bool isPreferredSize() const {return fCurOffset >= fPreferred;}
	bool wouldOverflow(unsigned numBytes) const {
		return (fCurOffset+numBytes) > fMax;
	}
	unsigned numOverflowBytes(unsigned numBytes) const {
		return (fCurOffset+numBytes) - fMax;
	}
	bool isTooBigForAPacket(unsigned numBytes) const {
		return numBytes > fMax;
	}

	void setOverflowData(unsigned overflowDataOffset,
		unsigned overflowDataSize,
	struct timeval const& presentationTime,
		unsigned durationInMicroseconds);
	unsigned overflowDataSize() const {return fOverflowDataSize;}
	struct timeval overflowPresentationTime() const {return fOverflowPresentationTime;}
	unsigned overflowDurationInMicroseconds() const {return fOverflowDurationInMicroseconds;}
	bool haveOverflowData() const {return fOverflowDataSize > 0;}
	void useOverflowData();

	void adjustPacketStart(unsigned numBytes);
	void resetPacketStart();
	void resetOffset() { fCurOffset = 0; }
	void resetOverflowData() { fOverflowDataOffset = fOverflowDataSize = 0; }

private:
	unsigned fPacketStart, fCurOffset, fPreferred, fMax, fLimit;
	unsigned char* fBuf;

	unsigned fOverflowDataOffset, fOverflowDataSize;
	struct timeval fOverflowPresentationTime;
	unsigned fOverflowDurationInMicroseconds;
};

#endif
