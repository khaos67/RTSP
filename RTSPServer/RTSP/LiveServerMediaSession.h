#ifndef __LIVE_SERVER_MEDIA_SESSION_H__
#define __LIVE_SERVER_MEDIA_SESSION_H__

#include "ServerMediaSession.h"

class LiveServerMediaSession : public ServerMediaSession
{
public:
	LiveServerMediaSession(
		char const* streamName, 
		char const* info, 
		char const* description,
		bool isSSM,
		char const* miscSDPLines,
		StreamControl* streamControl = NULL);	

	virtual ~LiveServerMediaSession();
};

class LiveServerMediaSubsession : public ServerMediaSubsession
{
public:
	LiveServerMediaSubsession(char const* trackId, char const* sdp, char const* codec, unsigned char rtpPayloadType, unsigned timestampFrequency);
	virtual ~LiveServerMediaSubsession();
};

#endif
