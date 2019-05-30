#ifndef __RTSP_CLIENT_H__
#define __RTSP_CLIENT_H__

#include "MySock.h"
#include "TaskScheduler.h"
#include "MediaSession.h"
#include "DigestAuthentication.hh"

#define RECV_BUF_SIZE			(1024*1024)
#define SEND_GET_PARAM_DURATION	(50)

typedef void (*OnCloseFunc)(void *arg, int err, int result);
typedef void (*OnPacketReceiveFunc)(void *arg, const char *trackId, char *buf, int len);

class RTPSource;

class RTSPClient
{
public:
	RTSPClient();
	virtual ~RTSPClient();

	int openURL(const char *url, int streamType, int timeout = 2, bool rtpOnly = false);
	int playURL(FrameHandlerFunc func, void *funcData, 
		OnCloseFunc onCloseFunc, void *onCloseFuncData,
		OnPacketReceiveFunc onRTPReceiveFunc = NULL, void *onRTPReceiveFuncData = NULL,
		OnPacketReceiveFunc onRTCPReceiveFunc = NULL, void *onRTCPReceiveFuncData = NULL);
	void closeURL();
	void sendGetParam();
	int sendPause();
	int sendPlay(double start = 0.0f, double end = -1.0f, float scale = 1.0f);
	int sendSetParam(char *name, char *value);

public:
	const char* videoCodec() { return fVideoCodec; }
	const char* audioCodec() { return fAudioCodec; }
	int audioChannel() { return fChannel; }
	int audioSampleRate() { return fAudioSampleRate; }
	int videoWidth() { return fVideoWidth; }
	int videoHeight() { return fVideoHeight; }
	int videoFPS() { return fVideoFps; }

	const uint8_t* videoExtraData() { return fVideoExtraData; }
	unsigned videoExtraDataSize() { return fVideoExtraDataSize; }
	const uint8_t* audioExtraData() { return fAudioExtraData; }
	unsigned audioExtraDataSize() { return fAudioExtraDataSize; }

	double playStartTime() { return fPlayStartTime; }
	double playEndTime() { return fPlayEndTime; }

	MediaSession& mediaSession() { return *fMediaSession; }	// for rtsp server

	unsigned lastResponseCode() { return fLastResponseCode; }
	
protected:
	char* sendOptionsCmd(const char *url, 
		char *username = NULL, char *password = NULL,
		Authenticator *authenticator = NULL);

	char* describeURL(const char *url, 
		Authenticator* authenticator = NULL, bool allowKasennaProtocol = false);

	char* describeWithPassword(char const* url,
		char const* username, char const* password,
		bool allowKasennaProtocol = false);

	bool setupMediaSubsession(MediaSubsession& subsession, 
		bool streamOutgoing = false,
		bool streamUsingTCP = false,
		bool forceMulticastOnUnspecified = false);

	bool playMediaSession(MediaSession& session, bool response,
		double start = 0.0f, double end = -1.0f, float scale = 1.0f);

	bool pauseMediaSession(MediaSession& session);

	bool setMediaSessionParameter(MediaSession& session,
		char const* parameterName,
		char const* parameterValue);

	bool getMediaSessionParameter(MediaSession& session,
		char const* parameterName,
		char*& parameterValue);

	bool teardownMediaSession(MediaSession& session);

protected:
	int connectToServer(char const *ip_addr, unsigned short port, int timeout);
	int sendRequest(char *str, char *tag);
	bool getResponse(char const* tag,
		unsigned& bytesRead, unsigned& responseCode,
		char*& firstLine, char*& nextLineStart,
		bool checkFor200Response = true);
	unsigned getResponse1(char*& responseBuffer, unsigned responseBufferSize);
	bool parseResponseCode(char const* line, unsigned& responseCode);
	bool parseTransportResponse(char const* line,
		char*& serverAddressStr,
		unsigned short& serverPortNum,
		unsigned char& rtpChannelId,
		unsigned char& rtcpChannelId);

	static void tcpReadHandler(void *instance, int);
	void tcpReadHandler1();
	void tcpReadError(int result);

	bool lookupStreamChannelId(unsigned char channel);
	bool readRTSPMessage();
	void readRTPOverTCP();

	bool parseRTSPMessage();
	void handleCmd_notSupported(char const* cseq);
	
	char const* sessionURL(MediaSession const& session) const;
	void constructSubsessionURL(MediaSubsession const& subsession,
		char const*& prefix,
		char const*& separator,
		char const*& suffix);

	bool parseRangeHeader(char const* buf, double& rangeStart, double& rangeEnd);
	bool parseRTPInfoHeader(char*& line, unsigned short& seqNum, unsigned int& timestamp);

	char* createAuthenticatorString(Authenticator const* authenticator, char const* cmd, char const* url);
	static void checkForAuthenticationFailure(unsigned responseCode, char*& nextLineStart, Authenticator* authenticator);

	void reset();
	void resetResponseBuffer();

protected:	
	static void rtpHandlerCallback(void *arg, char *trackId, char *buf, int len);
	static void rtcpHandlerCallback(void *arg, char *trackId, char *buf, int len);

protected:
	enum { AWAITING_DOLLAR, AWAITING_STREAM_CHANNEL_ID, AWAITING_SIZE1, AWAITING_SIZE2, AWAITING_PACKET_DATA,
	AWAITING_RTSP_MESSAGE } fTCPReadingState;
	unsigned char	fStreamChannelId, fSizeByte1;
	unsigned		fTCPReadSize;
	RTPSource*		fNextTCPSource;
	int				fNextTCPSourceType;

protected:
	MySock			fRtspSock;
	TaskScheduler*	fTask;
	MediaSession*	fMediaSession;

	int				m_nTimeoutSecond;

	char*			fResponseBuffer;
	int				fResponseBufferSize;
	int				fResponseBufferIdx;

	char*			fRtpBuffer;
	int				fRtpBufferSize;
	int				fRtpBufferIdx;

	char*			fUserAgentHeaderStr;
	unsigned		fUserAgentHeaderStrSize;
	char*			fBaseURL;
	unsigned		fCSeq;

	unsigned char	fTCPStreamIdCount;

	char*			fLastSessionId;
	char*			fLastSessionIdStr;
	unsigned		fSessionTimeoutParameter;

	Authenticator	fCurrentAuthenticator;

	unsigned		fLastResponseCode;

	OnCloseFunc		fCloseFunc;
	void*			fCloseFuncData;

	const char*		fVideoCodec;
	const char*		fAudioCodec;
	int				fVideoWidth;
	int				fVideoHeight;
	int				fVideoFps;
	int				fChannel;
	int				fAudioSampleRate;
	uint8_t*		fVideoExtraData;
	unsigned		fVideoExtraDataSize;
	uint8_t*		fAudioExtraData;
	unsigned		fAudioExtraDataSize;
	double			fPlayStartTime;
	double			fPlayEndTime;

	bool			fIsSendGetParam;
	time_t			fLastSendGetParam;	// GET_PARAMETER polling time

	// for rtsp server
	OnPacketReceiveFunc	fRTPReceiveFunc;
	void*				fRTPReceiveFuncData;
	OnPacketReceiveFunc	fRTCPReceiveFunc;
	void*				fRTCPReceiveFuncData;
};

#endif
