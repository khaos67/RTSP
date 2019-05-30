#ifndef __RTSP_LIVE_STREAMER_H__
#define __RTSP_LIVE_STREAMER_H__

#include "RTSPClient.h"
#include "ServerMediaSession.h"
#include "RTSPServer.h"

typedef enum {
	STREAMER_STATE_STOPPED	= 0,
	STREAMER_STATE_OPENED	= 1,
	STREAMER_STATE_PAUSED	= 2,
	STREAMER_STATE_RUNNING	= 3
} STREAMER_STATE;

class RTSPLiveStreamer
{
public:
	RTSPLiveStreamer();
	virtual ~RTSPLiveStreamer();

	STREAMER_STATE state() { return m_nState; }

	int open(const char *url, int stream_type, const char *sessionName);
	int run();
	void close();

protected:
	static void onRtpReceived(void *arg, const char *trackId, char *buf, int len);
	void onRtpReceived1(const char *trackId, char *buf, int len);

	static void onRtcpReceived(void *arg, const char *trackId, char *buf, int len);
	void onRtcpReceived1(const char *trackId, char *buf, int len);

protected:
	char* checkControlPath(const char *controlPath);
	char* updateSdpLines(const char *sdpLines, const char *orgControlPath, const char *newControlPath);

protected:
	RTSPClient*		m_pRtspClient;
	STREAMER_STATE	m_nState;

	ServerMediaSession*	m_pServerSession;
	RTSPServer*			m_pRtspServer;
	char*				m_pSessionName;
};

#endif
