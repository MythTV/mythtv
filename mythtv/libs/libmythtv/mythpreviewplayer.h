#ifndef MYTHPREVIEWPLAYER_H
#define MYTHPREVIEWPLAYER_H

// MythTV
#include "mythplayer.h"

class MythPreviewPlayer : public MythPlayer
{
  public:
    explicit MythPreviewPlayer(PlayerContext* Context, PlayerFlags Flags = kNoFlags);
    char* GetScreenGrabAtFrame(uint64_t FrameNum, bool Absolute, int& BufferSize,
                               int& FrameWidth, int& FrameHeight, float& AspectRatio);
    char* GetScreenGrab       (std::chrono::seconds SecondsIn, int& BufferSize, int& FrameWidth,
                               int& FrameHeight, float& AspectRatio);

  private:
    void  SeekForScreenGrab(uint64_t& Number, uint64_t FrameNum, bool Absolute);
};

#endif
