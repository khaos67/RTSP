#ifndef __RTCP_H__
#define __RTCP_H__

#include "HashTable.hh"
#include "RTSPCommon.h"
#include "util.h"

class RTPReceptionStats; // forward

class RTPReceptionStatsDB {
public:
	unsigned totNumPacketsReceived() const { return fTotNumPacketsReceived; }
	unsigned numActiveSourcesSinceLastReset() const {
		return fNumActiveSourcesSinceLastReset;
	}

	void reset();
	// resets periodic stats (called each time they're used to
	// generate a reception report)

	class Iterator {
	public:
		Iterator(RTPReceptionStatsDB& receptionStatsDB);
		virtual ~Iterator();

		RTPReceptionStats* next(bool includeInactiveSources = false);
		// NULL if none

	private:
		HashTable::Iterator* fIter;
	};

	// The following is called whenever a RTP packet is received:
	void noteIncomingPacket(u_int32_t SSRC, u_int16_t seqNum,
		u_int32_t rtpTimestamp,
		unsigned timestampFrequency,
		bool useForJitterCalculation,
	struct timeval& resultPresentationTime,
		bool& resultHasBeenSyncedUsingRTCP,
		unsigned packetSize /* payload only */);

	// The following is called whenever a RTCP SR packet is received:
	void noteIncomingSR(u_int32_t SSRC,
		u_int32_t ntpTimestampMSW, u_int32_t ntpTimestampLSW,
		u_int32_t rtpTimestamp);

	// The following is called when a RTCP BYE packet is received:
	void removeRecord(u_int32_t SSRC);

	RTPReceptionStats* lookup(u_int32_t SSRC) const;

protected: // constructor and destructor, called only by RTPSource:
	friend class RTPSource;
	RTPReceptionStatsDB();
	virtual ~RTPReceptionStatsDB();

protected:
	void add(u_int32_t SSRC, RTPReceptionStats* stats);

protected:
	friend class Iterator;
	unsigned fNumActiveSourcesSinceLastReset;

private:
	HashTable* fTable;
	unsigned fTotNumPacketsReceived; // for all SSRCs
};

class RTPReceptionStats {
public:
	u_int32_t SSRC() const { return fSSRC; }
	unsigned numPacketsReceivedSinceLastReset() const {
		return fNumPacketsReceivedSinceLastReset;
	}
	unsigned totNumPacketsReceived() const { return fTotNumPacketsReceived; }
	double totNumKBytesReceived() const;

	unsigned totNumPacketsExpected() const {
		return fHighestExtSeqNumReceived - fBaseExtSeqNumReceived;
	}

	unsigned baseExtSeqNumReceived() const { return fBaseExtSeqNumReceived; }
	unsigned lastResetExtSeqNumReceived() const {
		return fLastResetExtSeqNumReceived;
	}
	unsigned highestExtSeqNumReceived() const {
		return fHighestExtSeqNumReceived;
	}

	unsigned jitter() const;

	unsigned lastReceivedSR_NTPmsw() const { return fLastReceivedSR_NTPmsw; }
	unsigned lastReceivedSR_NTPlsw() const { return fLastReceivedSR_NTPlsw; }
	struct timeval const& lastReceivedSR_time() const {
		return fLastReceivedSR_time;
	}

	unsigned minInterPacketGapUS() const { return fMinInterPacketGapUS; }
	unsigned maxInterPacketGapUS() const { return fMaxInterPacketGapUS; }
	struct timeval const& totalInterPacketGaps() const {
		return fTotalInterPacketGaps;
	}

protected:
	// called only by RTPReceptionStatsDB:
	friend class RTPReceptionStatsDB;
	RTPReceptionStats(u_int32_t SSRC, u_int16_t initialSeqNum);
	RTPReceptionStats(u_int32_t SSRC);
	virtual ~RTPReceptionStats();

private:
	void noteIncomingPacket(u_int16_t seqNum, u_int32_t rtpTimestamp,
		unsigned timestampFrequency,
		bool useForJitterCalculation,
	struct timeval& resultPresentationTime,
		bool& resultHasBeenSyncedUsingRTCP,
		unsigned packetSize /* payload only */);
	void noteIncomingSR(u_int32_t ntpTimestampMSW, u_int32_t ntpTimestampLSW,
		u_int32_t rtpTimestamp);
	void init(u_int32_t SSRC);
	void initSeqNum(u_int16_t initialSeqNum);
	void reset();
	// resets periodic stats (called each time they're used to
	// generate a reception report)

protected:
	u_int32_t fSSRC;
	unsigned fNumPacketsReceivedSinceLastReset;
	unsigned fTotNumPacketsReceived;
	u_int32_t fTotBytesReceived_hi, fTotBytesReceived_lo;
	bool fHaveSeenInitialSequenceNumber;
	unsigned fBaseExtSeqNumReceived;
	unsigned fLastResetExtSeqNumReceived;
	unsigned fHighestExtSeqNumReceived;
	int fLastTransit; // used in the jitter calculation
	u_int32_t fPreviousPacketRTPTimestamp;
	double fJitter;
	// The following are recorded whenever we receive a RTCP SR for this SSRC:
	unsigned fLastReceivedSR_NTPmsw; // NTP timestamp (from SR), most-signif
	unsigned fLastReceivedSR_NTPlsw; // NTP timestamp (from SR), least-signif
	struct timeval fLastReceivedSR_time;
	struct timeval fLastPacketReceptionTime;
	unsigned fMinInterPacketGapUS, fMaxInterPacketGapUS;
	struct timeval fTotalInterPacketGaps;

private:
	// Used to convert from RTP timestamp to 'wall clock' time:
	bool fHasBeenSynchronized;
	u_int32_t fSyncTimestamp;
	struct timeval fSyncTime;
};

#endif
