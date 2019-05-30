#ifndef __RTSP_SERVER_H__
#define __RTSP_SERVER_H__

#include "MySock.h"
#include "TaskScheduler.h"
#include "MyList.h"
#include "RTSPCommon.h"

#define RTSP_BUFFER_SIZE	(20000)

typedef enum { OPEN_SERVER_SESSION, CLIENT_CONNECTED, CLIENT_DISCONNECTED } ServerCallbackType;

class CallbackParam {
public:
	CallbackParam(ServerCallbackType type);
	virtual ~CallbackParam() {}

public:
	ServerCallbackType	fType;
};

class OpenServerSessionParam : public CallbackParam {
public:
	OpenServerSessionParam(char *requestString);
	virtual ~OpenServerSessionParam() {}

public:
	char	fRequestString[256];
};

class ClientConnectedParam : public CallbackParam {
public:
	ClientConnectedParam(const char *sessionName, int sock, struct sockaddr_in &sockAddr, int streamType);
	virtual ~ClientConnectedParam() {}

public:
	char	fSessionName[256];
	int		fSock;
	struct sockaddr_in	fClientAddr;
	int		fStreamType;
};

class ClientDisconnectedParam : public CallbackParam {
public:
	ClientDisconnectedParam(int sock);
	virtual ~ClientDisconnectedParam() {}

public:
	int fSock;
};

typedef int (*RTSPServerCallback)(void *arg, CallbackParam *param);

class ServerMediaSession;
class ServerMediaSubsession;
class ClientSocket;

class RTSPServer
{
public:
	static RTSPServer* instance();
	static void destroy();

	int startServer(unsigned short port = 554, RTSPServerCallback func = NULL, void *arg = NULL);
	void stopServer();
	bool isServerRunning() { return fIsServerRunning; }
	int serverSessionCount() { return fServerMediaSessions.count(); }

	void addServerMediaSession(ServerMediaSession* serverMediaSession);
	ServerMediaSession* lookupServerMediaSession(char const* streamName);
	void removeServerMediaSession(ServerMediaSession* serverMediaSession);

	void closeAllClientSessionsForServerMediaSession(ServerMediaSession* serverMediaSession);
	void deleteServerMediaSession(ServerMediaSession* serverMediaSession);

	char* rtspURL(ServerMediaSession const* serverMediaSession, int clientSocket = -1);
	// returns a "rtsp://" URL that could be used to access the
	// specified session (which must already have been added to
	// us using "addServerMediaSession()".
	// This string is dynamically allocated; caller should delete[]
	// (If "clientSocket" is non-negative, then it is used (by calling "getsockname()") to determine
	//  the IP address to be used in the URL.)
	char* rtspURLPrefix(int clientSocket = -1);
	// like "rtspURL()", except that it returns just the common prefix used by
	// each session's "rtsp://" URL.
	// This string is dynamically allocated; caller should delete[]

protected:
	RTSPServer();
	virtual ~RTSPServer();

	static RTSPServer* fInstance;

	virtual char const* allowedCommandNames(); // used to implement "RTSPClientConnection::handleCmd_OPTIONS()"

protected:
	static void incomingConnectionHandlerRTSP(void*, int);
	void incomingConnectionHandlerRTSP1();
	void incomingConnectionHandler(int serverSocket);

protected:	
	class RTSPClientSession
	{
	public:
		RTSPClientSession(RTSPServer& ourServer, MySock& clientSock, unsigned int sessionId);
		virtual ~RTSPClientSession();

		friend class RTSPServer;

	protected:
		static void incomingRequestHandler(void*, int);
		void incomingRequestHandler1(unsigned char requestByte);
		void handleRequestBytes(int newBytesRead);

		void resetRequestBuffer();

		void handleCmd_OPTIONS();
		void handleCmd_DESCRIBE(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr);
		void handleCmd_SETUP(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr);
		void handleCmd_PLAY(ServerMediaSubsession* subsession, char const* fullRequestStr);
		void handleCmd_TEARDOWN(ServerMediaSubsession* subsession);
		void handleCmd_PAUSE(ServerMediaSubsession* subsession);
		void handleCmd_GET_PARAMETER(ServerMediaSubsession* subsession, char const* fullRequestStr);
		void handleCmd_SET_PARAMETER(ServerMediaSubsession* subsession, char const* fullRequestStr);
		void handleCmd_withinSession(char const* cmdName, char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr);
		void handleCmd_bad();
		void handleCmd_notSupported();
		void handleCmd_notFound();
		void handleCmd_sessionNotFound();
		void handleCmd_unsupportedTransport();

		void reclaimStreamStates();
		void shutdown();

		// Shortcuts for setting up a RTSP response (prior to sending it):
		void setRTSPResponse(char const* responseStr);
		void setRTSPResponse(char const* responseStr, unsigned int sessionId);
		void setRTSPResponse(char const* responseStr, char const* contentStr);
		void setRTSPResponse(char const* responseStr, u_int32_t sessionId, char const* contentStr);
		
		// tcp stream read fuctions
		void tcpReadHandler1();
		bool lookupStreamChannelId(unsigned char channel);	
		void readRTPOverTCP();

	protected:
		enum { AWAITING_DOLLAR, AWAITING_STREAM_CHANNEL_ID, AWAITING_SIZE1, AWAITING_SIZE2, AWAITING_PACKET_DATA,
			 } fTCPReadingState;
		unsigned char	fStreamChannelId, fSizeByte1;
		unsigned		fTCPReadSize;

		char*			fRtpBuffer;
		int				fRtpBufferSize;
		int				fRtpBufferIdx;

	protected:
		RTSPServer&	fOurServer;
		u_int32_t	fOurSessionId;
		ServerMediaSession*	fOurServerMediaSession;
		bool			fIsActive;
		MySock*			fClientSock;
		unsigned char	fRequestBuffer[RTSP_BUFFER_SIZE];
		unsigned		fRequestBytesAlreadySeen, fRequestBufferBytesLeft;
		unsigned char*	fLastCRLF;
		unsigned char	fResponseBuffer[RTSP_BUFFER_SIZE];
		char const*		fCurrentCSeq;
		unsigned		fBase64RemainderCount;	// used for optional RTSP-over-HTTP tunneling (possible values: 0,1,2,3)

		bool			fIsMulticast;
		unsigned char	fTCPStreamIdCount;	// used for (optional) RTP/TCP

		unsigned		fNumStreamStates;
		struct streamState {
			ServerMediaSubsession* subsession;
		} * fStreamStates;

		MyList<ClientSocket>	fClientSockList;
	};

	RTSPClientSession* createNewClientSession(MySock& clientSock);
	void addClientSession(RTSPClientSession *clientSession);
	void removeClientSession(RTSPClientSession *clientSession);

protected:
	friend class RTSPClientSession;

protected:
	bool			fIsServerRunning;
	unsigned short	fServerPort;

	RTSPServerCallback	fServerCallbackFunc;
	void*				fServerCallbackArg;

	MySock			fServerSock;
	TaskScheduler*	fTask;

	MyList<ServerMediaSession>	fServerMediaSessions;
	MyList<RTSPClientSession>	fClientSessions;
};

#endif
