#ifndef MYTHCOMMFLAGPLAYER_H
#define MYTHCOMMFLAGPLAYER_H

#include "mythplayer.h"

class MPUBLIC MythCommFlagPlayer : public MythPlayer
{
  public:
    MythCommFlagPlayer(bool muted = false) : MythPlayer(muted) { }
    bool RebuildSeekTable(bool showPercentage = true, StatusCallback cb = NULL,
                          void* cbData = NULL);
};

#endif // MYTHCOMMFLAGPLAYER_H
