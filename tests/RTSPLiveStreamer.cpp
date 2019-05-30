#include "RTSPLiveStreamer.h"
#include "RTSPCommonEnv.h"
#include "LiveServerMediaSession.h"

RTSPLiveStreamer::RTSPLiveStreamer() : m_pServerSession(NULL), m_pSessionName(NULL)
{
	m_pRtspClient = new RTSPClient();
	m_pRtspServer = RTSPServer::instance();
	m_nState = STREAMER_STATE_STOPPED;
}

RTSPLiveStreamer::~RTSPLiveStreamer()
{
	delete[] m_pSessionName;
	delete m_pRtspClient;
}

int RTSPLiveStreamer::open(const char *url, int stream_type, const char *sessionName)
{
	if (state() != STREAMER_STATE_STOPPED)
		return -1;

	if (m_pRtspServer->lookupServerMediaSession(sessionName) != NULL) {
		DPRINTF("failed to open server session, session %s already exists\n", sessionName);
		return -1;
	}

	int ret = m_pRtspClient->openURL(url, stream_type, 2, true);
	if (ret < 0) {
		DPRINTF("%s open failed\n", url);
		return ret;
	}

	delete[] m_pSessionName;
	m_pSessionName = strDup(sessionName);

	m_nState = STREAMER_STATE_OPENED;

	return 0;
}

int RTSPLiveStreamer::run()
{
	if (state() != STREAMER_STATE_OPENED)
		return -1;

	if (m_pRtspClient->playURL(NULL, NULL, NULL, NULL, onRtpReceived, this, onRtcpReceived, this) < 0)
		return -1;

	m_pServerSession = new LiveServerMediaSession(m_pSessionName, "DXMediaPlayer", "Session streamed by \"DXMediaPlayer\"", false, NULL);

	MediaSubsessionIterator *iter = new MediaSubsessionIterator(m_pRtspClient->mediaSession());
	MediaSubsession *subsession = NULL;
	while ((subsession=iter->next()) != NULL) {
		char *sdpLines;
		char *controlPath = checkControlPath(subsession->controlPath());

		if (strcmp(controlPath, subsession->controlPath()) == 0)
			sdpLines = strDup(subsession->savedSDPLines());
		else
			sdpLines = updateSdpLines(subsession->savedSDPLines(), subsession->controlPath(), controlPath);

		m_pServerSession->addSubsession(
			new LiveServerMediaSubsession(
				controlPath,
				sdpLines, 
				subsession->codecName(),
				subsession->rtpPayloadFormat(), 
				subsession->rtpTimestampFrequency())
				);

		if (controlPath) delete[] controlPath;
		if (sdpLines) delete[] sdpLines;
	}

	delete iter;

	m_pRtspServer->addServerMediaSession(m_pServerSession);

	m_nState = STREAMER_STATE_RUNNING;

	return 0;
}

void RTSPLiveStreamer::close()
{
	m_pRtspClient->closeURL();
	m_pRtspServer->deleteServerMediaSession(m_pServerSession);
	m_pServerSession = NULL;
	delete[] m_pSessionName; m_pSessionName = NULL;
	m_nState = STREAMER_STATE_STOPPED;
}

void RTSPLiveStreamer::onRtpReceived(void *arg, const char *trackId, char *buf, int len)
{
	RTSPLiveStreamer *streamer = (RTSPLiveStreamer *)arg;
	streamer->onRtpReceived1(trackId, buf, len);
}

void RTSPLiveStreamer::onRtpReceived1(const char *trackId, char *buf, int len)
{
	if (m_pServerSession)
		m_pServerSession->sendClientRtp(trackId, buf, len);
}

void RTSPLiveStreamer::onRtcpReceived(void *arg, const char *trackId, char *buf, int len)
{
	RTSPLiveStreamer *streamer = (RTSPLiveStreamer *)arg;
	streamer->onRtcpReceived1(trackId, buf, len);
}

void RTSPLiveStreamer::onRtcpReceived1(const char *trackId, char *buf, int len)
{
	if (m_pServerSession)
		m_pServerSession->sendClientRtcp(trackId, buf, len);
}

char* RTSPLiveStreamer::checkControlPath(const char *controlPath)
{
	const char *ptr = &controlPath[strlen(controlPath)-1];
	while (ptr >= controlPath) {
		if (*ptr == '/')
			break;
		ptr--;
	}

	char *newControlPath;

	if (*ptr == '/')
		newControlPath = strDup(ptr+1);
	else
		newControlPath = strDup(controlPath);

	return newControlPath;
}

char* RTSPLiveStreamer::updateSdpLines(const char *sdpLines, const char *orgControlPath, const char *newControlPath)
{
	char* sdpLinesDup = strDup(sdpLines);

	int len = strlen(sdpLines) + strlen(newControlPath);
	char *newSdpLines = new char[len];
	memset(newSdpLines, 0, len);
	char *nextLineStart = sdpLinesDup;
	char *lineStart;

	while (1) {
		lineStart = nextLineStart;
		if (lineStart == NULL) break;

		nextLineStart = getLine(lineStart);

		if (strstr(lineStart, orgControlPath)) {
			char tmp[256] = {0};
			sprintf(tmp, "a=control:%s\r\n", newControlPath);
			strcat(newSdpLines, tmp);
		} else {
			strcat(newSdpLines, lineStart);
			strcat(newSdpLines, "\r\n");
		}
	}

	delete[] sdpLinesDup;
	return newSdpLines;
}
