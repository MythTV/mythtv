// A filter that passes through (unchanged) chunks that contain an integral number
// of MPEG-2 Transport Stream packets, but returning (in "fDurationInMicroseconds")
// an updated estimate of the time gap between chunks.
// C++ header

#ifndef _MPEG2_TRANSPORT_STREAM_FRAMER_HH
#define _MPEG2_TRANSPORT_STREAM_FRAMER_HH

#ifndef _FRAMED_FILTER_HH
#include "FramedFilter.hh"
#endif

#ifndef _HASH_TABLE_HH
#include "HashTable.hh"
#endif

class MPEG2TransportStreamFramer: public FramedFilter {
public:
  static MPEG2TransportStreamFramer*
  createNew(UsageEnvironment& env, FramedSource* inputSource);

  unsigned long tsPacketCount() const { return fTSPacketCount; }

protected:
  MPEG2TransportStreamFramer(UsageEnvironment& env, FramedSource* inputSource);
      // called only by createNew()
  virtual ~MPEG2TransportStreamFramer();

private:
  // Redefined virtual functions:
  virtual void doGetNextFrame();
  virtual void doStopGettingFrames();

private:
  static void afterGettingFrame(void* clientData, unsigned frameSize,
				unsigned numTruncatedBytes,
				struct timeval presentationTime,
				unsigned durationInMicroseconds);
  void afterGettingFrame1(unsigned frameSize,
			  struct timeval presentationTime);

  void updateTSPacketDurationEstimate(unsigned char* pkt, double timeNow);

private:
  unsigned long fTSPacketCount;
  double fTSPacketDurationEstimate;
  HashTable* fPIDStatusTable;
};

#endif
