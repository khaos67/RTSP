#ifndef __MPEG4GENERIC_RTP_SOURCE_H__
#define __MPEG4GENERIC_RTP_SOURCE_H__

#include "RTPSource.h"

class MPEG4GenericRTPSource : public RTPSource
{
public:
	MPEG4GenericRTPSource(int streamType, MediaSubsession &subsession, TaskScheduler &task,
		char const *mode, unsigned sizeLength, unsigned indexLength, unsigned indexDeltaLength);
	virtual ~MPEG4GenericRTPSource();

protected:
	virtual void processFrame(RTPPacketBuffer *packet);

protected:
	char *fMode;
	unsigned fSizeLength, fIndexLength, fIndexDeltaLength;
	unsigned fNumAUHeaders; // in the most recently read packet
	unsigned fNextAUHeader; // index of the next AU Header to read
	struct AUHeader* fAUHeaders;
};

#endif
