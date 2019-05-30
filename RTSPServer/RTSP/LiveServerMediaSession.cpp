#include "LiveServerMediaSession.h"
#include "util.h"

LiveServerMediaSession::LiveServerMediaSession(
	const char *streamName, const char *info, const char *description, bool isSSM, const char *miscSDPLines,
	StreamControl* streamControl)
: ServerMediaSession(streamName, info, description, isSSM, miscSDPLines, streamControl)
{
	fSessionType = SESSION_LIVE;
}

LiveServerMediaSession::~LiveServerMediaSession()
{
}

LiveServerMediaSubsession::LiveServerMediaSubsession(char const* trackId, char const* sdp, 
													 char const* codec, unsigned char rtpPayloadType, unsigned timestampFrequency) 
: ServerMediaSubsession(trackId, codec, rtpPayloadType, timestampFrequency)
{
	fSDPLines = strDup(sdp);
}

LiveServerMediaSubsession::~LiveServerMediaSubsession()
{
	if (fSDPLines)
		delete[] fSDPLines;
}
