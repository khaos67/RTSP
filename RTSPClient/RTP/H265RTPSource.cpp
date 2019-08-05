#include "H265RTPSource.h"
#include "MediaSession.h"
#include "RTSPCommonEnv.h"

H265RTPSource::H265RTPSource(int connType, MediaSubsession& subsession, TaskScheduler& task)
	: H264RTPSource(connType, subsession, task)
{

}

H265RTPSource::~H265RTPSource()
{

}

void H265RTPSource::processFrame(RTPPacketBuffer* packet)
{
	uint8_t* buf = (uint8_t*)packet->payload();
	int len = packet->payloadLen();

	uint8_t* buf_ptr = buf;
	uint8_t* headerStart = buf;
	bool isCompleteFrame = false;

	int64_t media_timestamp = packet->extTimestamp() == 0 ? getMediaTimestamp(packet->timestamp()) : packet->extTimestamp();

	uint8_t nalUnitType = (headerStart[0] & 0x7E) >> 1;

	if (RTSPCommonEnv::nDebugFlag & DEBUG_FLAG_RTP_PAYLOAD)
		DPRINTF("nal_type: %d, size: %d\n", nalUnitType, len);

	switch (nalUnitType) {
	case 48: {	// Aggregation Packet (AP)
		buf_ptr += 2; len -= 2;
		while (len > 3)
		{
			uint16_t nalUSize = (buf_ptr[0] << 8) | (buf_ptr[1]);
			if (nalUSize > len) {
				DPRINTF("Aggregation Packet process error, staplen: %d, len\n", nalUSize, len);
				break;
			}

			buf_ptr += 2; len -= 2;
			nalUnitType = (buf_ptr[0] & 0x7E) >> 1;

			putStartCode();
			copyToFrameBuffer(buf_ptr, nalUSize);

			buf_ptr += nalUSize; len -= nalUSize;

			if (fFrameHandlerFunc)
				fFrameHandlerFunc(fFrameHandlerFuncData, fFrameType, media_timestamp, fFrameBuf, fFrameBufPos);
			resetFrameBuf();
		}
	} break;
	case 49: {	// Fragmentation Unit (FU)
		uint8_t startBit = headerStart[2] & 0x80;
		uint8_t endBit = headerStart[2] & 0x40;
		if (startBit) {
			uint8_t nal_unit_type = headerStart[2] & 0x3F;
			uint8_t newNalHeader[2];
			newNalHeader[0] = (headerStart[0] & 0x81) | (nal_unit_type << 1);
			newNalHeader[1] = headerStart[1];

			headerStart[1] = newNalHeader[0];
			headerStart[2] = newNalHeader[1];
			buf_ptr++; len--;

			putStartCode();
		}
		else {
			buf_ptr += 3; len -= 3;
		}
		copyToFrameBuffer(buf_ptr, len);
		isCompleteFrame = (endBit != 0);
	} break;
	default: {	// This packet contains one complete NAL unit:
		putStartCode();
		copyToFrameBuffer(buf_ptr, len);
		isCompleteFrame = true;
	} break;
	}

	if (isCompleteFrame) {
		if (fFrameHandlerFunc)
			fFrameHandlerFunc(fFrameHandlerFuncData, fFrameType, media_timestamp, fFrameBuf, fFrameBufPos);
		resetFrameBuf();
	}
}