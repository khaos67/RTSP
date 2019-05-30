#include "RTSPServer.h"
#include "RTSPCommonEnv.h"
#include "RTSPCommon.h"
#include "ServerMediaSession.h"
#include "ClientSocket.h"
#include "util.h"
#include "NetAddress.h"

#include <stdio.h>

RTSPServer* RTSPServer::fInstance = new RTSPServer();

RTSPServer* RTSPServer::instance()
{
	return fInstance;
}

void RTSPServer::destroy()
{
	if (fInstance) {
		delete fInstance;
		fInstance = NULL;
	}
}

char const* RTSPServer::allowedCommandNames() {
	return "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER";
}

RTSPServer::RTSPServer() : fIsServerRunning(false), fServerCallbackFunc(NULL)
{
	fTask = new TaskScheduler();
#ifdef WIN32
	srand(GetTickCount());
#else
	srand(clock());
#endif
}

RTSPServer::~RTSPServer()
{
	stopServer();
	DELETE_OBJECT(fTask);
}

int RTSPServer::startServer(unsigned short port, RTSPServerCallback func, void *arg)
{
	if (!fIsServerRunning) {
		fServerPort = port;

		if (fServerSock.setupServerSock(fServerPort, true) < 0) {
			DPRINTF("failed to start RTSP Server (%d)\n", fServerPort);
			return -9;
		}

		fServerSock.setSendBufferTo(1024*50);
		
		fTask->turnOnBackgroundReadHandling(fServerSock.sock(), &incomingConnectionHandlerRTSP, this);
		fTask->startEventLoop();

		DPRINTF("RTSP Server started, port: %d\n", fServerPort);

		fIsServerRunning = true;
	}

	fServerCallbackFunc = func;
	fServerCallbackArg = arg;

	return 0;
}

void RTSPServer::stopServer()
{	
	if (fIsServerRunning) {
		// do not allow more client connection
		fTask->turnOffBackgroundReadHandling(fServerSock.sock());
		fServerSock.closeSock();

		// wait until all client sessions are disconnected
		while (fServerMediaSessions.count() > 0 || fClientSessions.count() > 0) {
#ifdef WIN32
			Sleep(10);
#else
			usleep(10*1000);
#endif
		}

		// stop task loop
		fTask->stopEventLoop();
		fIsServerRunning = false;

		DPRINTF("RTSP Server stopped\n");
	}
}

void RTSPServer::addServerMediaSession(ServerMediaSession *serverMediaSession)
{
	if (serverMediaSession == NULL) return;

	removeServerMediaSession(serverMediaSession);

	fServerMediaSessions.lock();
	fServerMediaSessions.insert(serverMediaSession);
	fServerMediaSessions.unlock();

	DPRINTF("server session %s added, count : %d\n", serverMediaSession->streamName(), fServerMediaSessions.count());
}

void RTSPServer::removeServerMediaSession(ServerMediaSession *serverMediaSession)
{
	if (serverMediaSession == NULL) return;

	fServerMediaSessions.lock();

	fServerMediaSessions.gotoBeginCursor();
	ServerMediaSession *cursor = fServerMediaSessions.getCursor();
	while (cursor) {
		if (cursor == serverMediaSession) {
			DPRINTF("server session %s being removed\n", serverMediaSession->streamName());
			if (serverMediaSession->referenceCount() == 0) {
				fServerMediaSessions.remove();
			} else {
				serverMediaSession->deleteWhenUnreferenced() = true;
			}
			DPRINTF("server session count : %d\n", fServerMediaSessions.count());
			break;
		}
		fServerMediaSessions.getNext();
		cursor = fServerMediaSessions.getCursor();
	}

	fServerMediaSessions.unlock();
}

ServerMediaSession* RTSPServer::lookupServerMediaSession(const char *streamName)
{
	if (streamName == NULL) return NULL;

	ServerMediaSession *session = NULL;

	fServerMediaSessions.lock();

	fServerMediaSessions.gotoBeginCursor();
	ServerMediaSession *cursor = fServerMediaSessions.getCursor();
	while (cursor) {
		if (strcmp(cursor->streamName(), streamName) == 0) {
			session = cursor;
			break;
		}
		cursor = fServerMediaSessions.getNextCursor();
	}

	fServerMediaSessions.unlock();

	return session;
}

void RTSPServer::closeAllClientSessionsForServerMediaSession(ServerMediaSession *serverMediaSession)
{
	if (serverMediaSession == NULL) return;

	fClientSessions.lock();

	fClientSessions.gotoBeginCursor();
	RTSPClientSession *cursor = fClientSessions.getCursor();
	while (cursor) {
		if (cursor->fOurServerMediaSession == serverMediaSession)
			cursor->shutdown();
		
		fClientSessions.getNext();
		cursor = fClientSessions.getCursor();
	}

	fClientSessions.unlock();
}

void RTSPServer::deleteServerMediaSession(ServerMediaSession *serverMediaSession)
{
	if (serverMediaSession == NULL) return;

	closeAllClientSessionsForServerMediaSession(serverMediaSession);
	removeServerMediaSession(serverMediaSession);
}

char* RTSPServer::rtspURL(ServerMediaSession const* serverMediaSession, int clientSocket) 
{
	char* urlPrefix = rtspURLPrefix(clientSocket);
	char const* sessionName = serverMediaSession->streamName();

	char* resultURL = new char[strlen(urlPrefix) + strlen(sessionName) + 1];
	sprintf(resultURL, "%s%s", urlPrefix, sessionName);

	delete[] urlPrefix;
	return resultURL;
}

char* RTSPServer::rtspURLPrefix(int clientSocket) 
{
	struct sockaddr_in ourAddress;
	if (clientSocket < 0) {
		// Use our default IP address in the URL:
		ourAddress.sin_addr.s_addr = ReceivingInterfaceAddr != 0 ? ReceivingInterfaceAddr : ourIPAddress(); // hack
	} else {
		socklen_t namelen = sizeof ourAddress;
		getsockname(clientSocket, (struct sockaddr*)&ourAddress, &namelen);
	}

	char urlBuffer[100]; // more than big enough for "rtsp://<ip-address>:<port>/"

	unsigned short port = fServerSock.port();
	if (port == 554 /* the default port number */) {
		sprintf(urlBuffer, "rtsp://%s/", inet_ntoa(ourAddress.sin_addr));
	} else {
		sprintf(urlBuffer, "rtsp://%s:%hu/", inet_ntoa(ourAddress.sin_addr), port);
	}

	return strDup(urlBuffer);
}

void RTSPServer::incomingConnectionHandlerRTSP(void *instance, int)
{
	RTSPServer *server = (RTSPServer *)instance;
	server->incomingConnectionHandlerRTSP1();
}

void RTSPServer::incomingConnectionHandlerRTSP1()
{
	incomingConnectionHandler(fServerSock.sock());
}

void RTSPServer::incomingConnectionHandler(int serverSocket)
{
	MySock *clientSock = new MySock();
	if (clientSock->setupClientSock(serverSocket, 1) <= 0) {
		delete clientSock;
		return;
	}

	addClientSession(createNewClientSession(*clientSock));
}

RTSPServer::RTSPClientSession* RTSPServer::createNewClientSession(MySock &clientSock)
{
	return new RTSPClientSession(*this, clientSock, rand());
}

void RTSPServer::addClientSession(RTSPClientSession *clientSession)
{
	fClientSessions.lock();
	fClientSessions.insert(clientSession);

	//DPRINTF("client session %u added, count : %d\n", clientSession->fOurSessionId, fClientSessions.count());

	fClientSessions.unlock();
}

void RTSPServer::removeClientSession(RTSPClientSession *clientSession)
{
	fClientSessions.lock();

	fClientSessions.gotoBeginCursor();
	RTSPClientSession *cursor = fClientSessions.getCursor();
	while (cursor) {
		if (cursor == clientSession) {
			fClientSessions.deleteCursor();
			//DPRINTF("client session %u removed, count : %d\n", clientSession->fOurSessionId, fClientSessions.count());
			break;
		}
		fClientSessions.getNext();
		cursor = fClientSessions.getCursor();
	}

	fClientSessions.unlock();
}

RTSPServer::RTSPClientSession::RTSPClientSession(RTSPServer &ourServer, MySock &clientSock, unsigned int sessionId)
: fOurServer(ourServer), fOurSessionId(sessionId), fOurServerMediaSession(NULL), fClientSock(&clientSock), fIsActive(true)
, fIsMulticast(false), fTCPStreamIdCount(0)
, fNumStreamStates(0), fStreamStates(NULL)
{
	fTCPReadingState = AWAITING_DOLLAR;
	fRtpBuffer = new char[1024*1024];
	fRtpBufferIdx = 0;
	fRtpBufferSize = 1024*1024;

	resetRequestBuffer();
	fOurServer.fTask->turnOnBackgroundReadHandling(fClientSock->sock(), incomingRequestHandler, this);
}

RTSPServer::RTSPClientSession::~RTSPClientSession()
{
	fOurServer.removeClientSession(this);

	fOurServer.fTask->turnOffBackgroundReadHandling(fClientSock->sock());

	if (fOurServer.fServerCallbackFunc) {
		ClientDisconnectedParam *param = new ClientDisconnectedParam(fClientSock->sock());
		fOurServer.fServerCallbackFunc(fOurServer.fServerCallbackArg, param);
	}

	if (fClientSock)
		delete fClientSock;

	reclaimStreamStates();

	if (fOurServerMediaSession != NULL) {
		fOurServerMediaSession->decrementReferenceCount();
		if (fOurServerMediaSession->referenceCount() == 0 && fOurServerMediaSession->deleteWhenUnreferenced()) {
			fOurServer.removeServerMediaSession(fOurServerMediaSession);
			fOurServerMediaSession = NULL;
		}

		if (fOurServerMediaSession && fOurServerMediaSession->sessionType() == SESSION_ONDEMAND) {
			fOurServerMediaSession->stopStream();
		}
	}

	delete[] fRtpBuffer;
}

void RTSPServer::RTSPClientSession::reclaimStreamStates()
{
	delete[] fStreamStates; fStreamStates = NULL;
	fNumStreamStates = 0;

	fClientSockList.gotoBeginCursor();
	ClientSocket *cursor = fClientSockList.getNextCursor();
	while (cursor) {
		if (fOurServerMediaSession)
			fOurServerMediaSession->removeClientSocket(cursor);
		cursor = fClientSockList.getNextCursor();
	}

	fClientSockList.clearList();
}

void RTSPServer::RTSPClientSession::shutdown()
{
	fClientSock->shutdown();
}

void RTSPServer::RTSPClientSession::resetRequestBuffer()
{
	fRequestBytesAlreadySeen = 0;
	fRequestBufferBytesLeft = sizeof(fRequestBuffer);
	fLastCRLF = &fRequestBuffer[-3];
	fBase64RemainderCount = 0;
}

typedef enum StreamingMode {
	RTP_UDP,
	RTP_TCP,
	RAW_UDP
} StreamingMode;

static void parseTransportHeader(char const* buf,
								 StreamingMode& streamingMode,
								 char*& streamingModeString,
								 char*& destinationAddressStr,
								 u_int8_t& destinationTTL,
								 unsigned short& clientRTPPortNum, // if UDP
								 unsigned short& clientRTCPPortNum, // if UDP
								 unsigned char& rtpChannelId, // if TCP
								 unsigned char& rtcpChannelId // if TCP
								 ) 
{
	// Initialize the result parameters to default values:
	streamingMode = RTP_UDP;
	streamingModeString = NULL;
	destinationAddressStr = NULL;
	destinationTTL = 255;
	clientRTPPortNum = 0;
	clientRTCPPortNum = 1;
	rtpChannelId = rtcpChannelId = 0xFF;

	unsigned short p1, p2;
	unsigned ttl, rtpCid, rtcpCid;

	// First, find "Transport:"
	while (1) {
		if (*buf == '\0') return; // not found
		if (*buf == '\r' && *(buf+1) == '\n' && *(buf+2) == '\r') return; // end of the headers => not found
		if (_strcasecmp(buf, "Transport:", 10) == 0) break;
		++buf;
	}

	// Then, run through each of the fields, looking for ones we handle:
	char const* fields = buf + 10;
	while (*fields == ' ') ++fields;
	char* field = strDupSize(fields);
	while (sscanf(fields, "%[^;\r\n]", field) == 1) {
		if (strcmp(field, "RTP/AVP/TCP") == 0) {
			streamingMode = RTP_TCP;
		} else if (strcmp(field, "RAW/RAW/UDP") == 0 ||
			strcmp(field, "MP2T/H2221/UDP") == 0) {
				streamingMode = RAW_UDP;
				streamingModeString = strDup(field);
		} else if (_strcasecmp(field, "destination=", 12) == 0) {
			delete[] destinationAddressStr;
			destinationAddressStr = strDup(field+12);
		} else if (sscanf(field, "ttl%u", &ttl) == 1) {
			destinationTTL = (u_int8_t)ttl;
		} else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) {
			clientRTPPortNum = p1;
			clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p2; // ignore the second port number if the client asked for raw UDP
		} else if (sscanf(field, "client_port=%hu", &p1) == 1) {
			clientRTPPortNum = p1;
			clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p1 + 1;
		} else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
			rtpChannelId = (unsigned char)rtpCid;
			rtcpChannelId = (unsigned char)rtcpCid;
		}

		fields += strlen(field);
		while (*fields == ';' || *fields == ' ' || *fields == '\t') ++fields; // skip over separating ';' chars or whitespace
		if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
	}
	delete[] field;
}

void RTSPServer::RTSPClientSession::incomingRequestHandler(void *instance, int)
{
	RTSPClientSession *session = (RTSPClientSession *)instance;
	session->tcpReadHandler1();
}

void RTSPServer::RTSPClientSession::incomingRequestHandler1(unsigned char requestByte)
{
	if (fRequestBufferBytesLeft == 0 || fRequestBytesAlreadySeen >= RTSP_BUFFER_SIZE)
		return;

	fRequestBuffer[fRequestBytesAlreadySeen] = requestByte;
	handleRequestBytes(1);

	struct sockaddr_in dummy;
	int bytesRead = fClientSock->readSocket1((char *)&fRequestBuffer[fRequestBytesAlreadySeen], fRequestBufferBytesLeft, dummy);
	handleRequestBytes(bytesRead);
}

void RTSPServer::RTSPClientSession::handleRequestBytes(int newBytesRead)
{
	int numBytesRemaining = 0;

	do
	{
		if (newBytesRead <= 0 || (unsigned)newBytesRead >= fRequestBufferBytesLeft) {
			// Either the client socket has died, or the request was too big for us.
			// Terminate this connection:
			DPRINTF("RTSPClientConnection[%p]::handleRequestBytes() read %d new bytes (of %d); terminating connection!\n", this, newBytesRead, fRequestBufferBytesLeft);
			fIsActive = false;
			break;
		}

		bool endOfMsg = false;
		unsigned char *ptr = &fRequestBuffer[fRequestBytesAlreadySeen];

		if (RTSPCommonEnv::nDebugFlag&DEBUG_FLAG_RTSP) {
			ptr[newBytesRead] = '\0';
			DPRINTF("RTSPClientConnection[%p]::handleRequestBytes() %s %d new bytes:\n%s\n",
				this, numBytesRemaining > 0 ? "processing" : "read", newBytesRead, ptr);
		}

		unsigned char *tmpPtr = fLastCRLF + 2;
		if (fBase64RemainderCount == 0) { // no more Base-64 bytes remain to be read/decoded
			// Look for the end of the message: <CR><LF><CR><LF>
			if (tmpPtr < fRequestBuffer) tmpPtr = fRequestBuffer;
			while (tmpPtr < &ptr[newBytesRead-1]) {
				if (*tmpPtr == '\r' && *(tmpPtr+1) == '\n') {
					if (tmpPtr - fLastCRLF == 2) { // This is it:
						endOfMsg = true;
						break;
					}
					fLastCRLF = tmpPtr;
				}
				++tmpPtr;
			}
		}

		fRequestBufferBytesLeft -= newBytesRead;
		fRequestBytesAlreadySeen += newBytesRead;

		if (!endOfMsg) break; // subsequent reads will be needed to complete the request

		// Parse the request string into command name and 'CSeq', then handle the command:
		fRequestBuffer[fRequestBytesAlreadySeen] = '\0';
		char cmdName[RTSP_PARAM_STRING_MAX];
		char urlPreSuffix[RTSP_PARAM_STRING_MAX];
		char urlSuffix[RTSP_PARAM_STRING_MAX];
		char cseq[RTSP_PARAM_STRING_MAX];
		char sessionIdStr[RTSP_PARAM_STRING_MAX];
		unsigned contentLength = 0;

		fLastCRLF[2] = '\0'; // temporarily, for parsing

		bool parseSucceeded = parseRTSPRequestString((char*)fRequestBuffer, fLastCRLF+2 - fRequestBuffer,
			cmdName, sizeof cmdName,
			urlPreSuffix, sizeof urlPreSuffix,
			urlSuffix, sizeof urlSuffix,
			cseq, sizeof cseq,
			sessionIdStr, sizeof sessionIdStr,
			contentLength);

		fLastCRLF[2] = '\r'; // restore its value
		bool playAfterSetup = false;

		if (parseSucceeded) {
#ifdef _DEBUG
			//DPRINTF("parseRTSPRequestString() succeeded, returning cmdName \"%s\", urlPreSuffix \"%s\", urlSuffix \"%s\", CSeq \"%s\", Content-Length %u, with %ld bytes following the message.\n", cmdName, urlPreSuffix, urlSuffix, cseq, contentLength, ptr + newBytesRead - (tmpPtr + 2));
#endif
			// If there was a "Content-Length:" header, then make sure we've received all of the data that it specified:
			if (ptr + newBytesRead < tmpPtr + 2 + contentLength) 
				break; // we still need more data; subsequent reads will give it to us

			// We now have a complete RTSP request.
			// Handle the specified command (beginning with commands that are session-independent):
			fCurrentCSeq = cseq;
			if (strcmp(cmdName, "OPTIONS") == 0) {
				handleCmd_OPTIONS();
			} else if (urlPreSuffix[0] == '\0' && urlSuffix[0] == '*' && urlSuffix[1] == '\0') {
				// The special "*" URL means: an operation on the entire server.  This works only for GET_PARAMETER and SET_PARAMETER:
				if (strcmp(cmdName, "GET_PARAMETER") == 0) {
					handleCmd_notSupported();
				} else if (strcmp(cmdName, "SET_PARAMETER") == 0) {
					handleCmd_notSupported();
				}
			} else if (strcmp(cmdName, "DESCRIBE") == 0) {
				handleCmd_DESCRIBE(urlPreSuffix, urlSuffix, (const char*)fRequestBuffer);
			} else if (strcmp(cmdName, "SETUP") == 0) {
				handleCmd_SETUP(urlPreSuffix, urlSuffix, (const char*)fRequestBuffer);
			} else if (strcmp(cmdName, "TEARDOWN") == 0
				|| strcmp(cmdName, "PLAY") == 0
				|| strcmp(cmdName, "PAUSE") == 0
				|| strcmp(cmdName, "GET_PARAMETER") == 0
				|| strcmp(cmdName, "SET_PARAMETER") == 0) 
			{
				handleCmd_withinSession(cmdName, urlPreSuffix, urlSuffix, (char const*)fRequestBuffer);
			} else {
				handleCmd_notSupported();
			}
		} else {
			handleCmd_bad();
		}

		if (RTSPCommonEnv::nDebugFlag&DEBUG_FLAG_RTSP)
			DPRINTF("sending response:\n%s\n", fResponseBuffer);

		fClientSock->writeSocket((char *)fResponseBuffer, strlen((char *)fResponseBuffer));

		// Check whether there are extra bytes remaining in the buffer, after the end of the request (a rare case).
		// If so, move them to the front of our buffer, and keep processing it, because it might be a following, pipelined request.
		unsigned requestSize = (fLastCRLF+4-fRequestBuffer) + contentLength;
		numBytesRemaining = fRequestBytesAlreadySeen - requestSize;
		resetRequestBuffer(); // to prepare for any subsequent request

		if (numBytesRemaining > 0) {
			memmove(fRequestBuffer, &fRequestBuffer[requestSize], numBytesRemaining);
			newBytesRead = numBytesRemaining;
		}
	} while (numBytesRemaining > 0);

	if (!fIsActive) {
		delete this;
	}
}

void RTSPServer::RTSPClientSession::tcpReadHandler1()
{
	int result = 0;
	unsigned char c;
	struct sockaddr_in fromAddress;

	if (fTCPReadingState != AWAITING_PACKET_DATA) {
		result = fClientSock->readSocket1((char *)&c, 1, fromAddress);
		if (result != 1) {
			handleRequestBytes(result);
			return;
		}
	}

	switch (fTCPReadingState)
	{
	case AWAITING_DOLLAR: {
		if (c == '$') {
			fTCPReadingState = AWAITING_STREAM_CHANNEL_ID;
		} else {
			incomingRequestHandler1(c);
		}
						  } break;
	case AWAITING_STREAM_CHANNEL_ID: {
		if (lookupStreamChannelId(c)) {
			fStreamChannelId = c;
			fTCPReadingState = AWAITING_SIZE1;

			if (RTSPCommonEnv::nDebugFlag&DEBUG_FLAG_RTP)
				DPRINTF("channel id: %d\n", fStreamChannelId);
		} else {
			fTCPReadingState = AWAITING_DOLLAR;
		}
									 } break;
	case AWAITING_SIZE1: {
		fSizeByte1 = c;
		fTCPReadingState = AWAITING_SIZE2;
						 } break;
	case AWAITING_SIZE2: {
		fTCPReadSize = (fSizeByte1<<8) | c;
		fTCPReadingState = AWAITING_PACKET_DATA;
		fRtpBufferIdx = 0;

		if (RTSPCommonEnv::nDebugFlag&DEBUG_FLAG_RTP)
			DPRINTF("size: %d\n", fTCPReadSize);
						 } break;
	case AWAITING_PACKET_DATA: {
		readRTPOverTCP();
							   } break;
	}
}

bool RTSPServer::RTSPClientSession::lookupStreamChannelId(unsigned char channel)
{	
	return true;	// dummy
}

void RTSPServer::RTSPClientSession::readRTPOverTCP()
{
	int bytesRead = fTCPReadSize - fRtpBufferIdx;
	struct sockaddr_in fromAddress;

	int result = fClientSock->readSocket1(&fRtpBuffer[fRtpBufferIdx], bytesRead, fromAddress);
	if (result <= 0) {
		handleRequestBytes(result);
		return;
	}

	fRtpBufferIdx += result;

	if (fRtpBufferIdx != fTCPReadSize) 
		return;

	fTCPReadingState = AWAITING_DOLLAR;	
	fRtpBufferIdx = 0;
}

// Handler routines for specific RTSP commands:

void RTSPServer::RTSPClientSession::handleCmd_OPTIONS() 
{
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
		"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n",
		fCurrentCSeq, dateHeader(), fOurServer.allowedCommandNames());
}

void RTSPServer::RTSPClientSession::handleCmd_DESCRIBE(const char *urlPreSuffix, const char *urlSuffix, const char *fullRequestStr)
{
	ServerMediaSession* session = NULL;
	char* sdpDescription = NULL;
	char* rtspURL = NULL;
	do {
		char urlTotalSuffix[RTSP_PARAM_STRING_MAX];
		if (strlen(urlPreSuffix) + strlen(urlSuffix) + 2 > sizeof urlTotalSuffix) {
			handleCmd_bad();
			break;
		}
		urlTotalSuffix[0] = '\0';
		if (urlPreSuffix[0] != '\0') {
			strcat(urlTotalSuffix, urlPreSuffix);
			strcat(urlTotalSuffix, "/");
		}
		strcat(urlTotalSuffix, urlSuffix);

		if (fOurServer.fServerCallbackFunc) {
			OpenServerSessionParam *param = new OpenServerSessionParam(urlTotalSuffix);
			fOurServer.fServerCallbackFunc(fOurServer.fServerCallbackArg, param);
		}

		// We should really check that the request contains an "Accept:" #####
		// for "application/sdp", because that's what we're sending back #####

		// Begin by looking up the "ServerMediaSession" object for the specified "urlTotalSuffix":
		session = fOurServer.lookupServerMediaSession(urlTotalSuffix);
		if (session == NULL) {
			DPRINTF("[RTPServer] session %s not found\n", urlTotalSuffix);
			handleCmd_notFound();
			break;
		}

		// added by kimdh
		if (fOurServerMediaSession == NULL) {
			fOurServerMediaSession = session;
			fOurServerMediaSession->incrementReferenceCount();
		} else {
			if (fOurServerMediaSession != session) {
				session = NULL;
				handleCmd_bad();
				break;
			}
		}

		// Then, assemble a SDP description for this session:
		sdpDescription = session->generateSDPDescription();
		if (sdpDescription == NULL) {
			// This usually means that a file name that was specified for a
			// "ServerMediaSubsession" does not exist.
			setRTSPResponse("404 File Not Found, Or In Incorrect Format");
			break;
		}
		unsigned sdpDescriptionSize = strlen(sdpDescription);

		// Also, generate our RTSP URL, for the "Content-Base:" header
		// (which is necessary to ensure that the correct URL gets used in subsequent "SETUP" requests).
		rtspURL = fOurServer.rtspURL(session, fClientSock->sock());

		snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
			"%s"
			"Content-Base: %s/\r\n"
			"Content-Type: application/sdp\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",
			fCurrentCSeq,
			dateHeader(),
			rtspURL,
			sdpDescriptionSize,
			sdpDescription);
	} while (0);

	delete[] sdpDescription;
	delete[] rtspURL;
}

void RTSPServer::RTSPClientSession::handleCmd_SETUP(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr)
{
	// Normally, "urlPreSuffix" should be the session (stream) name, and "urlSuffix" should be the subsession (track) name.
	// However (being "liberal in what we accept"), we also handle 'aggregate' SETUP requests (i.e., without a track name),
	// in the special case where we have only a single track.  I.e., in this case, we also handle:
	//    "urlPreSuffix" is empty and "urlSuffix" is the session (stream) name, or
	//    "urlPreSuffix" concatenated with "urlSuffix" (with "/" inbetween) is the session (stream) name.
	char const* streamName = urlPreSuffix; // in the normal case
	char const* trackId = urlSuffix; // in the normal case
	char* concatenatedStreamName = NULL; // in the normal case

	do {
		// First, make sure the specified stream name exists:
		ServerMediaSession* sms = fOurServer.lookupServerMediaSession(streamName);
		if (sms == NULL) {
			// Check for the special case (noted above), before we give up:
			if (urlPreSuffix[0] == '\0') {
				streamName = urlSuffix;
			} else {
				concatenatedStreamName = new char[strlen(urlPreSuffix) + strlen(urlSuffix) + 2]; // allow for the "/" and the trailing '\0'
				sprintf(concatenatedStreamName, "%s/%s", urlPreSuffix, urlSuffix);
				streamName = concatenatedStreamName;
			}
			trackId = NULL;

			// Check again:
			sms = fOurServer.lookupServerMediaSession(streamName);

			// added by kimdh
			if (sms == NULL && fOurServerMediaSession) {
				sms = fOurServerMediaSession;
			}
		}

		if (sms == NULL) {
			if (fOurServerMediaSession == NULL) {
				// The client asked for a stream that doesn't exist (and this session descriptor has not been used before):
				handleCmd_notFound();
			} else {
				// The client asked for a stream that doesn't exist, but using a stream id for a stream that does exist. Bad request:
				handleCmd_bad();
			}
			break;
		} else {
			if (fOurServerMediaSession == NULL) {
				// We're accessing the "ServerMediaSession" for the first time.
				fOurServerMediaSession = sms;
				fOurServerMediaSession->incrementReferenceCount();
			} else if (sms != fOurServerMediaSession) {
				// The client asked for a stream that's different from the one originally requested for this stream id.  Bad request:
				handleCmd_bad();
				break;
			}
		}

		if (fStreamStates == NULL) {
			// This is the first "SETUP" for this session.  Set up our array of states for all of this session's subsessions (tracks):
			ServerMediaSubsessionIterator iter(*fOurServerMediaSession);
			for (fNumStreamStates = 0; iter.next() != NULL; ++fNumStreamStates) {} // begin by counting the number of subsessions (tracks)

			fStreamStates = new struct streamState[fNumStreamStates];

			iter.reset();
			ServerMediaSubsession* subsession;
			for (unsigned i = 0; i < fNumStreamStates; ++i) {
				subsession = iter.next();
				fStreamStates[i].subsession = subsession;
			}
		}

		// Look up information for the specified subsession (track):
		ServerMediaSubsession* subsession = NULL;
		unsigned streamNum;
		if (trackId != NULL && trackId[0] != '\0') { // normal case
			for (streamNum = 0; streamNum < fNumStreamStates; ++streamNum) {
				subsession = fStreamStates[streamNum].subsession;
				if (subsession != NULL && strcmp(trackId, subsession->trackId()) == 0) break;
			}
			if (streamNum >= fNumStreamStates) {
				// The specified track id doesn't exist, so this request fails:
				handleCmd_notFound();
				break;
			}
		} else {
			// Weird case: there was no track id in the URL.
			// This works only if we have only one subsession:
			if (fNumStreamStates != 1 || fStreamStates[0].subsession == NULL) {
				handleCmd_bad();
				break;
			}
			streamNum = 0;
			subsession = fStreamStates[streamNum].subsession;
		}
		// ASSERT: subsession != NULL

		// Look for a "Transport:" header in the request string, to extract client parameters:
		StreamingMode streamingMode;

		char* streamingModeString = NULL; // set when RAW_UDP streaming is specified
		char* clientsDestinationAddressStr;
		unsigned char clientsDestinationTTL;
		unsigned short clientRTPPortNum, clientRTCPPortNum;
		unsigned char rtpChannelId, rtcpChannelId;

		parseTransportHeader(fullRequestStr, streamingMode, streamingModeString,
			clientsDestinationAddressStr, clientsDestinationTTL,
			clientRTPPortNum, clientRTCPPortNum,
			rtpChannelId, rtcpChannelId);

		if (streamingMode == RTP_TCP && rtpChannelId == 0xFF) {
			streamingMode = RTP_TCP;
			rtpChannelId = fTCPStreamIdCount; rtcpChannelId = fTCPStreamIdCount+1;
		}
		if (streamingMode == RTP_TCP) fTCPStreamIdCount += 2;

		unsigned short clientRTPPort = clientRTPPortNum;
		unsigned short clientRTCPPort = clientRTCPPortNum;

		// Then, get server parameters from the 'subsession':
		int tcpSocketNum = streamingMode == RTP_TCP ? fClientSock->sock() : -1;
		unsigned int destinationAddress = 0;
		unsigned char destinationTTL = 255;

		delete[] clientsDestinationAddressStr;
		unsigned short serverRTPPort = 0;
		unsigned short serverRTCPPort = 0;

		struct sockaddr_in sourceAddr; socklen_t namelen = sizeof sourceAddr;
		getsockname(fClientSock->sock(), (struct sockaddr*)&sourceAddr, &namelen);

		subsession->getStreamParameters(fOurSessionId, fClientSock->clientAddress().sin_addr.s_addr,
			clientRTPPort, clientRTCPPort,
			tcpSocketNum, rtpChannelId, rtcpChannelId,
			destinationAddress, destinationTTL, fIsMulticast,
			serverRTPPort, serverRTCPPort);

		// add client socket
		if (streamingMode == RTP_UDP) {
			MySock *rtpSock = new MySock();
			rtpSock->setupDatagramSock(serverRTPPort, true);
			if (rtpSock->sock() < 0) {
				DPRINTF("failed to setup rtp udp socket %d !!!\n", rtpSock->sock());
				delete rtpSock;
				delete[] streamingModeString;
				handleCmd_notFound();
				break;
			}
			rtpSock->setSendBufferTo(1024*1024*5);
			
			struct sockaddr_in rtpDestAddr;
			memset(&rtpDestAddr, 0, sizeof(struct sockaddr_in));
			rtpDestAddr.sin_family = AF_INET;
			rtpDestAddr.sin_addr.s_addr = destinationAddress;
			rtpDestAddr.sin_port = htons(clientRTPPort);

			MySock *rtcpSock = new MySock();
			rtcpSock->setupDatagramSock(serverRTCPPort, true);
			if (rtpSock->sock() < 0) {
				DPRINTF("failed to setup rtcp udp socket %d !!!\n", rtpSock->sock());
				delete rtpSock;
				delete rtcpSock;
				delete[] streamingModeString;
				handleCmd_notFound();
				break;
			}

			struct sockaddr_in rtcpDestAddr;
			memset(&rtcpDestAddr, 0, sizeof(struct sockaddr_in));
			rtcpDestAddr.sin_family = AF_INET;
			rtcpDestAddr.sin_addr.s_addr = destinationAddress;
			rtcpDestAddr.sin_port = htons(clientRTCPPort);

			ClientSocket *clientSock = new ClientSocket(*rtpSock, rtpDestAddr, *rtcpSock, rtcpDestAddr);
			fClientSockList.insert(clientSock);
			subsession->addClientSock(clientSock);
		} else if (streamingMode == RTP_TCP) {
			fClientSock->setSendBufferTo(1024*1024*5);
			ClientSocket *clientSock = new ClientSocket(*fClientSock, rtpChannelId, rtcpChannelId);
			fClientSockList.insert(clientSock);
			subsession->addClientSock(clientSock);
		}

		AddressString destAddrStr(destinationAddress);
		AddressString sourceAddrStr(sourceAddr);

		if (fIsMulticast) {
			switch (streamingMode) {
		  case RTP_UDP: {
			  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
				  "RTSP/1.0 200 OK\r\n"
				  "CSeq: %s\r\n"
				  "%s"
				  "Transport: RTP/AVP;multicast;destination=%s;source=%s;port=%d-%d;ttl=%d\r\n"
				  "Session: %08X\r\n\r\n",
				  fCurrentCSeq,
				  dateHeader(),				  
				  destAddrStr.val(), sourceAddrStr.val(), serverRTPPort, serverRTCPPort, destinationTTL,
				  fOurSessionId);
			  break;
						}
		  case RTP_TCP: {
			  // multicast streams can't be sent via TCP
			  handleCmd_unsupportedTransport();
			  break;
						}
		  case RAW_UDP: {
			  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
				  "RTSP/1.0 200 OK\r\n"
				  "CSeq: %s\r\n"
				  "%s"
				  "Transport: %s;multicast;destination=%s;source=%s;port=%d;ttl=%d\r\n"
				  "Session: %08X\r\n\r\n",
				  fCurrentCSeq,
				  dateHeader(),
				  streamingModeString, destAddrStr.val(), sourceAddrStr.val(), serverRTPPort, destinationTTL,
				  fOurSessionId);
			  break;
						}
			}
		} else {
			switch (streamingMode) {
		  case RTP_UDP: {
			  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
				  "RTSP/1.0 200 OK\r\n"
				  "CSeq: %s\r\n"
				  "%s"
				  "Transport: RTP/AVP;unicast;destination=%s;source=%s;client_port=%d-%d;server_port=%d-%d\r\n"
				  "Session: %08X\r\n\r\n",
				  fCurrentCSeq,
				  dateHeader(),
				  destAddrStr.val(), sourceAddrStr.val(), clientRTPPort, clientRTCPPort, serverRTPPort, serverRTCPPort,
				  fOurSessionId);
			  break;
						}
		  case RTP_TCP: {
			  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
				  "RTSP/1.0 200 OK\r\n"
				  "CSeq: %s\r\n"
				  "%s"
				  "Transport: RTP/AVP/TCP;unicast;destination=%s;source=%s;interleaved=%d-%d\r\n"
				  "Session: %08X\r\n\r\n",
				  fCurrentCSeq,
				  dateHeader(),
				  destAddrStr.val(), sourceAddrStr.val(), rtpChannelId, rtcpChannelId,
				  fOurSessionId);			  
			  break;
						}
		  case RAW_UDP: {
			  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
				  "RTSP/1.0 200 OK\r\n"
				  "CSeq: %s\r\n"
				  "%s"
				  "Transport: %s;unicast;destination=%s;source=%s;client_port=%d;server_port=%d\r\n"
				  "Session: %08X\r\n\r\n",
				  fCurrentCSeq,
				  dateHeader(),
				  streamingModeString, destAddrStr.val(), sourceAddrStr.val(), clientRTPPort, serverRTPPort,
				  fOurSessionId);
			  break;
						}
			}
		}
		delete[] streamingModeString;
	} while (0);

	delete[] concatenatedStreamName;
}

void RTSPServer::RTSPClientSession::handleCmd_PLAY(ServerMediaSubsession* subsession, char const* fullRequestStr)
{
	char* rtspURL = fOurServer.rtspURL(fOurServerMediaSession, fClientSock->sock());
	unsigned rtspURLSize = strlen(rtspURL);

	// Parse the client's "Scale:" header, if any:
	float scale;
	bool sawScaleHeader = parseScaleHeader(fullRequestStr, scale);

	char buf[100];
	char* scaleHeader;
	if (!sawScaleHeader) {
		buf[0] = '\0'; // Because we didn't see a Scale: header, don't send one back
	} else {
		sprintf(buf, "Scale: %f\r\n", scale);
	}
	scaleHeader = strDup(buf);

	// Parse the client's "Range:" header, if any:
	float duration = 0.0;
	double rangeStart = 0.0, rangeEnd = 0.0;
	char* absStart = NULL; char* absEnd = NULL;
	bool startTimeIsNow;
	bool sawRangeHeader = parseRangeHeader(fullRequestStr, rangeStart, rangeEnd, absStart, absEnd, startTimeIsNow);

	if (sawRangeHeader && absStart == NULL/*not seeking by 'absolute' time*/) {
		// Use this information, plus the stream's duration (if known), to create our own "Range:" header, for the response:
		duration = subsession == NULL /*aggregate op*/
			? fOurServerMediaSession->duration() : subsession->duration();
		if (duration < 0.0) {
			// We're an aggregate PLAY, but the subsessions have different durations.
			// Use the largest of these durations in our header
			duration = -duration;
		}

		// Make sure that "rangeStart" and "rangeEnd" (from the client's "Range:" header)
		// have sane values, before we send back our own "Range:" header in our response:
		if (rangeStart < 0.0) rangeStart = 0.0;
		else if (rangeStart > duration) rangeStart = duration;
		if (rangeEnd < 0.0) rangeEnd = 0.0;
		else if (rangeEnd > duration) rangeEnd = duration;

		if ((scale > 0.0 && rangeStart > rangeEnd && rangeEnd > 0.0) || (scale < 0.0 && rangeStart < rangeEnd)) {
			// "rangeStart" and "rangeEnd" were the wrong way around; swap them:
			double tmp = rangeStart;
			rangeStart = rangeEnd;
			rangeEnd = tmp;
		}
	}

	// Create a "RTP-Info:" line.  It will get filled in from each subsession's state:
	char const* rtpInfoFmt =
		"%s" // "RTP-Info:", plus any preceding rtpInfo items
		"%s" // comma separator, if needed
		"url=%s/%s"
		";seq=%d"
		";rtptime=%u"
		;
	unsigned rtpInfoFmtSize = strlen(rtpInfoFmt);
	char* rtpInfo = strDup("RTP-Info: ");
	unsigned i, numRTPInfoItems = 0;

	// Create the "Range:" header that we'll send back in our response.
	// (Note that we do this after seeking, in case the seeking operation changed the range start time.)
	if (absStart != NULL) {
		// We're seeking by 'absolute' time:
		if (absEnd == NULL) {
			sprintf(buf, "Range: clock=%s-\r\n", absStart);
		} else {
			sprintf(buf, "Range: clock=%s-%s\r\n", absStart, absEnd);
		}
		delete[] absStart; delete[] absEnd;
	} else {
		// We're seeking by relative (NPT) time:
		if (!sawRangeHeader || startTimeIsNow) {
			// We didn't seek, so in our response, begin the range with the current NPT (normal play time):
			float curNPT = 0.0;
			for (i = 0; i < fNumStreamStates; ++i) {
				if (subsession == NULL /* means: aggregated operation */
					|| subsession == fStreamStates[i].subsession) {
					if (fStreamStates[i].subsession == NULL) continue;
					float npt = fStreamStates[i].subsession->getCurrentNPT();
					if (npt > curNPT) curNPT = npt;
					// Note: If this is an aggregate "PLAY" on a multi-subsession stream,
					// then it's conceivable that the NPTs of each subsession may differ
					// (if there has been a previous seek on just one subsession).
					// In this (unusual) case, we just return the largest NPT; I hope that turns out OK...
				}
			}
			rangeStart = curNPT;
		}

		if (rangeEnd == 0.0 && scale >= 0.0) {
			sprintf(buf, "Range: npt=%.3f-\r\n", rangeStart);
		} else {
			sprintf(buf, "Range: npt=%.3f-%.3f\r\n", rangeStart, rangeEnd);
		}
	}
	char* rangeHeader = strDup(buf);

	STREAM_STATE currentState = STREAM_STATE_STOPPED;

	// Now, start streaming:
	if (fOurServerMediaSession) {		
		// activate all client sockets
		fClientSockList.gotoBeginCursor();
		ClientSocket *cursor = fClientSockList.getNextCursor();
		while (cursor) {
			cursor->activate();
			cursor = fClientSockList.getNextCursor();
		}

		currentState = fOurServerMediaSession->streamState();

		if (currentState == STREAM_STATE_STOPPED)
			fOurServerMediaSession->startStream(0.0);
		else if (currentState == STREAM_STATE_PAUSED)
			fOurServerMediaSession->resumeStream();
		else if (currentState == STREAM_STATE_RUNNING)
			fOurServerMediaSession->playContinueStream();
	} else {
		handleCmd_bad();
		return;
	}

	if (numRTPInfoItems == 0) {
		rtpInfo[0] = '\0';
	} else {
		unsigned rtpInfoLen = strlen(rtpInfo);
		rtpInfo[rtpInfoLen] = '\r';
		rtpInfo[rtpInfoLen+1] = '\n';
		rtpInfo[rtpInfoLen+2] = '\0';
	}

	// Fill in the response:
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n"
		"%s"
		"%s"
		"%s"
		"Session: %08X\r\n"
		"%s\r\n",
		fCurrentCSeq,
		dateHeader(),
		scaleHeader,
		rangeHeader,
		fOurSessionId,
		rtpInfo);
	delete[] rtpInfo; delete[] rangeHeader;
	delete[] scaleHeader; delete[] rtspURL;

	if (fOurServer.fServerCallbackFunc) {
		if (fOurServerMediaSession->sessionType() == SESSION_ONDEMAND && currentState != STREAM_STATE_STOPPED) {
			// ondemand
		} else {
			ClientConnectedParam *param = new ClientConnectedParam(
				fOurServerMediaSession->streamName(),
				fClientSock->sock(), fClientSock->clientAddress(), 
				fTCPStreamIdCount == 0 ? 0 : 1);
			fOurServer.fServerCallbackFunc(fOurServer.fServerCallbackArg, param);
		}
	}
}

void RTSPServer::RTSPClientSession::handleCmd_TEARDOWN(ServerMediaSubsession *subsession)
{
	if (fOurServerMediaSession)
		fOurServerMediaSession->stopStream();

	setRTSPResponse("200 OK");
}

void RTSPServer::RTSPClientSession::handleCmd_PAUSE(ServerMediaSubsession *subsession)
{
	if (fOurServerMediaSession && fOurServerMediaSession->sessionType() == SESSION_ONDEMAND) {
		fOurServerMediaSession->pauseStream();
	}

	setRTSPResponse("200 OK", fOurSessionId);
}

void RTSPServer::RTSPClientSession::handleCmd_GET_PARAMETER(ServerMediaSubsession *subsession, const char *fullRequestStr)
{
	setRTSPResponse("200 OK", fOurSessionId);
}

void RTSPServer::RTSPClientSession::handleCmd_SET_PARAMETER(ServerMediaSubsession *subsession, const char *fullRequestStr)
{			
	char* nextLineStart = getLine((char *)fullRequestStr);
	char* lineStart;
	while (1) {
		lineStart = nextLineStart;
		if (lineStart == NULL) 
			break;

		nextLineStart = getLine(lineStart);

		if (strncmp(lineStart, "command: ", 9) == 0) {
			char *strCommand = strDup(&lineStart[9]);

			if (strcmp(strCommand, "playforward") == 0) {
				fOurServerMediaSession->forwardStream();	
			} else if (strcmp(strCommand, "playbackward") == 0) {
				fOurServerMediaSession->backwardStream();
			} else if (strcmp(strCommand, "playforwardnext") == 0) {
				fOurServerMediaSession->forwardNextStream();
			} else if (strcmp(strCommand, "playbackwardnext") == 0) {
				fOurServerMediaSession->backwardNextStream();
			} else if (strncmp(strCommand, "seek=", 5) == 0) {
				char *seektime = strDup(&strCommand[5]);
				int timestamp = atoi(seektime);
				delete[] seektime;
				fOurServerMediaSession->seekStream(timestamp);
			} else if (strncmp(strCommand, "speed=", 6) == 0) {
				char *speedbuf = strDup(&strCommand[6]);
				float speed = atof(speedbuf);
				delete[] speedbuf;
				fOurServerMediaSession->speedStream(speed);
			}

			delete[] strCommand;
			break;
		}
	}

	setRTSPResponse("200 OK");
}

void RTSPServer::RTSPClientSession::handleCmd_withinSession(
			  char const* cmdName,
			  char const* urlPreSuffix, char const* urlSuffix,
			  char const* fullRequestStr) 
{
	// This will either be:
	// - a non-aggregated operation, if "urlPreSuffix" is the session (stream)
	//   name and "urlSuffix" is the subsession (track) name, or
	// - an aggregated operation, if "urlSuffix" is the session (stream) name,
	//   or "urlPreSuffix" is the session (stream) name, and "urlSuffix" is empty,
	//   or "urlPreSuffix" and "urlSuffix" are both nonempty, but when concatenated, (with "/") form the session (stream) name.
	// Begin by figuring out which of these it is:
	ServerMediaSubsession* subsession;

	if (fOurServerMediaSession == NULL) { // There wasn't a previous SETUP!
		handleCmd_notSupported();
		return;
	} else if (urlSuffix[0] != '\0' && strcmp(fOurServerMediaSession->streamName(), urlPreSuffix) == 0) {
		// Non-aggregated operation.
		// Look up the media subsession whose track id is "urlSuffix":
		ServerMediaSubsessionIterator iter(*fOurServerMediaSession);
		while ((subsession = iter.next()) != NULL) {
			if (strcmp(subsession->trackId(), urlSuffix) == 0) break; // success
		}
		if (subsession == NULL) { // no such track!
			handleCmd_notFound();
			return;
		}
	} else if (strcmp(fOurServerMediaSession->streamName(), urlSuffix) == 0 ||
		(urlSuffix[0] == '\0' && strcmp(fOurServerMediaSession->streamName(), urlPreSuffix) == 0)) {
			// Aggregated operation
			subsession = NULL;
	} else if (urlPreSuffix[0] != '\0' && urlSuffix[0] != '\0') {
		// Aggregated operation, if <urlPreSuffix>/<urlSuffix> is the session (stream) name:
		unsigned const urlPreSuffixLen = strlen(urlPreSuffix);
		if (strncmp(fOurServerMediaSession->streamName(), urlPreSuffix, urlPreSuffixLen) == 0 &&
			fOurServerMediaSession->streamName()[urlPreSuffixLen] == '/' &&
			strcmp(&(fOurServerMediaSession->streamName())[urlPreSuffixLen+1], urlSuffix) == 0) {
				subsession = NULL;
		} else {
			handleCmd_notFound();
			return;
		}
	} else { // the request doesn't match a known stream and/or track at all!
		handleCmd_notFound();
		return;
	}

	if (strcmp(cmdName, "TEARDOWN") == 0) {
		handleCmd_TEARDOWN(subsession);
	} else if (strcmp(cmdName, "PLAY") == 0) {
		handleCmd_PLAY(subsession, fullRequestStr);
	} else if (strcmp(cmdName, "PAUSE") == 0) {
		handleCmd_PAUSE(subsession);
	} else if (strcmp(cmdName, "GET_PARAMETER") == 0) {
		handleCmd_GET_PARAMETER(subsession, fullRequestStr);
	} else if (strcmp(cmdName, "SET_PARAMETER") == 0) {
		handleCmd_SET_PARAMETER(subsession, fullRequestStr);
	}
}

void RTSPServer::RTSPClientSession::handleCmd_bad()
{
	// Don't do anything with "fCurrentCSeq", because it might be nonsense
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
		"RTSP/1.0 400 Bad Request\r\n%sAllow: %s\r\n\r\n",
		dateHeader(), fOurServer.allowedCommandNames());
}

void RTSPServer::RTSPClientSession::handleCmd_notSupported() 
{
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
		"RTSP/1.0 405 Method Not Allowed\r\nCSeq: %s\r\n%sAllow: %s\r\n\r\n",
		fCurrentCSeq, dateHeader(), fOurServer.allowedCommandNames());
}

void RTSPServer::RTSPClientSession::handleCmd_notFound() 
{
	setRTSPResponse("404 Stream Not Found");
}

void RTSPServer::RTSPClientSession::handleCmd_sessionNotFound() 
{
	setRTSPResponse("454 Session Not Found");
}

void RTSPServer::RTSPClientSession::handleCmd_unsupportedTransport()
{
	setRTSPResponse("461 Unsupported Transport");
}

void RTSPServer::RTSPClientSession::setRTSPResponse(char const* responseStr) 
{
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s\r\n",
		responseStr,
		fCurrentCSeq,
		dateHeader());
}

void RTSPServer::RTSPClientSession::setRTSPResponse(char const* responseStr, u_int32_t sessionId) 
{
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Session: %08X\r\n\r\n",
		responseStr,
		fCurrentCSeq,
		dateHeader(),
		sessionId);
}

void RTSPServer::RTSPClientSession::setRTSPResponse(char const* responseStr, char const* contentStr) 
{
	if (contentStr == NULL) contentStr = "";
	unsigned const contentLen = strlen(contentStr);

	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Content-Length: %d\r\n\r\n"
		"%s",
		responseStr,
		fCurrentCSeq,
		dateHeader(),
		contentLen,
		contentStr);
}

void RTSPServer::RTSPClientSession::setRTSPResponse(char const* responseStr, u_int32_t sessionId, char const* contentStr) 
{
	if (contentStr == NULL) contentStr = "";
	unsigned const contentLen = strlen(contentStr);

	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Session: %08X\r\n"
		"Content-Length: %d\r\n\r\n"
		"%s",
		responseStr,
		fCurrentCSeq,
		dateHeader(),
		sessionId,
		contentLen,
		contentStr);
}

CallbackParam::CallbackParam(ServerCallbackType type)
{
	fType = type;
}

OpenServerSessionParam::OpenServerSessionParam(char *requestString) : CallbackParam(OPEN_SERVER_SESSION)
{
	snprintf(fRequestString, sizeof(fRequestString), "%s", requestString);
}

ClientConnectedParam::ClientConnectedParam(const char *sessionName, int sock, sockaddr_in &sockAddr, int streamType) : CallbackParam(CLIENT_CONNECTED)
{
	snprintf(fSessionName, sizeof(fSessionName), "%s", sessionName);
	fSock = sock;
	memcpy(&fClientAddr, &sockAddr, sizeof(fClientAddr));
	fStreamType = streamType;
}

ClientDisconnectedParam::ClientDisconnectedParam(int sock) : CallbackParam(CLIENT_DISCONNECTED)
{
	fSock = sock;
}
