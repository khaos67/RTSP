#ifndef __AC3_RTP_SOURCE_H__
#define __AC3_RTP_SOURCE_H__

#include "RTPSource.h"

class AC3RTPSource : public RTPSource
{
public:
	AC3RTPSource(int connType, MediaSubsession &subsession, TaskScheduler &task);
	virtual ~AC3RTPSource();

protected:
	virtual void processFrame(RTPPacketBuffer *packet);
};

#endif
