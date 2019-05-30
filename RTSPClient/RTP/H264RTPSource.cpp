#include <stdio.h>

#include "H264RTPSource.h"
#include "MediaSession.h"
#include "RTSPCommonEnv.h"

H264RTPSource::H264RTPSource(int connType, MediaSubsession &subsession, TaskScheduler &task)
: RTPSource(connType, subsession, task)
{
	parseSpropParameterSets((char *)subsession.fmtp_spropparametersets());
}

H264RTPSource::~H264RTPSource()
{
}

void H264RTPSource::putStartCode()
{
	fFrameBuf[fFrameBufPos++] = 0x00;
	fFrameBuf[fFrameBufPos++] = 0x00;
	fFrameBuf[fFrameBufPos++] = 0x00;
	fFrameBuf[fFrameBufPos++] = 0x01;
}

void H264RTPSource::processFrame(RTPPacketBuffer *packet)
{
	uint8_t *buf = (uint8_t *)packet->payload();
	int len = packet->payloadLen();

	int offset = trimStartCode(buf, len);
	buf = &buf[offset];
	len -= offset;

	uint8_t *buf_ptr = buf;
	bool isCompleteFrame = false;

	int64_t media_timestamp = packet->extTimestamp() == 0 ? getMediaTimestamp(packet->timestamp()) : packet->extTimestamp();

	uint8_t nalUnitType = (buf[0]&0x1F);

	if (RTSPCommonEnv::nDebugFlag&DEBUG_FLAG_RTP_PAYLOAD)
		DPRINTF("nal_type: %d, size: %d\n", nalUnitType, len);

	if (!fIsStartFrame) {
		if (fExtraData) {
			putStartCode();
			offset = trimStartCode(fExtraData, fExtraDataSize);
			copyToFrameBuffer(&fExtraData[offset], fExtraDataSize - offset);
		}
		fIsStartFrame = true;
	}

	switch (nalUnitType)
	{
	case 28: {	// FU-A
		uint8_t startBit = buf[1]&0x80;
		uint8_t endBit = buf[1]&0x40;

		if (startBit) {
			buf_ptr++; len--;
			buf[1] = (buf[0]&0xE0) + (buf[1]&0x1F);
			putStartCode();
		} else {
			buf_ptr += 2; len -= 2;
		}

		copyToFrameBuffer(buf_ptr, len);
		isCompleteFrame = (endBit != 0);		
		break;
			 }
	case 5: {	// IDR-Picture
		putStartCode();
		copyToFrameBuffer(buf_ptr, len);
		isCompleteFrame = true;
		break;
			}
	case 7: {	// SPS
		putStartCode();
		copyToFrameBuffer(buf_ptr, len);
		isCompleteFrame = false;
		break;
			}
	case 8: {	// PPS
		putStartCode();
		copyToFrameBuffer(buf_ptr, len);
		isCompleteFrame = false;
		break;
			}
	case 24: {	// STAP-A
		buf_ptr++; len--;
		while (len > 3)
		{
			uint16_t staplen = (buf_ptr[0]<<8) | (buf_ptr[1]);
			if (staplen > len) {
				DPRINTF("STAP-A process error, staplen: %d, len\n", staplen, len);
				break;
			}

			buf_ptr += 2; len -= 2;
			nalUnitType = buf_ptr[0]&0x1F;

			putStartCode();
			copyToFrameBuffer(buf_ptr, staplen);

			buf_ptr += staplen; len -= staplen;

			if (fFrameHandlerFunc)
				fFrameHandlerFunc(fFrameHandlerFuncData, fFrameType, media_timestamp, fFrameBuf, fFrameBufPos);
			resetFrameBuf();
		}
		break;
			 }
	default:
		putStartCode();
		copyToFrameBuffer(buf_ptr, len);
		isCompleteFrame = true;
		break;
	}

	if (isCompleteFrame) {
		if (fFrameHandlerFunc)
			fFrameHandlerFunc(fFrameHandlerFuncData, fFrameType, media_timestamp, fFrameBuf, fFrameBufPos);
		resetFrameBuf();
	}
}

/*char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";*/
static int b64_decode( char *dest, char *src )
{
	const char *dest_start = dest;
	int  i_level;
	int  last = 0;
	int  b64[256] = {
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
		52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
		15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
		-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
		41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
	};

	for( i_level = 0; *src != '\0'; src++ )
	{
		int  c;

		c = b64[(unsigned int)*src];
		if( c == -1 )
		{
			continue;
		}

		switch( i_level )
		{
		case 0:
			i_level++;
			break;
		case 1:
			*dest++ = ( last << 2 ) | ( ( c >> 4)&0x03 );
			i_level++;
			break;
		case 2:
			*dest++ = ( ( last << 4 )&0xf0 ) | ( ( c >> 2 )&0x0f );
			i_level++;
			break;
		case 3:
			*dest++ = ( ( last &0x03 ) << 6 ) | c;
			i_level = 0;
		}
		last = c;
	}

	*dest = '\0';

	return dest - dest_start;
}

unsigned char* parseH264ConfigStr(char const* configStr, unsigned int& configSize, unsigned int& spsSize)
{
	char *dup, *psz;
	int i, i_records = 1;

	if( configSize )
		configSize = 0;

	if( spsSize )
		spsSize = 0;

	if( configStr == NULL || *configStr == '\0' )
		return NULL;

	dup = new char[strlen(configStr)+1];
	memset(dup, 0, strlen(configStr)+1);
	memcpy(dup, configStr, strlen(configStr)+1);
	psz = dup;

	/* Count the number of comma's */
	for( psz = dup; *psz != '\0'; ++psz )
	{
		if( *psz == ',')
		{
			++i_records;
			*psz = '\0';
		}
	}

	int sz = 5 * strlen(dup);
	if (sz == 0) {
		delete[] dup;
		return NULL;
	}

	unsigned char *cfg = new unsigned char[sz];
	memset(cfg, 0, sz);
	psz = dup;
	for( i = 0; i < i_records; i++ )
	{
		cfg[configSize++] = 0x00;
		cfg[configSize++] = 0x00;
		cfg[configSize++] = 0x00;
		cfg[configSize++] = 0x01;

		configSize += b64_decode( (char*)&cfg[configSize], psz );

		psz += strlen(psz)+1;
		if (i == 0)		// SPS
			spsSize = configSize;
	}

	delete[] dup;
	return cfg;
}

int H264RTPSource::parseSpropParameterSets(char *spropParameterSets)
{
	if (spropParameterSets == NULL)
		return -1;

	unsigned int config_size = 0, sps_size = 0;

	fExtraData = parseH264ConfigStr(spropParameterSets, fExtraDataSize, sps_size);
        	
	return 0;
}
