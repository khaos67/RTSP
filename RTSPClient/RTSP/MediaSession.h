#ifndef __MEDIA_SESSION_H__
#define __MEDIA_SESSION_H__

#include <stdio.h>
#include "RTPSource.h"

class MediaSubsession; // forward

class MediaSession
{
public:
	static MediaSession* createNew(char const* sdpDescription);
	virtual ~MediaSession();

	bool hasSubsessions() const { return fSubsessionsHead != NULL; }
	double& playStartTime() { return fMaxPlayStartTime; }
	double& playEndTime() { return fMaxPlayEndTime; }
	char* connectionEndpointName() const { return fConnectionEndpointName; }
	char const* CNAME() const { return fCNAME; }
	struct in_addr const& sourceFilterAddr() const { return fSourceFilterAddr; }
	float& scale() { return fScale; }
	char* mediaSessionType() const { return fMediaSessionType; }
	char* sessionName() const { return fSessionName; }
	char* sessionDescription() const { return fSessionDescription; }
	char const* controlPath() const { return fControlPath; }

protected:
	MediaSession();

	bool initializeWithSDP(char const* sdpDescription);
	bool parseSDPLine(char const* input, char const*& nextLine);
	bool parseSDPLine_s(char const* sdpLine);
	bool parseSDPLine_i(char const* sdpLine);
	bool parseSDPLine_c(char const* sdpLine);
	bool parseSDPAttribute_type(char const* sdpLine);
	bool parseSDPAttribute_control(char const* sdpLine);
	bool parseSDPAttribute_range(char const* sdpLine);
	bool parseSDPAttribute_source_filter(char const* sdpLine);

	static char* lookupPayloadFormat(unsigned char rtpPayloadType,
		unsigned& rtpTimestampFrequency,
		unsigned& numChannels);
	static unsigned guessRTPTimestampFrequency(char const* mediumName,
		char const* codecName);

protected:
	friend class MediaSubsessionIterator;
	char* fCNAME; // used for RTCP

	// Linkage fields:
	MediaSubsession* fSubsessionsHead;
	MediaSubsession* fSubsessionsTail;

	// Fields set from a SDP description:
	char* fConnectionEndpointName;
	double fMaxPlayStartTime;
	double fMaxPlayEndTime;
	struct in_addr fSourceFilterAddr; // used for SSM
	float fScale; // set from a RTSP "Scale:" header
	char* fMediaSessionType; // holds a=type value
	char* fSessionName; // holds s=<session name> value
	char* fSessionDescription; // holds i=<session description> value
	char* fControlPath; // holds optional a=control: string
};


class MediaSubsessionIterator 
{
public:
	MediaSubsessionIterator(MediaSession& session);
	virtual ~MediaSubsessionIterator();

	MediaSubsession* next(); // NULL if none
	void reset();

private:
	MediaSession& fOurSession;
	MediaSubsession* fNextPtr;
};


class MediaSubsession 
{
public:
	MediaSession& parentSession() { return fParent; }
	MediaSession const& parentSession() const { return fParent; }

	unsigned short clientPortNum() const { return fClientPortNum; }
	unsigned char rtpPayloadFormat() const { return fRTPPayloadFormat; }
	char const* savedSDPLines() const { return fSavedSDPLines; }
	char const* mediumName() const { return fMediumName; }
	char const* codecName() const { return fCodecName; }
	char const* protocolName() const { return fProtocolName; }
	char const* controlPath() const { return fControlPath; }
	bool isSSM() const { return fSourceFilterAddr.s_addr != 0; }

	unsigned short videoWidth() const { return fVideoWidth; }
	unsigned short videoHeight() const { return fVideoHeight; }
	unsigned videoFPS() const { return fVideoFPS; }
	unsigned numChannels() const { return fNumChannels; }
	float& scale() { return fScale; }

	unsigned rtpTimestampFrequency() const { return fRTPTimestampFrequency; }

	double playStartTime() const;
	double playEndTime() const;
	// Used only to set the local fields:
	double& _playStartTime() { return fPlayStartTime; }
	double& _playEndTime() { return fPlayEndTime; }

	bool initiate(int streamType, TaskScheduler &task, bool rtpOnly = false);
	// Creates a "RTPSource" for this subsession. (Has no effect if it's
	// already been created.)  Returns true iff this succeeds.
	void deInitiate(); // Destroys any previously created RTPSource
	bool setClientPortNum(unsigned short portNum);
	// Sets the preferred client port number that any "RTPSource" for
	// this subsession would use.  (By default, the client port number
	// is gotten from the original SDP description, or - if the SDP
	// description does not specfy a client port number - an ephemeral
	// (even) port number is chosen.)  This routine should *not* be
	// called after initiate().
	char*& connectionEndpointName() { return fConnectionEndpointName; }
	char const* connectionEndpointName() const {
		return fConnectionEndpointName;
	}

	// Various parameters set in "a=fmtp:" SDP lines:
	unsigned fmtp_auxiliarydatasizelength() const { return fAuxiliarydatasizelength; }
	unsigned fmtp_constantduration() const { return fConstantduration; }
	unsigned fmtp_constantsize() const { return fConstantsize; }
	unsigned fmtp_crc() const { return fCRC; }
	unsigned fmtp_ctsdeltalength() const { return fCtsdeltalength; }
	unsigned fmtp_de_interleavebuffersize() const { return fDe_interleavebuffersize; }
	unsigned fmtp_dtsdeltalength() const { return fDtsdeltalength; }
	unsigned fmtp_indexdeltalength() const { return fIndexdeltalength; }
	unsigned fmtp_indexlength() const { return fIndexlength; }
	unsigned fmtp_interleaving() const { return fInterleaving; }
	unsigned fmtp_maxdisplacement() const { return fMaxdisplacement; }
	unsigned fmtp_objecttype() const { return fObjecttype; }
	unsigned fmtp_octetalign() const { return fOctetalign; }
	unsigned fmtp_profile_level_id() const { return fProfile_level_id; }
	unsigned fmtp_robustsorting() const { return fRobustsorting; }
	unsigned fmtp_sizelength() const { return fSizelength; }
	unsigned fmtp_streamstateindication() const { return fStreamstateindication; }
	unsigned fmtp_streamtype() const { return fStreamtype; }
	bool fmtp_cpresent() const { return fCpresent; }
	bool fmtp_randomaccessindication() const { return fRandomaccessindication; }
	char const* fmtp_config() const { return fConfig; }
	char const* fmtp_mode() const { return fMode; }
	char const* fmtp_spropparametersets() const { return fSpropParameterSets; }

	unsigned int connectionEndpointAddress() const; // Converts "fConnectionEndpointName" to an address (or 0 if unknown)
	void setDestinations(unsigned int defaultDestAddress);

	// Public fields that external callers can use to keep state.
	// (They are responsible for all storage management on these fields)
	char const* sessionId; // used by RTSP
	unsigned short serverPortNum; // in host byte order (used by RTSP)
	unsigned char rtpChannelId, rtcpChannelId; // used by RTSP (for RTP/TCP)

	// Parameters set from a RTSP "RTP-Info:" header:
	struct {
		unsigned short seqNum;
		unsigned int timestamp;
		bool infoIsNew; // not part of the RTSP header; instead, set whenever this struct is filled in
	} rtpInfo;

	double getNormalPlayTime(struct timeval const& presentationTime);
	// Computes the stream's "Normal Play Time" (NPT) from the given "presentationTime".
	// (For the definition of "Normal Play Time", see RFC 2326, section 3.6.)
	// This function is useful only if the "rtpInfo" structure was previously filled in
	// (e.g., by a "RTP-Info:" header in a RTSP response).
	// Also, for this function to work properly, the RTP stream's presentation times must (eventually) be
	// synchronized via RTCP.

	RTPSource	*fRTPSource;

protected:
	friend class MediaSession;
	friend class MediaSubsessionIterator;
	MediaSubsession(MediaSession& parent);
	virtual ~MediaSubsession();

	void setNext(MediaSubsession* next) { fNext = next; }

	bool parseSDPLine_c(char const* sdpLine);
	bool parseSDPAttribute_rtpmap(char const* sdpLine);
	bool parseSDPAttribute_control(char const* sdpLine);
	bool parseSDPAttribute_range(char const* sdpLine);
	bool parseSDPAttribute_fmtp(char const* sdpLine);
	bool parseSDPAttribute_source_filter(char const* sdpLine);
	bool parseSDPAttribute_x_dimensions(char const* sdpLine);
	bool parseSDPAttribute_framesize(char const* sdpLine);  
	bool parseSDPAttribute_framerate(char const* sdpLine);

protected:
	// Linkage fields:
	MediaSession& fParent;
	MediaSubsession* fNext;

	// Fields set from a SDP description:
	char* fConnectionEndpointName; // may also be set by RTSP SETUP response
	unsigned short fClientPortNum; // in host byte order
	// This field is also set by initiate()
	unsigned char fRTPPayloadFormat;
	char* fSavedSDPLines;
	char* fMediumName;
	char* fCodecName;
	char* fProtocolName;
	unsigned fRTPTimestampFrequency;
	char* fControlPath; // holds optional a=control: string
	struct in_addr fSourceFilterAddr; // used for SSM

	// Parameters set by "a=fmtp:" SDP lines:
	unsigned fAuxiliarydatasizelength, fConstantduration, fConstantsize;
	unsigned fCRC, fCtsdeltalength, fDe_interleavebuffersize, fDtsdeltalength;
	unsigned fIndexdeltalength, fIndexlength, fInterleaving;
	unsigned fMaxdisplacement, fObjecttype;
	unsigned fOctetalign, fProfile_level_id, fRobustsorting;
	unsigned fSizelength, fStreamstateindication, fStreamtype;
	bool fCpresent, fRandomaccessindication;
	char *fConfig, *fMode, *fSpropParameterSets;

	double fPlayStartTime;
	double fPlayEndTime;
	unsigned short fVideoWidth, fVideoHeight;
	// screen dimensions (set by an optional a=x-dimensions: <w>,<h> line)
	unsigned fVideoFPS;
	// frame rate (set by an optional "a=framerate: <fps>" or "a=x-framerate: <fps>" line)
	unsigned fNumChannels;
	// optionally set by "a=rtpmap:" lines for audio sessions.  Default: 1
	float fScale; // set from a RTSP "Scale:" header
	double fNPT_PTS_Offset; // set by "getNormalPlayTime()"; add this to a PTS to get NPT	
};

#endif
