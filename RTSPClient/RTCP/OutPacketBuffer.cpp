#include "OutPacketBuffer.h"
#include "NetCommon.h"

unsigned OutPacketBuffer::maxSize = 60000; // by default

OutPacketBuffer::OutPacketBuffer(unsigned preferredPacketSize,
				 unsigned maxPacketSize)
  : fPreferred(preferredPacketSize), fMax(maxPacketSize),
    fOverflowDataSize(0) {
  unsigned maxNumPackets = (maxSize + (maxPacketSize-1))/maxPacketSize;
  fLimit = maxNumPackets*maxPacketSize;
  fBuf = new unsigned char[fLimit];
  resetPacketStart();
  resetOffset();
  resetOverflowData();
}

OutPacketBuffer::~OutPacketBuffer() {
  delete[] fBuf;
}

void OutPacketBuffer::enqueue(unsigned char const* from, unsigned numBytes) {
  if (numBytes > totalBytesAvailable()) {
#ifdef DEBUG
    fprintf(stderr, "OutPacketBuffer::enqueue() warning: %d > %d\n", numBytes, totalBytesAvailable());
#endif
    numBytes = totalBytesAvailable();
  }

  if (curPtr() != from) memmove(curPtr(), from, numBytes);
  increment(numBytes);
}

void OutPacketBuffer::enqueueWord(unsigned word) {
  unsigned nWord = htonl(word);
  enqueue((unsigned char*)&nWord, 4);
}

void OutPacketBuffer::insert(unsigned char const* from, unsigned numBytes,
			     unsigned toPosition) {
  unsigned realToPosition = fPacketStart + toPosition;
  if (realToPosition + numBytes > fLimit) {
    if (realToPosition > fLimit) return; // we can't do this
    numBytes = fLimit - realToPosition;
  }

  memmove(&fBuf[realToPosition], from, numBytes);
  if (toPosition + numBytes > fCurOffset) {
    fCurOffset = toPosition + numBytes;
  }
}

void OutPacketBuffer::insertWord(unsigned word, unsigned toPosition) {
  unsigned nWord = htonl(word);
  insert((unsigned char*)&nWord, 4, toPosition);
}

void OutPacketBuffer::extract(unsigned char* to, unsigned numBytes,
			      unsigned fromPosition) {
  unsigned realFromPosition = fPacketStart + fromPosition;
  if (realFromPosition + numBytes > fLimit) { // sanity check
    if (realFromPosition > fLimit) return; // we can't do this
    numBytes = fLimit - realFromPosition;
  }

  memmove(to, &fBuf[realFromPosition], numBytes);
}

unsigned OutPacketBuffer::extractWord(unsigned fromPosition) {
  unsigned nWord;
  extract((unsigned char*)&nWord, 4, fromPosition);
  return ntohl(nWord);
}

void OutPacketBuffer::skipBytes(unsigned numBytes) {
  if (numBytes > totalBytesAvailable()) {
    numBytes = totalBytesAvailable();
  }

  increment(numBytes);
}

void OutPacketBuffer
::setOverflowData(unsigned overflowDataOffset,
		  unsigned overflowDataSize,
		  struct timeval const& presentationTime,
		  unsigned durationInMicroseconds) {
  fOverflowDataOffset = overflowDataOffset;
  fOverflowDataSize = overflowDataSize;
  fOverflowPresentationTime = presentationTime;
  fOverflowDurationInMicroseconds = durationInMicroseconds;
}

void OutPacketBuffer::useOverflowData() {
  enqueue(&fBuf[fPacketStart + fOverflowDataOffset], fOverflowDataSize);
  fCurOffset -= fOverflowDataSize; // undoes increment performed by "enqueue"
  resetOverflowData();
}

void OutPacketBuffer::adjustPacketStart(unsigned numBytes) {
  fPacketStart += numBytes;
  if (fOverflowDataOffset >= numBytes) {
    fOverflowDataOffset -= numBytes;
  } else {
    fOverflowDataOffset = 0;
    fOverflowDataSize = 0; // an error otherwise
  }
}

void OutPacketBuffer::resetPacketStart() {
  if (fOverflowDataSize > 0) {
    fOverflowDataOffset += fPacketStart;
  }
  fPacketStart = 0;
}
