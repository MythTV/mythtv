#ifndef MYTHCOMMFLAGPLAYER_H
#define MYTHCOMMFLAGPLAYER_H

#include "NuppelVideoPlayer.h"

class MPUBLIC MythCommFlagPlayer : public NuppelVideoPlayer
{
  public:
    MythCommFlagPlayer(bool muted = false) : NuppelVideoPlayer(muted) { }
    bool RebuildSeekTable(bool showPercentage = true, StatusCallback cb = NULL,
                          void* cbData = NULL);
};

#endif // MYTHCOMMFLAGPLAYER_H
