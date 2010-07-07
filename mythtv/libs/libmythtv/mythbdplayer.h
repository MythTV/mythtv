#ifndef MYTHBDPLAYER_H
#define MYTHBDPLAYER_H

#include "NuppelVideoPlayer.h"

class MythBDPlayer : public NuppelVideoPlayer
{
  public:
    MythBDPlayer(bool muted = false);
    virtual int     GetNumChapters(void);
    virtual int     GetCurrentChapter(void);
    virtual void    GetChapterTimes(QList<long long> &times);
    virtual int64_t GetChapter(int chapter);
};

#endif // MYTHBDPLAYER_H
