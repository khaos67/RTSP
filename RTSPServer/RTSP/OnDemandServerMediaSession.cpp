#include "OnDemandServerMediaSession.h"
#include "util.h"

OnDemandServerMediaSession::OnDemandServerMediaSession(
	const char *streamName, const char *info, const char *description, bool isSSM, const char *miscSDPLines,
	StreamControl* streamControl)
: ServerMediaSession(streamName, info, description, isSSM, miscSDPLines, streamControl)
{
	fSessionType = SESSION_ONDEMAND;
}

OnDemandServerMediaSession::~OnDemandServerMediaSession()
{
}

OnDemandServerMediaSubsession::OnDemandServerMediaSubsession(char const* trackId, char const* sdp, 
															 char const* codec, unsigned char rtpPayloadType, unsigned timestampFrequency) 
: ServerMediaSubsession(trackId, codec, rtpPayloadType, timestampFrequency)
{
	fSDPLines = strDup(sdp);
	fRTPPayloadType = rtpPayloadType;
	fTimestampFrequency = timestampFrequency;
}

OnDemandServerMediaSubsession::~OnDemandServerMediaSubsession()
{
	if (fSDPLines)
		delete[] fSDPLines;
}
