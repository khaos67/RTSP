#include "MPEG4GenericRTPSource.h"
#include "MediaSession.h"
#include "RTSPCommonEnv.h"
#include "BitVector.hh"

////////// AUHeader //////////
struct AUHeader {
	unsigned size;
	unsigned index; // indexDelta for the 2nd & subsequent headers
};

MPEG4GenericRTPSource::MPEG4GenericRTPSource(int streamType, MediaSubsession &subsession, TaskScheduler &task,
char const *mode, unsigned sizeLength, unsigned indexLength, unsigned indexDeltaLength)
: RTPSource(streamType, subsession, task), fSizeLength(sizeLength), fIndexLength(indexLength), fIndexDeltaLength(indexDeltaLength),
fNumAUHeaders(0), fNextAUHeader(0), fAUHeaders(NULL)
{
    fMode = strDup(mode);
    // Check for a "mode" that we don't yet support: //#####
    if (mode == NULL || (strcmp(mode, "aac-hbr") != 0 && strcmp(mode, "generic") != 0)) {
		DPRINTF("MPEG4GenericRTPSource Warning: Unknown or unsupported \"mode\": %s\n", mode);
    }
}

MPEG4GenericRTPSource::~MPEG4GenericRTPSource()
{
	DELETE_ARRAY(fAUHeaders);
	DELETE_ARRAY(fMode);	
}

void MPEG4GenericRTPSource::processFrame(RTPPacketBuffer *packet)
{
	uint8_t *buf = (uint8_t *)packet->payload();
	int len = packet->payloadLen();

	int64_t media_timestamp = packet->extTimestamp() == 0 ? getMediaTimestamp(packet->timestamp()) : packet->extTimestamp();

	unsigned char* headerStart = buf;
	unsigned packetSize = len;

	// default values:
	unsigned resultSpecialHeaderSize = 0;
	fNumAUHeaders = 0;
	fNextAUHeader = 0;
	delete[] fAUHeaders; fAUHeaders = NULL;

	if (fSizeLength > 0) {
		// The packet begins with a "AU Header Section".  Parse it, to
		// determine the "AU-header"s for each frame present in this packet:
		resultSpecialHeaderSize += 2;
		if (packetSize < resultSpecialHeaderSize) return;

		unsigned AU_headers_length = (headerStart[0]<<8)|headerStart[1];
		unsigned AU_headers_length_bytes = (AU_headers_length+7)/8;
		if (packetSize
			< resultSpecialHeaderSize + AU_headers_length_bytes) return;
		resultSpecialHeaderSize += AU_headers_length_bytes;

		// Figure out how many AU-headers are present in the packet:
		int bitsAvail = AU_headers_length - (fSizeLength + fIndexLength);
		if (bitsAvail >= 0 && (fSizeLength + fIndexDeltaLength) > 0) {
			fNumAUHeaders = 1 + bitsAvail/(fSizeLength + fIndexDeltaLength);
		}
		if (fNumAUHeaders > 0) {
			fAUHeaders = new AUHeader[fNumAUHeaders];
			// Fill in each header:
			BitVector bv(&headerStart[2], 0, AU_headers_length);
			fAUHeaders[0].size = bv.getBits(fSizeLength);
			fAUHeaders[0].index = bv.getBits(fIndexLength);

			for (unsigned i = 1; i < fNumAUHeaders; ++i) {
				fAUHeaders[i].size = bv.getBits(fSizeLength);
				fAUHeaders[i].index = bv.getBits(fIndexDeltaLength);
			}
		}
	}

	uint8_t *ptr = &buf[resultSpecialHeaderSize];
	for (int i = 0; i < fNumAUHeaders; i++) {
		copyToFrameBuffer(ptr, fAUHeaders[i].size);
		ptr += fAUHeaders[i].size;

		if (fFrameHandlerFunc)
			fFrameHandlerFunc(fFrameHandlerFuncData, fFrameType, media_timestamp, fFrameBuf, fFrameBufPos);
		resetFrameBuf();
	}
}
