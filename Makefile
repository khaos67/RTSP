OBJ_DIR = ./objs
CC = gcc
CXX = g++
INC =	-I./Common -I./OS_Common -I./Sock -I./Util \
		-I./RTSPClient/Common -I./RTSPClient/RTCP -I./RTSPClient/RTP -I./RTSPClient/RTSP \
		-I./RTSPServer/Common -I./RTSPServer/RTSP
CFLAGS = -fno-stack-protector -Wall -W -O2 -fPIC -g -DLINUX $(INC)
CXXFLAGS = -fno-stack-protector -Wall -W -O2 -fPIC -g -DLINUX $(INC)
AR = ar
RANLIB = ranlib

RTSP_COMMON_OBJS = 	$(OBJ_DIR)/RTSPCommon.o \
						$(OBJ_DIR)/RTSPCommonEnv.o						
TARGET_RTSP_COMMON = $(OBJ_DIR)/lib_rtsp_common.a

OS_COMMON_OBJS =	$(OBJ_DIR)/Event.o \
					$(OBJ_DIR)/Mutex.o \
					$(OBJ_DIR)/MySemaphore.o \
					$(OBJ_DIR)/Thread.o
TARGET_OS_COMMON = $(OBJ_DIR)/lib_os_common.a

SOCK_OBJS =	$(OBJ_DIR)/SockCommon.o \
				$(OBJ_DIR)/MySock.o \
				$(OBJ_DIR)/TaskScheduler.o
TARGET_SOCK = $(OBJ_DIR)/lib_sock.a

UTIL_OBJS =	$(OBJ_DIR)/Base64.o \
			$(OBJ_DIR)/our_md5.o \
			$(OBJ_DIR)/our_md5hl.o \
			$(OBJ_DIR)/util.o
TARGET_UTIL = $(OBJ_DIR)/lib_util.a

RTSPCLIENT_COMMON_OBJS =	$(OBJ_DIR)/BitVector.o \
							$(OBJ_DIR)/DigestAuthentication.o
TARGET_RTSPCLIENT_COMMON = $(OBJ_DIR)/lib_rtspclient_common.a

RTSPCLIENT_RTCP_OBJS =	$(OBJ_DIR)/BasicHashTable.o \
						$(OBJ_DIR)/HashTable.o \
						$(OBJ_DIR)/OutPacketBuffer.o \
						$(OBJ_DIR)/RTCP.o \
						$(OBJ_DIR)/RTCPInstance.o \
						$(OBJ_DIR)/rtcp_from_spec.o
TARGET_RTSPCLIENT_RTCP = $(OBJ_DIR)/lib_rtspclient_rtcp.a

RTSPCLIENT_RTP_OBJS =	$(OBJ_DIR)/AC3RTPSource.o \
						$(OBJ_DIR)/H264RTPSource.o \
						$(OBJ_DIR)/H265RTPSource.o \
						$(OBJ_DIR)/JPEGRTPSource.o \
						$(OBJ_DIR)/MPEG4ESRTPSource.o \
						$(OBJ_DIR)/MPEG4GenericRTPSource.o \
						$(OBJ_DIR)/RTPPacketBuffer.o \
						$(OBJ_DIR)/RTPSource.o
TARGET_RTSPCLIENT_RTP = $(OBJ_DIR)/lib_rtspclient_rtp.a

RTSPCLIENT_RTSP_OBJS =	$(OBJ_DIR)/MediaSession.o \
						$(OBJ_DIR)/RTSPClient.o
TARGET_RTSPCLIENT_RTSP = $(OBJ_DIR)/lib_rtspclient_rtsp.a

LIB_ARCHIVE_RTSPCLIENT =	$(TARGET_RTSP_COMMON) \
							$(TARGET_OS_COMMON) \
							$(TARGET_SOCK) \
							$(TARGET_UTIL) \
							$(TARGET_RTSPCLIENT_COMMON) \
							$(TARGET_RTSPCLIENT_RTCP) \
							$(TARGET_RTSPCLIENT_RTP) \
							$(TARGET_RTSPCLIENT_RTSP)

RTSPSERVER_COMMON_OBJS =	$(OBJ_DIR)/NetAddress.o
TARGET_RTSPSERVER_COMMON = $(OBJ_DIR)/lib_rtspserver_common.a

RTSPSERVER_RTSP_OBJS =	$(OBJ_DIR)/ClientSocket.o \
						$(OBJ_DIR)/LiveServerMediaSession.o \
						$(OBJ_DIR)/OnDemandServerMediaSession.o \
						$(OBJ_DIR)/RTSPServer.o \
						$(OBJ_DIR)/ServerMediaSession.o			
TARGET_RTSPSERVER_RTSP = $(OBJ_DIR)/lib_rtspserver_rtsp.a

LIB_ARCHIVE_RTSPSERVER =	$(TARGET_RTSP_COMMON) \
							$(TARGET_OS_COMMON) \
							$(TARGET_SOCK) \
							$(TARGET_UTIL) \
							$(TARGET_RTSPSERVER_COMMON) \
							$(TARGET_RTSPSERVER_RTSP)

TARGET_RTSPCLIENT = $(OBJ_DIR)/libRTSPClient.so
TARGET_RTSPSERVER = $(OBJ_DIR)/libRTSPServer.so

TARGET = $(TARGET_RTSPCLIENT) $(TARGET_RTSPSERVER)

all : makebuilddir $(TARGET)

makebuilddir:
	-@if [ ! -d $(OBJ_DIR)  ]; then mkdir -p $(OBJ_DIR); fi
	
clean : 
	rm -rf $(OBJ_DIR)/*.o $(OBJ_DIR)/*.a $(OBJ_DIR)/*.so

$(TARGET_RTSPCLIENT) : $(LIB_ARCHIVE_RTSPCLIENT)
	g++ -shared -o $(TARGET_RTSPCLIENT) -pthread -Wl,--whole-archive $(LIB_ARCHIVE_RTSPCLIENT) -Wl,--no-whole-archive
$(TARGET_RTSPSERVER) : $(LIB_ARCHIVE_RTSPSERVER)
	g++ -shared -o $(TARGET_RTSPSERVER) -pthread -Wl,--whole-archive $(LIB_ARCHIVE_RTSPSERVER) -Wl,--no-whole-archive

$(TARGET_RTSP_COMMON) : $(RTSP_COMMON_OBJS)
	$(AR) rcv $(TARGET_RTSP_COMMON) $(RTSP_COMMON_OBJS)
	$(RANLIB) $(TARGET_RTSP_COMMON)

$(TARGET_OS_COMMON) : $(OS_COMMON_OBJS)
	$(AR) rcv $(TARGET_OS_COMMON) $(OS_COMMON_OBJS)
	$(RANLIB) $(TARGET_OS_COMMON)

$(TARGET_SOCK) : $(SOCK_OBJS)
	$(AR) rcv $(TARGET_SOCK) $(SOCK_OBJS)
	$(RANLIB) $(TARGET_SOCK)
	
$(TARGET_UTIL) : $(UTIL_OBJS)
	$(AR) rcv $(TARGET_UTIL) $(UTIL_OBJS)
	$(RANLIB) $(TARGET_UTIL)

$(TARGET_RTSPCLIENT_COMMON) : $(RTSPCLIENT_COMMON_OBJS)
	$(AR) rcv $(TARGET_RTSPCLIENT_COMMON) $(RTSPCLIENT_COMMON_OBJS)
	$(RANLIB) $(TARGET_RTSPCLIENT_COMMON)

$(TARGET_RTSPCLIENT_RTCP) : $(RTSPCLIENT_RTCP_OBJS)
	$(AR) rcv $(TARGET_RTSPCLIENT_RTCP) $(RTSPCLIENT_RTCP_OBJS)
	$(RANLIB) $(TARGET_RTSPCLIENT_RTCP)

$(TARGET_RTSPCLIENT_RTP) : $(RTSPCLIENT_RTP_OBJS)
	$(AR) rcv $(TARGET_RTSPCLIENT_RTP) $(RTSPCLIENT_RTP_OBJS)
	$(RANLIB) $(TARGET_RTSPCLIENT_RTP)

$(TARGET_RTSPCLIENT_RTSP) : $(RTSPCLIENT_RTSP_OBJS)
	$(AR) rcv $(TARGET_RTSPCLIENT_RTSP) $(RTSPCLIENT_RTSP_OBJS)
	$(RANLIB) $(TARGET_RTSPCLIENT_RTSP)

$(TARGET_RTSPSERVER_COMMON) : $(RTSPSERVER_COMMON_OBJS)
	$(AR) rcv $(TARGET_RTSPSERVER_COMMON) $(RTSPSERVER_COMMON_OBJS)
	$(RANLIB) $(TARGET_RTSPSERVER_COMMON)

$(TARGET_RTSPSERVER_RTSP) : $(RTSPSERVER_RTSP_OBJS)
	$(AR) rcv $(TARGET_RTSPSERVER_RTSP) $(RTSPSERVER_RTSP_OBJS)
	$(RANLIB) $(TARGET_RTSPSERVER_RTSP)

$(OBJ_DIR)/RTSPCommon.o : ./Common/RTSPCommon.cpp
	$(CXX) $(CXXFLAGS) -c ./Common/RTSPCommon.cpp -o $(OBJ_DIR)/RTSPCommon.o
$(OBJ_DIR)/RTSPCommonEnv.o : ./Common/RTSPCommonEnv.cpp
	$(CXX) $(CXXFLAGS) -c ./Common/RTSPCommonEnv.cpp -o $(OBJ_DIR)/RTSPCommonEnv.o

$(OBJ_DIR)/Event.o : ./OS_Common/Event.cpp
	$(CXX) $(CXXFLAGS) -c ./OS_Common/Event.cpp -o $(OBJ_DIR)/Event.o
$(OBJ_DIR)/Mutex.o : ./OS_Common/Mutex.cpp
	$(CXX) $(CXXFLAGS) -c ./OS_Common/Mutex.cpp -o $(OBJ_DIR)/Mutex.o
$(OBJ_DIR)/MySemaphore.o : ./OS_Common/MySemaphore.cpp
	$(CXX) $(CXXFLAGS) -c ./OS_Common/MySemaphore.cpp -o $(OBJ_DIR)/MySemaphore.o
$(OBJ_DIR)/Thread.o : ./OS_Common/Thread.cpp
	$(CXX) $(CXXFLAGS) -c ./OS_Common/Thread.cpp -o $(OBJ_DIR)/Thread.o

$(OBJ_DIR)/SockCommon.o : ./Sock/SockCommon.cpp
	$(CXX) $(CXXFLAGS) -c ./Sock/SockCommon.cpp -o $(OBJ_DIR)/SockCommon.o
$(OBJ_DIR)/MySock.o : ./Sock/MySock.cpp
	$(CXX) $(CXXFLAGS) -c ./Sock/MySock.cpp -o $(OBJ_DIR)/MySock.o
$(OBJ_DIR)/TaskScheduler.o : ./Sock/TaskScheduler.cpp
	$(CXX) $(CXXFLAGS) -c ./Sock/TaskScheduler.cpp -o $(OBJ_DIR)/TaskScheduler.o	

$(OBJ_DIR)/Base64.o : ./Util/Base64.cpp
	$(CXX) $(CXXFLAGS) -c ./Util/Base64.cpp -o $(OBJ_DIR)/Base64.o
$(OBJ_DIR)/our_md5.o : ./Util/our_md5.c
	$(CC) $(CFLAGS) -c ./Util/our_md5.c -o $(OBJ_DIR)/our_md5.o
$(OBJ_DIR)/our_md5hl.o : ./Util/our_md5hl.c
	$(CC) $(CFLAGS) -c ./Util/our_md5hl.c -o $(OBJ_DIR)/our_md5hl.o
$(OBJ_DIR)/util.o : ./Util/util.cpp
	$(CXX) $(CXXFLAGS) -c ./Util/util.cpp -o $(OBJ_DIR)/util.o

$(OBJ_DIR)/BitVector.o : ./RTSPClient/Common/BitVector.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/Common/BitVector.cpp -o $(OBJ_DIR)/BitVector.o
$(OBJ_DIR)/DigestAuthentication.o : ./RTSPClient/Common/DigestAuthentication.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/Common/DigestAuthentication.cpp -o $(OBJ_DIR)/DigestAuthentication.o

$(OBJ_DIR)/BasicHashTable.o : ./RTSPClient/RTCP/BasicHashTable.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTCP/BasicHashTable.cpp -o $(OBJ_DIR)/BasicHashTable.o
$(OBJ_DIR)/HashTable.o : ./RTSPClient/RTCP/HashTable.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTCP/HashTable.cpp -o $(OBJ_DIR)/HashTable.o
$(OBJ_DIR)/OutPacketBuffer.o : ./RTSPClient/RTCP/OutPacketBuffer.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTCP/OutPacketBuffer.cpp -o $(OBJ_DIR)/OutPacketBuffer.o	
$(OBJ_DIR)/RTCP.o : ./RTSPClient/RTCP/RTCP.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTCP/RTCP.cpp -o $(OBJ_DIR)/RTCP.o	
$(OBJ_DIR)/RTCPInstance.o : ./RTSPClient/RTCP/RTCPInstance.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTCP/RTCPInstance.cpp -o $(OBJ_DIR)/RTCPInstance.o	
$(OBJ_DIR)/rtcp_from_spec.o : ./RTSPClient/RTCP/rtcp_from_spec.c
	$(CC) $(CFLAGS) -c ./RTSPClient/RTCP/rtcp_from_spec.c -o $(OBJ_DIR)/rtcp_from_spec.o		

$(OBJ_DIR)/AC3RTPSource.o : ./RTSPClient/RTP/AC3RTPSource.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTP/AC3RTPSource.cpp -o $(OBJ_DIR)/AC3RTPSource.o
$(OBJ_DIR)/H264RTPSource.o : ./RTSPClient/RTP/H264RTPSource.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTP/H264RTPSource.cpp -o $(OBJ_DIR)/H264RTPSource.o
$(OBJ_DIR)/H265RTPSource.o : ./RTSPClient/RTP/H265RTPSource.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTP/H265RTPSource.cpp -o $(OBJ_DIR)/H265RTPSource.o
$(OBJ_DIR)/JPEGRTPSource.o : ./RTSPClient/RTP/JPEGRTPSource.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTP/JPEGRTPSource.cpp -o $(OBJ_DIR)/JPEGRTPSource.o	
$(OBJ_DIR)/MPEG4ESRTPSource.o : ./RTSPClient/RTP/MPEG4ESRTPSource.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTP/MPEG4ESRTPSource.cpp -o $(OBJ_DIR)/MPEG4ESRTPSource.o	
$(OBJ_DIR)/MPEG4GenericRTPSource.o : ./RTSPClient/RTP/MPEG4GenericRTPSource.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTP/MPEG4GenericRTPSource.cpp -o $(OBJ_DIR)/MPEG4GenericRTPSource.o
$(OBJ_DIR)/RTPPacketBuffer.o : ./RTSPClient/RTP/RTPPacketBuffer.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTP/RTPPacketBuffer.cpp -o $(OBJ_DIR)/RTPPacketBuffer.o
$(OBJ_DIR)/RTPSource.o : ./RTSPClient/RTP/RTPSource.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTP/RTPSource.cpp -o $(OBJ_DIR)/RTPSource.o

$(OBJ_DIR)/MediaSession.o : ./RTSPClient/RTSP/MediaSession.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTSP/MediaSession.cpp -o $(OBJ_DIR)/MediaSession.o
$(OBJ_DIR)/RTSPClient.o : ./RTSPClient/RTSP/RTSPClient.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPClient/RTSP/RTSPClient.cpp -o $(OBJ_DIR)/RTSPClient.o

$(OBJ_DIR)/NetAddress.o : ./RTSPServer/Common/NetAddress.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPServer/Common/NetAddress.cpp -o $(OBJ_DIR)/NetAddress.o

$(OBJ_DIR)/ClientSocket.o : ./RTSPServer/RTSP/ClientSocket.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPServer/RTSP/ClientSocket.cpp -o $(OBJ_DIR)/ClientSocket.o
$(OBJ_DIR)/LiveServerMediaSession.o : ./RTSPServer/RTSP/LiveServerMediaSession.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPServer/RTSP/LiveServerMediaSession.cpp -o $(OBJ_DIR)/LiveServerMediaSession.o
$(OBJ_DIR)/OnDemandServerMediaSession.o : ./RTSPServer/RTSP/OnDemandServerMediaSession.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPServer/RTSP/OnDemandServerMediaSession.cpp -o $(OBJ_DIR)/OnDemandServerMediaSession.o
$(OBJ_DIR)/RTSPServer.o : ./RTSPServer/RTSP/RTSPServer.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPServer/RTSP/RTSPServer.cpp -o $(OBJ_DIR)/RTSPServer.o
$(OBJ_DIR)/ServerMediaSession.o : ./RTSPServer/RTSP/ServerMediaSession.cpp
	$(CXX) $(CXXFLAGS) -c ./RTSPServer/RTSP/ServerMediaSession.cpp -o $(OBJ_DIR)/ServerMediaSession.o

