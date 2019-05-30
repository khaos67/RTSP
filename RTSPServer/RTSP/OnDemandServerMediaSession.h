#ifndef __ONDEMAND_SERVER_MEDIA_SESSION_H__
#define __ONDEMAND_SERVER_MEDIA_SESSION_H__

#include "ServerMediaSession.h"

class OnDemandServerMediaSession : public ServerMediaSession
{
public:
	OnDemandServerMediaSession(
		char const* streamName, 
		char const* info, 
		char const* description,
		bool isSSM,
		char const* miscSDPLines,
		StreamControl* streamControl);	

	virtual ~OnDemandServerMediaSession();
};

class OnDemandServerMediaSubsession : public ServerMediaSubsession
{
public:
	OnDemandServerMediaSubsession(char const* trackId, char const* sdp, 
		char const* codec, unsigned char rtpPayloadType, unsigned timestampFrequency);
	virtual ~OnDemandServerMediaSubsession();
};

#endif
