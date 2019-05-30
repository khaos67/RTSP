// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the RTSPCLIENTDLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// RTSPCLIENTDLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef RTSPCLIENTDLL_EXPORTS
#define RTSPCLIENTDLL_API __declspec(dllexport)
#else
#define RTSPCLIENTDLL_API __declspec(dllimport)
#endif

#define DEBUG_FLAG_RTSP			(0x00000001)
#define DEBUG_FLAG_RTP			(0x00000010)
#define DEBUG_FLAG_RTP_PAYLOAD	(0x00000100)

typedef enum DLL_RTP_FRAME_TYPE { DLL_FRAME_TYPE_VIDEO, DLL_FRAME_TYPE_AUDIO, DLL_FRAME_TYPE_ETC };
typedef void DllFrameHandlerFunc(void *arg, DLL_RTP_FRAME_TYPE frame_type, __int64 timestamp, unsigned char *buf, int len);

RTSPCLIENTDLL_API void* rtspclient_new();
RTSPCLIENTDLL_API void rtspclient_delete(void *rtspclient);
RTSPCLIENTDLL_API int rtspclient_open_url(void *rtspclient, const char *url, int conn_type, int timeout);
RTSPCLIENTDLL_API int rtspclient_play_url(void *rtspclient, DllFrameHandlerFunc *func, void *funcData);
RTSPCLIENTDLL_API void rtspclient_close_url(void *rtspclient);
RTSPCLIENTDLL_API void rtspclient_set_debug_flag(int flag);
RTSPCLIENTDLL_API void rtspclient_set_debug_print(int print);
