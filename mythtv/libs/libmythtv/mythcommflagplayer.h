#ifndef MYTHCOMMFLAGPLAYER_H
#define MYTHCOMMFLAGPLAYER_H

#include "mythplayer.h"

class MTV_PUBLIC MythCommFlagPlayer : public MythPlayer
{
  public:
    explicit MythCommFlagPlayer(PlayerFlags flags = kNoFlags) : MythPlayer(flags) { }
    MythCommFlagPlayer(MythCommFlagPlayer& rhs);
    bool RebuildSeekTable(bool showPercentage = true, StatusCallback cb = nullptr,
                          void* cbData = nullptr);
};

#endif // MYTHCOMMFLAGPLAYER_H
