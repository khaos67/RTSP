#ifndef __JPEG_RTP_SOURCE_H__
#define __JPEG_RTP_SOURCE_H__

#include "RTPSource.h"

#define MAX_JPEG_HEADER_SIZE	(1024)

class JPEGRTPSource : public RTPSource
{
public:
	JPEGRTPSource(int connType, MediaSubsession &subsession, TaskScheduler &task);
	virtual ~JPEGRTPSource();

protected:	
	virtual void processFrame(RTPPacketBuffer *packet);

protected:
	unsigned fDefaultWidth, fDefaultHeight;
};

#endif
