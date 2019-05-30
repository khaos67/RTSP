OBJ_DIR = ../objs
INC = 	-I../Sock -I../OS_Common -I../Common -I../Util \
		-I../RTSPClient/RTSP -I../RTSPClient/RTP -I../RTSPClient/RTCP -I../RTSPClient/Common \
		-I../RTSPServer/RTSP -I../RTSPServer/Common
CXXFLAGS = -Wall -W -O2 -fPIC -g -DLINUX $(INC)

LIB_RTSP_CLIENT_SERVER = libRTSPClient.so libRTSPServer.so

TARGET = rtspclient rtspserver

all : makebuilddir $(TARGET)

makebuilddir:
	-@if [ ! -d $(OBJ_DIR)  ]; then mkdir -p $(OBJ_DIR); fi

$(TARGET) : $(LIB_RTSP_CLIENT_SERVER)
	g++ -o rtspclient $(CXXFLAGS) rtspclient.cpp -lRTSPClient -L./
	g++ -o rtspserver $(CXXFLAGS) rtspserver.cpp RTSPLiveStreamer.cpp -lRTSPServer -lRTSPClient -L./
	
clean : 
	rm -rf $(TARGET) $(LIB_RTSP_CLIENT_SERVER)

libRTSPClient.so :
		cd ../ && $(MAKE)
		cp -f $(OBJ_DIR)/libRTSPClient.so ./
libRTSPServer.so :
		cd ../ && $(MAKE)
		cp -f $(OBJ_DIR)/libRTSPServer.so ./
