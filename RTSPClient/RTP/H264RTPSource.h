#ifndef __H264_RTP_SOURCE_H__
#define __H264_RTP_SOURCE_H__

#include "RTPSource.h"

class H264RTPSource : public RTPSource
{
public:
	H264RTPSource(int connType, MediaSubsession &subsession, TaskScheduler &task);
	virtual ~H264RTPSource();

protected:	
	virtual void processFrame(RTPPacketBuffer *packet);

	void putStartCode();
	int parseSpropParameterSets(char *spropParameterSets);	
};

#endif
