#ifndef DUMMYDECODER_H_
#define DUMMYDECODER_H_

#include <qmap.h>
#include <qptrlist.h>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"

extern "C" {
#include "frame.h"
}

class DummyDecoder : public DecoderBase
{
  public:
    DummyDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo) : 
        DecoderBase(parent, pginfo) {}
   ~DummyDecoder() {}

    int OpenFile(RingBuffer *, bool, char *, int) { return 0; }
    bool GetFrame(int) { usleep(10000); return false; }

    bool isLastFrameKey(void) { return true; }
    void WriteStoredData(RingBuffer *, bool, long) {}

    long UpdateStoredFrameNum(long) { return 0; }

    QString GetEncodingType(void) const { return QString("MPEG-2"); }

    bool SyncPositionMap(void) { return false; }
};

#endif
