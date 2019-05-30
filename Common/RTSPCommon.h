#ifndef __RTSP_COMMON_H__
#define __RTSP_COMMON_H__

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;

typedef unsigned char		u_int8_t;
typedef unsigned short		u_int16_t;
typedef unsigned int		u_int32_t;

#ifdef WIN32
typedef unsigned __int64	uint64_t;
typedef unsigned __int64	u_int64_t;
typedef __int64				int64_t;
#else
#include <inttypes.h>
#endif

#pragma pack(push, 1)

typedef struct 
{				
	uint16_t    cc:     4;      /* csrc count */
	uint16_t    ext:    1;      /* header extension flag */
	uint16_t    pad:    1;      /* padding flag - for encryption */
	uint16_t    ver:    2;      /* protocal version */
	uint16_t    pt:     7;      /* payload type */
	uint16_t    mk:     1;      /* marker bit - for profile */
	uint16_t	seq;			/* sequence number of this packet */
	uint32_t	ts;				/* timestamp of this packet */
	uint32_t	ssrc;			/* source of packet */
} RTP_HEADER;

typedef struct
{
	uint16_t	rc:		5;
	uint16_t	pad:	1;
	uint16_t	ver:	2;
	uint16_t	pt:		8;
	uint16_t	length;
} RTCP_HEADER;

typedef struct
{
	uint16_t	profile;
	uint16_t	length;
	int64_t	timestamp;
} EXT_HEADER;

#pragma pack(pop)

enum STREAM_TYPE { STREAM_TYPE_UDP = 0, STREAM_TYPE_TCP = 1, STREAM_TYPE_MULTICAST = 2 };

bool seqNumLT(u_int16_t s1, u_int16_t s2);

#define RTSP_PARAM_STRING_MAX 200

bool parseRTSPRequestString(char const *reqStr, unsigned reqStrSize,
							char *resultCmdName,
							unsigned resultCmdNameMaxSize,
							char* resultURLPreSuffix,
							unsigned resultURLPreSuffixMaxSize,
							char* resultURLSuffix,
							unsigned resultURLSuffixMaxSize,
							char* resultCSeq,
							unsigned resultCSeqMaxSize,
							char* resultSessionId,
							unsigned resultSessionIdMaxSize,
							unsigned& contentLength);

bool parseRangeParam(char const* paramStr, double& rangeStart, double& rangeEnd, char*& absStartTime, char*& absEndTime, bool& startTimeIsNow);
bool parseRangeHeader(char const* buf, double& rangeStart, double& rangeEnd, char*& absStartTime, char*& absEndTime, bool& startTimeIsNow);
bool parseScaleHeader(char const* buf, float& scale);

bool parseRTSPURL(char const *url, unsigned &address, unsigned short &port, char const **urlSuffix);
bool parseRTSPURLUsernamePassword(char const* url, char*& username, char*& password);

char const* dateHeader(); // A "Date:" header that can be used in a RTSP (or HTTP) response 

int trimStartCode(uint8_t *buf, int len);

char* getLine(char* startOfLine);

int checkEndian();	// 0: little endian, 1: big endian

#define htonll(x) \
 ((((x) & 0xff00000000000000LL) >> 56) | \
 (((x) & 0x00ff000000000000LL) >> 40) | \
 (((x) & 0x0000ff0000000000LL) >> 24) | \
 (((x) & 0x000000ff00000000LL) >> 8) | \
 (((x) & 0x00000000ff000000LL) << 8) | \
 (((x) & 0x0000000000ff0000LL) << 24) | \
 (((x) & 0x000000000000ff00LL) << 40) | \
 (((x) & 0x00000000000000ffLL) << 56))
#define ntohll(x) \
 ((((x) & 0x00000000000000FF) << 56) | \
 (((x) & 0x000000000000FF00) << 40) | \
 (((x) & 0x0000000000FF0000) << 24) | \
 (((x) & 0x00000000FF000000) << 8)  | \
 (((x) & 0x000000FF00000000) >> 8)  | \
 (((x) & 0x0000FF0000000000) >> 24) | \
 (((x) & 0x00FF000000000000) >> 40) | \
 (((x) & 0xFF00000000000000) >> 56))

char* createSDPString(char *mediaType, unsigned char payloadType, char *codec, unsigned frequency, char *trackId);

#endif
