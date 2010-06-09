#include "mythbdplayer.h"

#define LOC     QString("BDPlayer: ")
#define LOC_ERR QString("BDPlayer error: ")

MythBDPlayer::MythBDPlayer(bool muted) : NuppelVideoPlayer(muted)
{
}
