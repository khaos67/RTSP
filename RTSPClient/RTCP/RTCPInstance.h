#ifndef __RTCP_INSTANCE_H__
#define __RTCP_INSTANCE_H__

#include "NetCommon.h"
#include "RTSPCommon.h"
#include "OutPacketBuffer.h"

class RTPSource;
class RTPReceptionStats;

class SDESItem {
public:
  SDESItem(unsigned char tag, unsigned char const* value);

  unsigned char const* data() const {return fData;}
  unsigned totalSize() const;

private:
  unsigned char fData[2 + 0xFF]; // first 2 bytes are tag and length
};

class RTCPMemberDatabase; // forward

class RTCPInstance
{
public:
	RTCPInstance(unsigned totSessionBW, unsigned char const *cname, RTPSource *source);
	virtual ~RTCPInstance();

	void rtcpPacketHandler(char *buf, int len);

	unsigned numMembers() const;

	static void onExpire(RTCPInstance* instance);

private:
	void addRR();
	void enqueueCommonReportPrefix(unsigned char packetType, u_int32_t SSRC, unsigned numExtraWords = 0);
    void enqueueCommonReportSuffix();
    void enqueueReportBlock(RTPReceptionStats* receptionStats);
	void addSDES();

	void sendBuiltPacket();

	void onReceive(int typeOfPacket, int totPacketSize, u_int32_t ssrc);	
	void onExpire1();

private:
	unsigned fTotSessionBW;
	RTPSource *fSource;
	OutPacketBuffer* fOutBuf;

	SDESItem fCNAME;
	RTCPMemberDatabase* fKnownMembers;
	unsigned fOutgoingReportCount; // used for SSRC member aging

	double fAveRTCPSize;
	int fIsInitial;
	double fPrevReportTime;
	double fNextReportTime;
	int fPrevNumMembers;

	int fLastSentSize;
	int fLastReceivedSize;
	u_int32_t fLastReceivedSSRC;
	int fTypeOfEvent;
	int fTypeOfPacket;
	bool fHaveJustSentPacket;
	unsigned fLastPacketSentSize;

public:	// because this stuff is used by an external "C" function
	void sendReport();
	int typeOfEvent() {return fTypeOfEvent;}
	int sentPacketSize() {return fLastSentSize;}
	int packetType() {return fTypeOfPacket;}
	int receivedPacketSize() {return fLastReceivedSize;}
	int checkNewSSRC();
	void removeLastReceivedSSRC();
	void removeSSRC(u_int32_t ssrc, bool alsoRemoveStats);
};

// RTCP packet types:
const unsigned char RTCP_PT_SR = 200;
const unsigned char RTCP_PT_RR = 201;
const unsigned char RTCP_PT_SDES = 202;
const unsigned char RTCP_PT_BYE = 203;
const unsigned char RTCP_PT_APP = 204;

// SDES tags:
const unsigned char RTCP_SDES_END = 0;
const unsigned char RTCP_SDES_CNAME = 1;
const unsigned char RTCP_SDES_NAME = 2;
const unsigned char RTCP_SDES_EMAIL = 3;
const unsigned char RTCP_SDES_PHONE = 4;
const unsigned char RTCP_SDES_LOC = 5;
const unsigned char RTCP_SDES_TOOL = 6;
const unsigned char RTCP_SDES_NOTE = 7;
const unsigned char RTCP_SDES_PRIV = 8;

#endif
