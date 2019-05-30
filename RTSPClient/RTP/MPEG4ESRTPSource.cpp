#include "MPEG4ESRTPSource.h"
#include "MediaSession.h"

unsigned char* parseGeneralConfigStr(char const* configStr, unsigned& configSize);

MPEG4ESRTPSource::MPEG4ESRTPSource(int streamType, MediaSubsession &subsession, TaskScheduler &task)
: RTPSource(streamType, subsession, task)
{
	fExtraData = parseGeneralConfigStr((char const*)subsession.fmtp_config(), fExtraDataSize);
}

MPEG4ESRTPSource::~MPEG4ESRTPSource()
{
}

void MPEG4ESRTPSource::processFrame(RTPPacketBuffer *packet)
{
	uint8_t *buf = (uint8_t *)packet->payload();
	int len = packet->payloadLen();
	int64_t media_timestamp = packet->extTimestamp() == 0 ? getMediaTimestamp(packet->timestamp()) : packet->extTimestamp();

	if (len >= 4) {
		if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1)
			fBeginFrame = true;
	}

	if (fBeginFrame && !fIsStartFrame) {
		if (fExtraData) 
			copyToFrameBuffer(fExtraData, fExtraDataSize);
		fIsStartFrame = true;
	}

	if (fBeginFrame)
		copyToFrameBuffer(buf, len);

	if (packet->markerBit()) {
		if (fFrameHandlerFunc)
			fFrameHandlerFunc(fFrameHandlerFuncData, fFrameType, media_timestamp, fFrameBuf, fFrameBufPos);
		resetFrameBuf();
		fBeginFrame = false;
	}
}

static bool getNibble(char const*& configStr,
			 unsigned char& resultNibble) 
{
	char c = configStr[0];
	if (c == '\0') return false; // we've reached the end

	if (c >= '0' && c <= '9') {
		resultNibble = c - '0';
	} else if (c >= 'A' && c <= 'F') {
		resultNibble = 10 + c - 'A';
	} else if (c >= 'a' && c <= 'f') {
		resultNibble = 10 + c - 'a';
	} else {
		return false;
	}

	++configStr; // move to the next nibble
	return true;
}

static bool getByte(char const*& configStr, unsigned char& resultByte) 
{
	resultByte = 0; // by default, in case parsing fails

	unsigned char firstNibble;
	if (!getNibble(configStr, firstNibble)) return false;
	resultByte = firstNibble<<4;

	unsigned char secondNibble = 0;
	if (!getNibble(configStr, secondNibble) && configStr[0] != '\0') {
		// There's a second nibble, but it's malformed
		return false;
	}
	resultByte |= secondNibble;

	return true;
}

unsigned char* parseGeneralConfigStr(char const* configStr,
				     // result parameter:
				     unsigned& configSize) 
{
	unsigned char* config = NULL;
	do {
		if (configStr == NULL) break;
		configSize = (strlen(configStr)+1)/2;

		config = new unsigned char[configSize];
		if (config == NULL) break;

		unsigned i;
		for (i = 0; i < configSize; ++i) {
			if (!getByte(configStr, config[i])) break;
		}
		if (i != configSize) break; // part of the string was bad

		return config;
	} while (0);

	configSize = 0;
	delete[] config;
	return NULL;
}
