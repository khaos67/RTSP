#include "util.h"
#include "NetCommon.h"
#include "RTSPCommonEnv.h"

char* strDup(char const* str) 
{
	if (str == NULL) return NULL;
	size_t len = strlen(str) + 1;
	char* copy = new char[len];

	if (copy != NULL) {
		memcpy(copy, str, len);
	}
	return copy;
}

char* strDupSize(char const* str) 
{
	if (str == NULL) return NULL;
	size_t len = strlen(str) + 1;
	char* copy = new char[len];

	return copy;
}

int CheckUdpPort(unsigned short port)
{
	int newSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (newSocket < 0) {
		DPRINTF("unable to create datagram socket: \n");
		return -1;
	}

	struct sockaddr_in c_addr;
	memset(&c_addr, 0, sizeof(c_addr));
	c_addr.sin_addr.s_addr = INADDR_ANY;
	c_addr.sin_family = AF_INET;
	c_addr.sin_port = htons(port);

	if (bind(newSocket, (struct sockaddr*)&c_addr, sizeof c_addr) != 0) {
		char tmpBuffer[100];
		sprintf(tmpBuffer, "[%s] bind() error (port number: %d): ", __FUNCTION__, port);
		DPRINTF0(tmpBuffer);
		closeSocket(newSocket);
		return -1;
	}

	closeSocket(newSocket);
	return 0;
}

#if (defined(__WIN32__) || defined(_WIN32)) && !defined(IMN_PIM)
// For Windoze, we need to implement our own gettimeofday()
#if !defined(_WIN32_WCE)
#include <sys/timeb.h>
#endif

int gettimeofday(struct timeval* tp, int* /*tz*/) {
#if defined(_WIN32_WCE)
  /* FILETIME of Jan 1 1970 00:00:00. */
  static const unsigned __int64 epoch = 116444736000000000L;

  FILETIME    file_time;
  SYSTEMTIME  system_time;
  ULARGE_INTEGER ularge;

  GetSystemTime(&system_time);
  SystemTimeToFileTime(&system_time, &file_time);
  ularge.LowPart = file_time.dwLowDateTime;
  ularge.HighPart = file_time.dwHighDateTime;

  tp->tv_sec = (long) ((ularge.QuadPart - epoch) / 10000000L);
  tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
#else
#ifdef USE_OLD_GETTIMEOFDAY_FOR_WINDOWS_CODE
  struct timeb tb;
  ftime(&tb);
  tp->tv_sec = tb.time;
  tp->tv_usec = 1000*tb.millitm;
#else
  LARGE_INTEGER tickNow;
  static LARGE_INTEGER tickFrequency;
  static BOOL tickFrequencySet = FALSE;
  if (tickFrequencySet == FALSE) {
    QueryPerformanceFrequency(&tickFrequency);
    tickFrequencySet = TRUE;
  }
  QueryPerformanceCounter(&tickNow);
  tp->tv_sec = (long) (tickNow.QuadPart / tickFrequency.QuadPart);
  tp->tv_usec = (long) (((tickNow.QuadPart % tickFrequency.QuadPart) * 1000000L) / tickFrequency.QuadPart);
#endif
#endif
  return 0;
}
#endif
