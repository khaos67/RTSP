# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := RTSPStreamer
LOCAL_SRC_FILES := RTSPClientJNI.cpp
LOCAL_LDLIBS 	:= -llog

LOCAL_CFLAGS	:= -DANDROID
LOCAL_CPPFLAGS	:= -DANDROID
LOCAL_C_INCLUDES	:= $(LOCAL_PATH)/../../OS_Common \
					   $(LOCAL_PATH)/../../Common \
					   $(LOCAL_PATH)/../../Util \
					   $(LOCAL_PATH)/../../OS_Common \
					   $(LOCAL_PATH)/../../Sock \
					   $(LOCAL_PATH)/../../RTSPClient/Common \
					   $(LOCAL_PATH)/../../RTSPClient/RTCP \
					   $(LOCAL_PATH)/../../RTSPClient/RTP \
					   $(LOCAL_PATH)/../../RTSPClient/RTSP \
					   $(LOCAL_PATH)/../../RTSPServer/Common \
					   $(LOCAL_PATH)/../../RTSPServer/RTSP
					   
OS_COMMON_SRC_FILES	:= $(LOCAL_PATH)/../../OS_Common/Mutex.cpp \
					   $(LOCAL_PATH)/../../OS_Common/Thread.cpp	\
					   $(LOCAL_PATH)/../../OS_Common/Event.cpp \
					   $(LOCAL_PATH)/../../OS_Common/MySemaphore.cpp

COMMON_SRC_FILES	:= $(LOCAL_PATH)/../../Common/RTSPCommon.cpp \
					   $(LOCAL_PATH)/../../Common/RTSPCommonEnv.cpp
					   
RTSPCLIENT_COMMON_SRC_FILES	:= $(LOCAL_PATH)/../../RTSPClient/Common/DigestAuthentication.cpp \
							   $(LOCAL_PATH)/../../RTSPClient/Common/BitVector.cpp

RTSPSERVER_COMMON_SRC_FILES := $(LOCAL_PATH)/../../RTSPServer/Common/NetAddress.cpp

UTIL_SRC_FILES	:= $(LOCAL_PATH)/../../Util/our_md5hl.c \
				   $(LOCAL_PATH)/../../Util/our_md5.c \
				   $(LOCAL_PATH)/../../Util/util.cpp \
				   $(LOCAL_PATH)/../../Util/Base64.cpp
			   
SOCK_SRC_FILES	:= $(LOCAL_PATH)/../../Sock/SockCommon.cpp \
				   $(LOCAL_PATH)/../../Sock/MySock.cpp \
				   $(LOCAL_PATH)/../../Sock/TaskScheduler.cpp
			   
RTCP_SRC_FILES	:= $(LOCAL_PATH)/../../RTSPClient/RTCP/HashTable.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTCP/BasicHashTable.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTCP/OutPacketBuffer.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTCP/RTCP.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTCP/rtcp_from_spec.c \
				   $(LOCAL_PATH)/../../RTSPClient/RTCP/RTCPInstance.cpp
			   
RTP_SRC_FILES	:= $(LOCAL_PATH)/../../RTSPClient/RTP/RTPPacketBuffer.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTP/RTPSource.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTP/H264RTPSource.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTP/H265RTPSource.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTP/MPEG4ESRTPSource.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTP/JPEGRTPSource.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTP/AC3RTPSource.cpp	\
				   $(LOCAL_PATH)/../../RTSPClient/RTP/MPEG4GenericRTPSource.cpp	

RTSPCLIENT_SRC_FILES	:= $(LOCAL_PATH)/../../RTSPClient/RTSP/MediaSession.cpp \
				   $(LOCAL_PATH)/../../RTSPClient/RTSP/RTSPClient.cpp

RTSPSERVER_SRC_FILES	:= $(LOCAL_PATH)/../../RTSPServer/RTSP/ClientSocket.cpp \
				   $(LOCAL_PATH)/../../RTSPServer/RTSP/LiveServerMediaSession.cpp \
				   $(LOCAL_PATH)/../../RTSPServer/RTSP/OnDemandServerMediaSession.cpp \
				   $(LOCAL_PATH)/../../RTSPServer/RTSP/RTSPServer.cpp \
				   $(LOCAL_PATH)/../../RTSPServer/RTSP/ServerMediaSession.cpp
			   				   
LOCAL_SRC_FILES		+= $(OS_COMMON_SRC_FILES)					   
LOCAL_SRC_FILES		+= $(COMMON_SRC_FILES)
LOCAL_SRC_FILES		+= $(RTSPCLIENT_COMMON_SRC_FILES)
LOCAL_SRC_FILES		+= $(RTSPSERVER_COMMON_SRC_FILES)
LOCAL_SRC_FILES		+= $(UTIL_SRC_FILES)
LOCAL_SRC_FILES		+= $(SOCK_SRC_FILES)
LOCAL_SRC_FILES		+= $(RTCP_SRC_FILES)
LOCAL_SRC_FILES		+= $(RTP_SRC_FILES)
LOCAL_SRC_FILES		+= $(RTSPCLIENT_SRC_FILES)
LOCAL_SRC_FILES		+= $(RTSPSERVER_SRC_FILES)

include $(BUILD_SHARED_LIBRARY)
