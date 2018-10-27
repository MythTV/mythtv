#ifndef DUMMYDECODER_H_
#define DUMMYDECODER_H_

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"
#include "mythframe.h"

class DummyDecoder : public DecoderBase
{
  public:
    DummyDecoder(MythPlayer *parent, const ProgramInfo &pginfo) :
        DecoderBase(parent, pginfo) {}
    virtual ~DummyDecoder() = default;

    int OpenFile(RingBuffer *, bool, char *, int) override // DecoderBase
        { return 0; }
    bool GetFrame(DecodeType)override // DecoderBase
        { usleep(10000); return false; }

    bool IsLastFrameKey(void) const override // DecoderBase
        { return true; }
    void WriteStoredData(RingBuffer *, bool, long) override {} // DecoderBase

    long UpdateStoredFrameNum(long) override { return 0; } // DecoderBase

    QString GetCodecDecoderName(void) const override // DecoderBase
        { return "dummy"; }
    MythCodecID GetVideoCodecID(void) const override // DecoderBase
        { return kCodec_NONE; }

    bool SyncPositionMap(void) override // DecoderBase
        { return false; }
};

#endif
