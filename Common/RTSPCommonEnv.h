#ifndef __RTSP_COMMON_ENV_H__
#define __RTSP_COMMON_ENV_H__

#ifdef WIN32
#include <windows.h>
#else
#include <stdarg.h>
extern int _vscprintf (const char * format, va_list pargs);
#endif

#ifdef ANDROID
#include <android/log.h>
#define DPRINTF(...)	__android_log_print(ANDROID_LOG_DEBUG, "RTSPClient", __VA_ARGS__)
#define DPRINTF0(X)	__android_log_print(ANDROID_LOG_DEBUG, "RTSPClient", "%s\n", X)
#else
#define DPRINTF		RTSPCommonEnv::DebugPrint
#define DPRINTF0	RTSPCommonEnv::DebugPrint
#endif

#define DEBUG_FLAG_RTSP			(0x01)
#define DEBUG_FLAG_RTP			(0x02)
#define DEBUG_FLAG_RTP_PAYLOAD	(0x04)
#define DEBUG_FLAG_ALL			(0xFF)

#define DELETE_OBJECT(obj) if (obj) { delete obj; obj = NULL; }
#define DELETE_ARRAY(arr) if (arr) { delete[] arr; arr = NULL; }

class RTSPCommonEnv
{
public:
	static int	nDebugFlag;
	static int	nDebugPrint;	// 0:console, 1:debugview, others:none

	static unsigned short	nClientPortRangeMin;
	static unsigned short	nClientPortRangeMax;

	static unsigned short	nServerPortRangeMin;
	static unsigned short	nServerPortRangeMax;

	static void DebugPrint(char *lpszFormat, ...);
	static void SetDebugFlag(int flag);
	static void UnsetDebugFlag(int flag);
};

#endif
