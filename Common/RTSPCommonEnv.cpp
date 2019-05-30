#include "RTSPCommonEnv.h"

#include <stdio.h>
#ifndef WIN32
#include <stdlib.h>
#include <string.h>
int _vscprintf (const char * format, va_list pargs)
{ 
    int retval; 
    va_list argcopy;
    va_copy(argcopy, pargs); 
    retval = vsnprintf(NULL, 0, format, argcopy); 
    va_end(argcopy); 
    return retval;
}
#endif

int RTSPCommonEnv::nDebugPrint = 0;
int RTSPCommonEnv::nDebugFlag = DEBUG_FLAG_RTSP;

unsigned short RTSPCommonEnv::nClientPortRangeMin = 10000;
unsigned short RTSPCommonEnv::nClientPortRangeMax = 65535;

unsigned short RTSPCommonEnv::nServerPortRangeMin = 20000;
unsigned short RTSPCommonEnv::nServerPortRangeMax = 65535;

#ifndef ANDROID
void RTSPCommonEnv::DebugPrint(char *lpszFormat, ...)
{
	va_list args;
	int len;
	char *buffer;

	va_start(args, lpszFormat);

	len = _vscprintf(lpszFormat, args) + 32;
	buffer = (char *)malloc(len * sizeof(char));

	const char *prefix = "[RTSP] ";

	int prefix_len = strlen(prefix);
	memcpy(buffer, prefix, prefix_len);

	vsprintf(&buffer[prefix_len], lpszFormat, args);
#ifdef WIN32
	if (nDebugPrint == 0) fprintf(stdout, buffer);
	else if (nDebugPrint == 1) OutputDebugString(buffer);
#else
	fprintf(stdout, buffer);
#endif

	free(buffer);
}
#endif

void RTSPCommonEnv::SetDebugFlag(int flag)
{
	nDebugFlag |= flag;
}

void RTSPCommonEnv::UnsetDebugFlag(int flag)
{
	int x = ~flag;
	nDebugFlag &= x;
}
