#ifndef __H265_RTP_SOURCE_H__
#define __H265_RTP_SOURCE_H__

#include "H264RTPSource.h"

class H265RTPSource : public H264RTPSource
{
public:
	H265RTPSource(int connType, MediaSubsession& subsession, TaskScheduler& task);
	virtual ~H265RTPSource();

protected:
	virtual void processFrame(RTPPacketBuffer* packet);
};

#endif
