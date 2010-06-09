#ifndef MYTHBDPLAYER_H
#define MYTHBDPLAYER_H

#include "NuppelVideoPlayer.h"

class MythBDPlayer : public NuppelVideoPlayer
{
  public:
    MythBDPlayer(bool muted = false);
};

#endif // MYTHBDPLAYER_H
