#include "AC3RTPSource.h"
#include "MediaSession.h"

AC3RTPSource::AC3RTPSource(int connType, MediaSubsession &subsession, TaskScheduler &task)
: RTPSource(connType, subsession, task)
{
}

AC3RTPSource::~AC3RTPSource()
{
}

void AC3RTPSource::processFrame(RTPPacketBuffer *packet)
{
	uint8_t *buf = packet->payload();
	int len = packet->payloadLen();
	int64_t media_timestamp = packet->extTimestamp() == 0 ? getMediaTimestamp(packet->timestamp()) : packet->extTimestamp();

	unsigned char* headerStart = buf;

	// There's a 2-byte payload header at the beginning:
	if (len < 2) return;

	unsigned char FT = headerStart[0]&0x03;
	fBeginFrame = FT != 3;

	if (fBeginFrame)
		copyToFrameBuffer(&buf[2], len-2);

	// The RTP "M" (marker) bit indicates the last fragment of a frame.
	// In case the sender did not set the "M" bit correctly, we also test for FT == 0:
	if (packet->markerBit() || FT == 0) {
		if (fFrameHandlerFunc)
			fFrameHandlerFunc(fFrameHandlerFuncData, fFrameType, media_timestamp, fFrameBuf, fFrameBufPos);
		resetFrameBuf();
		fBeginFrame = false;
	}
}
