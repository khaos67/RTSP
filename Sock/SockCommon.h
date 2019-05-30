#ifndef __SOCK_COMMON_H__
#define __SOCK_COMMON_H__

#include "NetCommon.h"

int setupStreamSock(short port, int makeNonBlocking);
int setupDatagramSock(short port, int makeNonBlocking);
int setupServerSock(short port, int makeNonBlocking);
int setupClientSock(int serverSock, int makeNonBlocking, struct sockaddr_in& clientAddr);
int makeSocketNonBlocking(int sock);

int makeTCP_NoDelay(int sock);

unsigned setSendBufferTo(int sock, unsigned requestedSize);
unsigned setReceiveBufferTo(int sock, unsigned requestedSize);
unsigned getSendBufferSize(int sock);
unsigned getReceiveBufferSize(int sock);

unsigned getBufferSize(int bufOptName, int sock);
unsigned setBufferSizeTo(int bufOptName, int sock, int requestedSize);

int blockUntilReadable(int sock, struct timeval* timeout);

int readSocket1(int sock, char *buffer, unsigned bufferSize, struct sockaddr_in &fromAddress);
int readSocket(int sock, char *buffer, unsigned bufferSize, struct sockaddr_in &fromAddress, struct timeval *timeout = NULL);
int readSocketExact(int sock, char *buffer, unsigned bufferSize, struct sockaddr_in &fromAddress, struct timeval *timeout = NULL);

int writeSocket(int sock, char *buffer, unsigned bufferSize);
int writeSocket(int sock, char *buffer, unsigned bufferSize, struct sockaddr_in& toAddress);

int sendRTPOverTCP(int sock, char *buffer, int len, unsigned char streamChannelId);

void shutdown(int sock);

bool isMulticastAddress(unsigned int address);
bool socketJoinGroupSSM(int sock, unsigned int groupAddress, unsigned int sourceFilterAddr);
bool socketLeaveGroupSSM(int sock, unsigned int groupAddress, unsigned int sourceFilterAddr);
bool socketJoinGroup(int sock, unsigned int groupAddress);
bool socketLeaveGroup(int sock, unsigned int groupAddress);

unsigned int ourIPAddress();

extern unsigned int ReceivingInterfaceAddr;
#endif
