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

    // DecoderBase
    int         OpenFile(RingBuffer *, bool, char *, int) override { return 0; }
    bool        GetFrame(DecodeType, bool&) override      { usleep(10000); return false; }
    bool        IsLastFrameKey(void) const override       { return true; }
    void        WriteStoredData(RingBuffer *, bool, long) override {}
    long        UpdateStoredFrameNum(long) override       { return 0; }
    QString     GetCodecDecoderName(void) const override  { return "dummy"; }
    MythCodecID GetVideoCodecID(void) const override      { return kCodec_NONE; }
    bool        SyncPositionMap(void) override            { return false; }
};

#endif
