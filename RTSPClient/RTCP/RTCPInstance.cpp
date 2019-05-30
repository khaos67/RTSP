#include "RTPSource.h"
#include "RTCPInstance.h"
#include "util.h"
#include "rtcp_from_spec.h"
#include "RTSPCommonEnv.h"

////////// RTCPMemberDatabase //////////

class RTCPMemberDatabase {
public:
  RTCPMemberDatabase(RTCPInstance& ourRTCPInstance)
    : fOurRTCPInstance(ourRTCPInstance), fNumMembers(1 /*ourself*/),
      fTable(HashTable::create(ONE_WORD_HASH_KEYS)) {
  }

  virtual ~RTCPMemberDatabase() {
	delete fTable;
  }

  bool isMember(unsigned ssrc) const {
    return fTable->Lookup((char*)(long)ssrc) != NULL;
  }

  bool noteMembership(unsigned ssrc, unsigned curTimeCount) {
    bool isNew = !isMember(ssrc);

    if (isNew) {
      ++fNumMembers;
    }

    // Record the current time, so we can age stale members
    fTable->Add((char*)(long)ssrc, (void*)(long)curTimeCount);

    return isNew;
  }

  bool remove(unsigned ssrc) {
    bool wasPresent = fTable->Remove((char*)(long)ssrc);
    if (wasPresent) {
      --fNumMembers;
    }
    return wasPresent;
  }

  unsigned numMembers() const {
    return fNumMembers;
  }

  void reapOldMembers(unsigned threshold);

private:
  RTCPInstance& fOurRTCPInstance;
  unsigned fNumMembers;
  HashTable* fTable;
};

void RTCPMemberDatabase::reapOldMembers(unsigned threshold) {
  bool foundOldMember;
  unsigned oldSSRC = 0;

  do {
    foundOldMember = false;

    HashTable::Iterator* iter
      = HashTable::Iterator::create(*fTable);
    unsigned long timeCount;
    char const* key;
    while ((timeCount = (unsigned long)(iter->next(key))) != 0) {
#ifdef DEBUG
      DPRINTF("reap: checking SSRC 0x%lx: %ld (threshold %d)\n", (unsigned long)key, timeCount, threshold);
#endif
      if (timeCount < (unsigned long)threshold) { // this SSRC is old
        unsigned long ssrc = (unsigned long)key;
        oldSSRC = (unsigned)ssrc;
        foundOldMember = true;
      }
    }
    delete iter;

    if (foundOldMember) {
#ifdef DEBUG
        DPRINTF("reap: removing SSRC 0x%x\n", oldSSRC);
#endif
      fOurRTCPInstance.removeSSRC(oldSSRC, true);
    }
  } while (foundOldMember);
}

////////// RTCPInstance //////////

static double dTimeNow() {
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);
    return (double) (timeNow.tv_sec + timeNow.tv_usec/1000000.0);
}

static unsigned const maxPacketSize = 1450;
	// bytes (1500, minus some allowance for IP, UDP, UMTP headers)
static unsigned const preferredPacketSize = 1000; // bytes

RTCPInstance::RTCPInstance(
			   unsigned totSessionBW,
			   unsigned char const* cname,
			   RTPSource *source)
  : fTotSessionBW(totSessionBW),
    fSource(source),
    fCNAME(RTCP_SDES_CNAME, cname), fOutgoingReportCount(1),
    fAveRTCPSize(0), fIsInitial(1), fPrevNumMembers(0),
    fLastSentSize(0), fLastReceivedSize(0), fLastReceivedSSRC(0),
    fTypeOfEvent(EVENT_UNKNOWN), fTypeOfPacket(PACKET_UNKNOWN_TYPE),
    fHaveJustSentPacket(false), fLastPacketSentSize(0)
{
#ifdef DEBUG
	DPRINTF("RTCPInstance[%p]::RTCPInstance()\n", this);
#endif
	if (fTotSessionBW == 0) { // not allowed!
		DPRINTF("RTCPInstance::RTCPInstance error: totSessionBW parameter should not be zero!\n");
		fTotSessionBW = 1;
	}

	double timeNow = dTimeNow();
	fPrevReportTime = fNextReportTime = timeNow;

	fKnownMembers = new RTCPMemberDatabase(*this);
	if (fKnownMembers == NULL) return;

	// A hack to save buffer space, because RTCP packets are always small:
	unsigned savedMaxSize = OutPacketBuffer::maxSize;
	OutPacketBuffer::maxSize = maxPacketSize;
	fOutBuf = new OutPacketBuffer(preferredPacketSize, maxPacketSize);
	OutPacketBuffer::maxSize = savedMaxSize;
	if (fOutBuf == NULL) return;

	// Send our first report.
	fTypeOfEvent = EVENT_REPORT;
	onExpire(this);
}

RTCPInstance::~RTCPInstance()
{
	delete fKnownMembers;
	delete fOutBuf;
}

unsigned RTCPInstance::numMembers() const 
{
	if (fKnownMembers == NULL) return 0;

	return fKnownMembers->numMembers();
}

int RTCPInstance::checkNewSSRC() 
{
	return fKnownMembers->noteMembership(fLastReceivedSSRC,
		fOutgoingReportCount);
}

void RTCPInstance::removeLastReceivedSSRC() 
{
	removeSSRC(fLastReceivedSSRC, false/*keep stats around*/);
}

void RTCPInstance::removeSSRC(u_int32_t ssrc, bool alsoRemoveStats) 
{
	fKnownMembers->remove(ssrc);

	if (alsoRemoveStats) {
		// Also, remove records of this SSRC from any reception or transmission stats
		if (fSource != NULL) fSource->receptionStatsDB().removeRecord(ssrc);
	}
}

void RTCPInstance::onExpire(RTCPInstance* instance) 
{
	instance->onExpire1();
}

void RTCPInstance::onExpire1() 
{
	// Note: fTotSessionBW is kbits per second
	double rtcpBW = 0.05*fTotSessionBW*1024/8; // -> bytes per second

	OnExpire(this, // event
		numMembers(), // members
		0, // senders
		rtcpBW, // rtcp_bw
		0, // we_sent
		&fAveRTCPSize, // ave_rtcp_size
		&fIsInitial, // initial
		dTimeNow(), // tc
		&fPrevReportTime, // tp
		&fPrevNumMembers // pmembers
		);
}

static unsigned const IP_UDP_HDR_SIZE = 28;
    // overhead (bytes) of IP and UDP hdrs

#define ADVANCE(n) pkt += (n); packetSize -= (n)

void RTCPInstance::rtcpPacketHandler(char *buf, int len)
{
	unsigned char *pkt = (unsigned char *)buf;
	unsigned packetSize = len;
	int typeOfPacket = PACKET_UNKNOWN_TYPE;

	do {
		int totPacketSize = IP_UDP_HDR_SIZE + packetSize;

		// Check the RTCP packet for validity:
		// It must at least contain a header (4 bytes), and this header
		// must be version=2, with no padding bit, and a payload type of
		// SR (200) or RR (201):
		if (packetSize < 4) break;
		unsigned rtcpHdr = ntohl(*(unsigned*)pkt);
		if ((rtcpHdr & 0xE0FE0000) != (0x80000000 | (RTCP_PT_SR<<16))) {
			DPRINTF("rejected bad RTCP packet: header 0x%08x\n", rtcpHdr);
			break;
		}

		// Process each of the individual RTCP 'subpackets' in (what may be)
		// a compound RTCP packet.
		unsigned reportSenderSSRC = 0;
		bool packetOK = false;
		while (1) {
			unsigned rc = (rtcpHdr>>24)&0x1F;
			unsigned pt = (rtcpHdr>>16)&0xFF;
			unsigned length = 4*(rtcpHdr&0xFFFF); // doesn't count hdr
			ADVANCE(4); // skip over the header
			if (length > packetSize) break;

			// Assume that each RTCP subpacket begins with a 4-byte SSRC:
			if (length < 4) break; length -= 4;
			reportSenderSSRC = ntohl(*(unsigned*)pkt); ADVANCE(4);

			bool subPacketOK = false;
			switch (pt) {
				case RTCP_PT_SR: {
#ifdef DEBUG
					DPRINTF("SR\n");
#endif
					if (length < 20) break; length -= 20;

					// Extract the NTP timestamp, and note this:
					unsigned NTPmsw = ntohl(*(unsigned*)pkt); ADVANCE(4);
					unsigned NTPlsw = ntohl(*(unsigned*)pkt); ADVANCE(4);
					unsigned rtpTimestamp = ntohl(*(unsigned*)pkt); ADVANCE(4);
					if (fSource != NULL) {
						RTPReceptionStatsDB& receptionStats
							= fSource->receptionStatsDB();
						receptionStats.noteIncomingSR(reportSenderSSRC,
							NTPmsw, NTPlsw, rtpTimestamp);
					}
					ADVANCE(8); // skip over packet count, octet count

					// If a 'SR handler' was set, call it now:
					//if (fSRHandlerTask != NULL) (*fSRHandlerTask)(fSRHandlerClientData);

					// The rest of the SR is handled like a RR (so, no "break;" here)
								 }
				case RTCP_PT_RR: {
#ifdef DEBUG
					DPRINTF("RR\n");
#endif
					unsigned reportBlocksSize = rc*(6*4);
					if (length < reportBlocksSize) break;
					length -= reportBlocksSize;

					ADVANCE(reportBlocksSize);

					if (pt == RTCP_PT_RR) { // i.e., we didn't fall through from 'SR'
					}

					subPacketOK = true;
					typeOfPacket = PACKET_RTCP_REPORT;
					break;
								 }
				case RTCP_PT_BYE: {
#ifdef DEBUG
					DPRINTF("BYE\n");
#endif
					// If a 'BYE handler' was set, call it now:

					// We should really check for & handle >1 SSRCs being present #####

					subPacketOK = true;
					typeOfPacket = PACKET_BYE;
					break;
								  }
				default: 
#ifdef DEBUG
					DPRINTF("UNSUPPORTED TYPE(0x%x)\n", pt);
#endif
					subPacketOK = true;
					break;						 
			}
			if (!subPacketOK) break;

			// need to check for (& handle) SSRC collision! #####

#ifdef DEBUG
			DPRINTF("validated RTCP subpacket (type %d): %d, %d, %d, 0x%08x\n", typeOfPacket, rc, pt, length, reportSenderSSRC);
#endif

			// Skip over any remaining bytes in this subpacket:
			ADVANCE(length);

			// Check whether another RTCP 'subpacket' follows:
			if (packetSize == 0) {
				packetOK = true;
				break;
			} else if (packetSize < 4) {
#ifdef DEBUG
				DPRINTF("extraneous %d bytes at end of RTCP packet!\n", packetSize);
#endif
				break;
			}
			rtcpHdr = ntohl(*(unsigned*)pkt);
			if ((rtcpHdr & 0xC0000000) != 0x80000000) {
#ifdef DEBUG
				DPRINTF("bad RTCP subpacket: header 0x%08x\n", rtcpHdr);
#endif
				break;
			}
		}

		if (!packetOK) {
#ifdef DEBUG
			DPRINTF("rejected bad RTCP subpacket: header 0x%08x\n", rtcpHdr);
#endif
			break;
		} else {
#ifdef DEBUG
			DPRINTF("validated entire RTCP packet\n");
#endif
		}

		onReceive(typeOfPacket, totPacketSize, reportSenderSSRC);
	} while (0);
}

void RTCPInstance::onReceive(int typeOfPacket, int totPacketSize,
			     unsigned ssrc) 
{
	fTypeOfPacket = typeOfPacket;
	fLastReceivedSize = totPacketSize;
	fLastReceivedSSRC = ssrc;

	int members = (int)numMembers();
	int senders = 0;

	OnReceive(this, // p
		this, // e
		&members, // members
		&fPrevNumMembers, // pmembers
		&senders, // senders
		&fAveRTCPSize, // avg_rtcp_size
		&fPrevReportTime, // tp
		dTimeNow(), // tc
		fNextReportTime);
}

void RTCPInstance::addRR()
{	
	enqueueCommonReportPrefix(RTCP_PT_RR, fSource->SSRC());
	enqueueCommonReportSuffix();
}

void RTCPInstance::enqueueCommonReportPrefix(unsigned char packetType,
					     unsigned SSRC,
					     unsigned numExtraWords) 
{
	unsigned numReportingSources;
	if (fSource == NULL) {
		numReportingSources = 0; // we don't receive anything
	} else {
		RTPReceptionStatsDB& allReceptionStats
			= fSource->receptionStatsDB();
		numReportingSources = allReceptionStats.numActiveSourcesSinceLastReset();
		// This must be <32, to fit in 5 bits:
		if (numReportingSources >= 32) { numReportingSources = 32; }
		// Later: support adding more reports to handle >32 sources (unlikely)#####
	}

	unsigned rtcpHdr = 0x80000000; // version 2, no padding
	rtcpHdr |= (numReportingSources<<24);
	rtcpHdr |= (packetType<<16);
	rtcpHdr |= (1 + numExtraWords + 6*numReportingSources);
	// each report block is 6 32-bit words long
	fOutBuf->enqueueWord(rtcpHdr);

	fOutBuf->enqueueWord(SSRC);
}

void RTCPInstance::enqueueCommonReportSuffix() 
{
	// Output the report blocks for each source:
	if (fSource != NULL) {
		RTPReceptionStatsDB& allReceptionStats
			= fSource->receptionStatsDB();

		RTPReceptionStatsDB::Iterator iterator(allReceptionStats);
		while (1) {
			RTPReceptionStats* receptionStats = iterator.next();
			if (receptionStats == NULL) break;
			enqueueReportBlock(receptionStats);
		}

		allReceptionStats.reset(); // because we have just generated a report
	}
}

void RTCPInstance::enqueueReportBlock(RTPReceptionStats* stats) 
{
	fOutBuf->enqueueWord(stats->SSRC());

	unsigned highestExtSeqNumReceived = stats->highestExtSeqNumReceived();

	unsigned totNumExpected
		= highestExtSeqNumReceived - stats->baseExtSeqNumReceived();
	int totNumLost = totNumExpected - stats->totNumPacketsReceived();
	// 'Clamp' this loss number to a 24-bit signed value:
	if (totNumLost > 0x007FFFFF) {
		totNumLost = 0x007FFFFF;
	} else if (totNumLost < 0) {
		if (totNumLost < -0x00800000) totNumLost = 0x00800000; // unlikely, but...
		totNumLost &= 0x00FFFFFF;
	}

	unsigned numExpectedSinceLastReset
		= highestExtSeqNumReceived - stats->lastResetExtSeqNumReceived();
	int numLostSinceLastReset
		= numExpectedSinceLastReset - stats->numPacketsReceivedSinceLastReset();
	unsigned char lossFraction;
	if (numExpectedSinceLastReset == 0 || numLostSinceLastReset < 0) {
		lossFraction = 0;
	} else {
		lossFraction = (unsigned char)
			((numLostSinceLastReset << 8) / numExpectedSinceLastReset);
	}

	fOutBuf->enqueueWord((lossFraction<<24) | totNumLost);
	fOutBuf->enqueueWord(highestExtSeqNumReceived);

	fOutBuf->enqueueWord(stats->jitter());

	unsigned NTPmsw = stats->lastReceivedSR_NTPmsw();
	unsigned NTPlsw = stats->lastReceivedSR_NTPlsw();
	unsigned LSR = ((NTPmsw&0xFFFF)<<16)|(NTPlsw>>16); // middle 32 bits
	fOutBuf->enqueueWord(LSR);

	// Figure out how long has elapsed since the last SR rcvd from this src:
	struct timeval const& LSRtime = stats->lastReceivedSR_time(); // "last SR"
	struct timeval timeNow, timeSinceLSR;
	gettimeofday(&timeNow, NULL);
	if (timeNow.tv_usec < LSRtime.tv_usec) {
		timeNow.tv_usec += 1000000;
		timeNow.tv_sec -= 1;
	}
	timeSinceLSR.tv_sec = timeNow.tv_sec - LSRtime.tv_sec;
	timeSinceLSR.tv_usec = timeNow.tv_usec - LSRtime.tv_usec;
	// The enqueued time is in units of 1/65536 seconds.
	// (Note that 65536/1000000 == 1024/15625)
	unsigned DLSR;
	if (LSR == 0) {
		DLSR = 0;
	} else {
		DLSR = (timeSinceLSR.tv_sec<<16)
			| ( (((timeSinceLSR.tv_usec<<11)+15625)/31250) & 0xFFFF);
	}
	fOutBuf->enqueueWord(DLSR);
}

void RTCPInstance::addSDES() 
{
	// For now we support only the CNAME item; later support more #####

	// Begin by figuring out the size of the entire SDES report:
	unsigned numBytes = 4;
	// counts the SSRC, but not the header; it'll get subtracted out
	numBytes += fCNAME.totalSize(); // includes id and length
	numBytes += 1; // the special END item

	unsigned num4ByteWords = (numBytes + 3)/4;

	unsigned rtcpHdr = 0x81000000; // version 2, no padding, 1 SSRC chunk
	rtcpHdr |= (RTCP_PT_SDES<<16);
	rtcpHdr |= num4ByteWords;
	fOutBuf->enqueueWord(rtcpHdr);

	if (fSource != NULL) {
		fOutBuf->enqueueWord(fSource->SSRC());
	} 

	// Add the CNAME:
	fOutBuf->enqueue(fCNAME.data(), fCNAME.totalSize());

	// Add the 'END' item (i.e., a zero byte), plus any more needed to pad:
	unsigned numPaddingBytesNeeded = 4 - (fOutBuf->curPacketSize() % 4);
	unsigned char const zero = '\0';
	while (numPaddingBytesNeeded-- > 0) fOutBuf->enqueue(&zero, 1);
}

void RTCPInstance::sendReport()
{
#ifdef DEBUG
	DPRINTF("sending REPORT\n");
#endif

	addRR();
	addSDES();

	sendBuiltPacket();
	
	// Periodically clean out old members from our SSRC membership database:
	const unsigned membershipReapPeriod = 5;
	if ((++fOutgoingReportCount) % membershipReapPeriod == 0) {
		unsigned threshold = fOutgoingReportCount - membershipReapPeriod;
		fKnownMembers->reapOldMembers(threshold);
	}
}

void RTCPInstance::sendBuiltPacket() 
{
#ifdef DEBUG
	DPRINTF("sending RTCP packet\n");
	unsigned char* p = fOutBuf->packet();
	for (unsigned i = 0; i < fOutBuf->curPacketSize(); ++i) {
		if (i%4 == 0) DPRINTF(" ");
		DPRINTF("%02x", p[i]);
	}
	DPRINTF("\n");
#endif
	unsigned reportSize = fOutBuf->curPacketSize();
	fSource->sendRtcpReport((char *)fOutBuf->packet(), reportSize);
	fOutBuf->resetOffset();

	fLastSentSize = IP_UDP_HDR_SIZE + reportSize;
	fHaveJustSentPacket = true;
	fLastPacketSentSize = reportSize;
}


////////// SDESItem //////////

SDESItem::SDESItem(unsigned char tag, unsigned char const* value) {
	unsigned length = strlen((char const*)value);
	if (length > 511) length = 511;

	fData[0] = tag;
	fData[1] = (unsigned char)length;
	memmove(&fData[2], value, length);

	// Pad the trailing bytes to a 4-byte boundary:
	while ((length)%4 > 0) fData[2 + length++] = '\0';
}

unsigned SDESItem::totalSize() const {
	return 2 + (unsigned)fData[1];
}

////////// Implementation of routines imported by the "rtcp_from_spec" C code

extern "C" void Schedule(double nextTime, event e) {
  RTCPInstance* instance = (RTCPInstance*)e;
  if (instance == NULL) return;

  //instance->schedule(nextTime);
}

extern "C" void Reschedule(double nextTime, event e) {
  RTCPInstance* instance = (RTCPInstance*)e;
  if (instance == NULL) return;

  //instance->reschedule(nextTime);
}

extern "C" void SendRTCPReport(event e) {
  RTCPInstance* instance = (RTCPInstance*)e;
  if (instance == NULL) return;

  instance->sendReport();
}

extern "C" void SendBYEPacket(event e) {
  RTCPInstance* instance = (RTCPInstance*)e;
  if (instance == NULL) return;

  //instance->sendBYE();
}

extern "C" int TypeOfEvent(event e) {
  RTCPInstance* instance = (RTCPInstance*)e;
  if (instance == NULL) return EVENT_UNKNOWN;

  return instance->typeOfEvent();
}

extern "C" int SentPacketSize(event e) {
  RTCPInstance* instance = (RTCPInstance*)e;
  if (instance == NULL) return 0;

  return instance->sentPacketSize();
}

extern "C" int PacketType(packet p) {
  RTCPInstance* instance = (RTCPInstance*)p;
  if (instance == NULL) return PACKET_UNKNOWN_TYPE;

  return instance->packetType();
}

extern "C" int ReceivedPacketSize(packet p) {
  RTCPInstance* instance = (RTCPInstance*)p;
  if (instance == NULL) return 0;

  return instance->receivedPacketSize();
}

extern "C" int NewMember(packet p) {
  RTCPInstance* instance = (RTCPInstance*)p;
  if (instance == NULL) return 0;

  return instance->checkNewSSRC();
}

extern "C" int NewSender(packet /*p*/) {
  return 0; // we don't yet recognize senders other than ourselves #####
}

extern "C" void AddMember(packet /*p*/) {
  // Do nothing; all of the real work was done when NewMember() was called
}

extern "C" void AddSender(packet /*p*/) {
  // we don't yet recognize senders other than ourselves #####
}

extern "C" void RemoveMember(packet p) {
  RTCPInstance* instance = (RTCPInstance*)p;
  if (instance == NULL) return;

  instance->removeLastReceivedSSRC();
}

extern "C" void RemoveSender(packet /*p*/) {
  // we don't yet recognize senders other than ourselves #####
}

extern "C" double drand30() {
  unsigned tmp = rand()&0x3FFFFFFF; // a random 30-bit integer
  return tmp/(double)(1024*1024*1024);
}
