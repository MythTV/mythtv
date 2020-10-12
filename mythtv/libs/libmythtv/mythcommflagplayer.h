#ifndef MYTHCOMMFLAGPLAYER_H
#define MYTHCOMMFLAGPLAYER_H

// MythTV
#include "mythplayer.h"

class MythRebuildSaver : public QRunnable
{
  public:
    MythRebuildSaver(DecoderBase* Decoder, uint64_t First, uint64_t Last);
    void        run      () override;
    static uint GetCount (DecoderBase* Decoder);
    static void Wait     (DecoderBase* Decoder);

  private:
    DecoderBase *m_decoder { nullptr };
    uint64_t     m_first   { 0 };
    uint64_t     m_last    { 0 };

    static QMutex                   s_lock;
    static QWaitCondition           s_wait;
    static QHash<DecoderBase*,uint> s_count;
};

class MTV_PUBLIC MythCommFlagPlayer : public MythPlayer
{
  public:
    explicit MythCommFlagPlayer(PlayerContext* Context, PlayerFlags Flags = kNoFlags);
    bool RebuildSeekTable(bool ShowPercentage = true, StatusCallback Callback = nullptr, void* Opaque = nullptr);
    MythVideoFrame* GetRawVideoFrame(long long FrameNumber = -1);
};

#endif
