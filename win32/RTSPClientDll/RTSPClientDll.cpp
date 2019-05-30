// RTSPClientDll.cpp : DLL 응용 프로그램을 위해 내보낸 함수를 정의합니다.
//

#include "stdafx.h"
#include "RTSPClientDll.h"
#include "RTSPClient.h"
#include "RTSPCommonEnv.h"

RTSPCLIENTDLL_API void* rtspclient_new()
{
	return new RTSPClient();
}

RTSPCLIENTDLL_API void rtspclient_delete(void *rtspclient)
{
	if (rtspclient) {
		RTSPClient *pRTSPClient = (RTSPClient *)rtspclient;
		delete pRTSPClient;
	}
}

RTSPCLIENTDLL_API int rtspclient_open_url(void *rtspclient, const char *url, int conn_type, int timeout)
{
	if (rtspclient) {
		RTSPClient *pRTSPClient = (RTSPClient *)rtspclient;
		return pRTSPClient->openURL(url, conn_type, timeout);
	}
	return -1;
}

RTSPCLIENTDLL_API int rtspclient_play_url(void *rtspclient, DllFrameHandlerFunc *func, void *funcData)
{
	if (rtspclient) {
		RTSPClient *pRTSPClient = (RTSPClient *)rtspclient;
		return pRTSPClient->playURL((FrameHandlerFunc)func, funcData, NULL, NULL, NULL, NULL);
	}
	return -1;
}

RTSPCLIENTDLL_API void rtspclient_close_url(void *rtspclient)
{
	if (rtspclient) {
		RTSPClient *pRTSPClient = (RTSPClient *)rtspclient;
		pRTSPClient->closeURL();
	}
}

RTSPCLIENTDLL_API void rtspclient_set_debug_flag(int flag)
{
	RTSPCommonEnv::SetDebugFlag(flag);
}

RTSPCLIENTDLL_API void rtspclient_set_debug_print(int print)
{
	RTSPCommonEnv::nDebugPrint = print;
}
