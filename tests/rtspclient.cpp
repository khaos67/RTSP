#include <stdio.h>
#ifdef WIN32
#include <conio.h>
#endif

//#define RTSPCLIENT_DLL

#ifdef LINUX
#undef RTSPCLIENT_DLL
#endif

#ifdef RTSPCLIENT_DLL
#pragma comment(lib, "RTSPClientDll.lib")
#include "RTSPClientDll.h"
#else
#pragma comment(lib, "RTSPClientLib.lib")
#include "RTSPClient.h"
#include "RTSPCommonEnv.h"
#endif

#ifdef WIN32
#include <windows.h>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#define mygetch	getch

#elif defined(LINUX)
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

int mygetch(void)
{
    struct termios oldt,
    newt;
    int ch;
    tcgetattr( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
    return ch;
}
#endif

FILE *fp_dump = NULL;

#ifdef RTSPCLIENT_DLL
static void frameHandlerFunc(void *arg, DLL_RTP_FRAME_TYPE frame_type, __int64 timestamp, unsigned char *buf, int len)
#else
static void frameHandlerFunc(void *arg, RTP_FRAME_TYPE frame_type, int64_t timestamp, unsigned char *buf, int len)
#endif
{
	if (fp_dump)
		fwrite(buf, len, 1, fp_dump);
}

int main(int argc, char *argv[])
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	int retry = 1;

#ifdef RTSPCLIENT_DLL
	rtspclient_set_debug_flag(DEBUG_FLAG_RTSP);
#else
	RTSPCommonEnv::SetDebugFlag(DEBUG_FLAG_RTSP);
#endif
	char *strURL = "rtsp://222.96.113.48:4554/AVStream1_2";

	fp_dump = fopen("video.264", "wb");

#ifdef RTSPCLIENT_DLL
	void *rtspClient = rtspclient_new();
#else
	RTSPClient *rtspClient = new RTSPClient();
#endif

again:
#ifdef RTSPCLIENT_DLL
	if (rtspclient_open_url(rtspClient, strURL, 1, 2) == 0)
#else
	if (rtspClient->openURL(strURL, 1, 2) == 0)
#endif
	{
#ifdef RTSPCLIENT_DLL
		if (rtspclient_play_url(rtspClient, frameHandlerFunc, rtspClient) == 0)
#else
		if (rtspClient->playURL(frameHandlerFunc, rtspClient, NULL, NULL) == 0)
#endif
		{
			char c;
			while (c = mygetch() != 'q')
			{
#ifdef WIN32
				Sleep(10);
#else
				usleep(10000);
#endif
			}
		}
	}	
exit:
#ifdef RTSPCLIENT_DLL
	rtspclient_close_url(rtspClient);
#else
	rtspClient->closeURL();
#endif

	if (--retry >  0)
		goto again;

#ifdef RTSPCLIENT_DLL
	rtspclient_delete(rtspClient);
#else
	delete rtspClient;
#endif

	if (fp_dump) fclose(fp_dump);

	return 0;
}
