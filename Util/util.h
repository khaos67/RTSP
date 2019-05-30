#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <Winsock2.h>
int gettimeofday(struct timeval* tp, int* /*tz*/);
#else
#include <sys/time.h>
#endif

extern char* strDup(char const* str);
extern char* strDupSize(char const* str);
extern int CheckUdpPort(unsigned short port);

#endif
