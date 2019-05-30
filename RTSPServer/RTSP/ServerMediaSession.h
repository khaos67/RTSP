#ifndef __SERVER_MEDIA_SESSION_H__
#define __SERVER_MEDIA_SESSION_H__

#include "ClientSocket.h"
#include "MyList.h"

#define STREAM_INFO			"DXMediaPlayer"
#define STREAM_DESCRIPTION	"Session streamed by \"DXMediaPlayer\""

typedef enum {
	STREAM_STATE_STOPPED	= 0,
	STREAM_STATE_OPENED		= 1,
	STREAM_STATE_PAUSED		= 2,
	STREAM_STATE_RUNNING	= 3
} STREAM_STATE;

typedef int (*StartStreamCallback)(void *arg, double start_time);
typedef void (*StopStreamCallback)(void *arg);
typedef void (*ControlStreamCallback0)(void *arg);
typedef void (*ControlStreamCallback1)(void *arg, int arg1);
typedef void (*ControlStreamCallback2)(void *arg, float arg1);

class StreamControl
{
public:
	StreamControl(void *arg, StartStreamCallback startFunc, StopStreamCallback stopFunc, 
		ControlStreamCallback0 pauseFunc = NULL, ControlStreamCallback0 resumeFunc = NULL,
		ControlStreamCallback1 playDirFunc = NULL,
		ControlStreamCallback1 playNextFunc = NULL, ControlStreamCallback0 playContinueFunc = NULL,
		ControlStreamCallback1 seekFunc = NULL,
		ControlStreamCallback2 speedFunc = NULL);
	virtual ~StreamControl();

	int startStream(double start_time);
	void stopStream();
	void pauseStream();
	void resumeStream();
	void playDirStream(int dir);
	void playNextStream(int dir);
	void playContinueStream();
	void seekStream(int timestamp);
	void speedStream(float speed);

	STREAM_STATE state() { return fStreamState; }

protected:
	void*				fThis;
	StartStreamCallback	fStartStreamFunc;
	StopStreamCallback	fStopStreamFunc;
	ControlStreamCallback0	fPauseStreamFunc;
	ControlStreamCallback0	fResumeStreamFunc;
	ControlStreamCallback1	fPlayDirStreamFunc;
	ControlStreamCallback1	fPlayNextStreamFunc;
	ControlStreamCallback0	fPlayContinueStreamFunc;
	ControlStreamCallback1	fSeekStreamFunc;
	ControlStreamCallback2	fSpeedStreamFunc;

	STREAM_STATE	fStreamState;
};

typedef enum {
	SESSION_LIVE,
	SESSION_ONDEMAND
} SESSION_TYPE;

class ServerMediaSubsession;

class ServerMediaSession
{
public:
	ServerMediaSession(
		char const* streamName, 
		char const* info, 
		char const* description,
		bool isSSM,
		char const* miscSDPLines,
		StreamControl* streamControl);	

	virtual ~ServerMediaSession();

	char* generateSDPDescription();

	char const* streamName() const { return fStreamName; }

	bool addSubsession(ServerMediaSubsession* subsession);
	unsigned numSubsessions() const { return fSubsessionCounter; }

	float duration() const;
	// a result == 0 means an unbounded session (the default)
	// a result < 0 means: subsession durations differ; the result is -(the largest).
	// a result > 0 means: this is the duration of a bounded session

	unsigned referenceCount() const { return fReferenceCount; }
	void incrementReferenceCount() { ++fReferenceCount; }
	void decrementReferenceCount() { if (fReferenceCount > 0) --fReferenceCount; }
	bool& deleteWhenUnreferenced() { return fDeleteWhenUnreferenced; }

	void deleteAllSubsessions();

	// stream control
	int startStream(double start_time);
	void stopStream();
	void pauseStream();
	void resumeStream();
	void forwardStream();
	void backwardStream();
	void forwardNextStream();
	void backwardNextStream();
	void playContinueStream();
	void seekStream(int timestamp);
	void speedStream(float speed);
	STREAM_STATE streamState();
	void closeStreamControl();
	
	void removeClientSocket(ClientSocket *sock);
	int sendClientRtp(const char *trackId, char *buf, int len);
	int sendClientRtcp(const char *trackId, char *buf, int len);

	SESSION_TYPE sessionType() { return fSessionType; }

protected:
	SESSION_TYPE	fSessionType;

private:
	bool	fIsSSM;

	friend class ServerMediaSubsessionIterator;
	ServerMediaSubsession*	fSubsessionsHead;
	ServerMediaSubsession*	fSubsessionsTail;
	unsigned	fSubsessionCounter;

	char*	fStreamName;
	char*	fInfoSDPString;
	char*	fDescriptionSDPString;
	char*	fMiscSDPLines;
	struct timeval	fCreationTime;
	unsigned	fReferenceCount;
	bool		fDeleteWhenUnreferenced;

	StreamControl*	fStreamControl;	
	MUTEX			fMutex;
};

class ServerMediaSubsessionIterator 
{
public:
	ServerMediaSubsessionIterator(ServerMediaSession& session);
	virtual ~ServerMediaSubsessionIterator();

	ServerMediaSubsession* next(); // NULL if none
	void reset();

private:
	ServerMediaSession& fOurSession;
	ServerMediaSubsession* fNextPtr;
};

class ServerMediaSubsession
{
public:
	unsigned trackNumber() const { return fTrackNumber; }
	char const* trackId();

	virtual void getStreamParameters(unsigned clientSessionId, // in
		unsigned int clientAddress, // in
		unsigned short const& clientRTPPort, // in
		unsigned short const& clientRTCPPort, // in
		int tcpSocketNum, // in (-1 means use UDP, not TCP)
		unsigned char rtpChannelId, // in (used if TCP)
		unsigned char rtcpChannelId, // in (used if TCP)
		unsigned int& destinationAddress, // in out
		unsigned char& destinationTTL, // in out
		bool& isMulticast, // out
		unsigned short& serverRTPPort, // out
		unsigned short& serverRTCPPort // out
		);

	virtual float getCurrentNPT();
	virtual float duration() const;

	void addClientSock(ClientSocket *sock);
	bool removeClientSock(ClientSocket *sock);

	char const* codecName() { return fCodecName; }
	unsigned char rtpPayloadType() { return fRTPPayloadType; }
	unsigned timestampFrequency() { return fTimestampFrequency; }

protected:
	ServerMediaSubsession(char const* trackId, char const* codec, unsigned char rtpPayload, unsigned timestampFreq);
	virtual ~ServerMediaSubsession();

	char const* sdpLines();

	int sendClientRtp(char *buf, int len);
	int sendClientRtcp(char *buf, int len);

	ServerMediaSession*	fParentSession;

	MyList<ClientSocket>	fClientSockList;

private:
	friend class ServerMediaSession;
	friend class ServerMediaSubsessionIterator;
	ServerMediaSubsession* fNext;

	unsigned fTrackNumber;
	char const* fTrackId;

protected:
	char*			fSDPLines;
	const char*		fCodecName;
	unsigned char	fRTPPayloadType;
	unsigned		fTimestampFrequency;	
};

#endif
