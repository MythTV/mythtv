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

    virtual int GetNumTitles(void) const;
    virtual int GetCurrentTitle(void) const;
    virtual int GetTitleDuration(int title) const;
    virtual QString GetTitleName(int title) const;
    virtual bool SwitchTitle(int title);
    virtual bool PrevTitle(void);
    virtual bool NextTitle(void);
};

#endif // MYTHBDPLAYER_H
