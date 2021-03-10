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
    ~DummyDecoder() override = default;

    // DecoderBase
    int         OpenFile(MythMediaBuffer* /*Buffer*/, bool /*novideo*/, TestBufferVec & /*testbuf*/) override
                    { return 0; }
    bool        GetFrame(DecodeType /*Type*/, bool &/*Retry*/) override
                    { usleep(10000); return false; }
    bool        IsLastFrameKey(void) const override       { return true; }
    void        WriteStoredData(MythMediaBuffer* /*Buffer*/, bool /*storevid*/,
                                std::chrono::milliseconds /*timecodeOffset*/) override {}
    long        UpdateStoredFrameNum(long /*frame*/) override { return 0; }
    QString     GetCodecDecoderName(void) const override  { return "dummy"; }
    MythCodecID GetVideoCodecID(void) const override      { return kCodec_NONE; }
    bool        SyncPositionMap(void) override            { return false; }
};

#endif
