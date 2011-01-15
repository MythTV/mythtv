#ifndef MYTHBDPLAYER_H
#define MYTHBDPLAYER_H

#include "mythplayer.h"

class MythBDPlayer : public MythPlayer
{
  public:
    MythBDPlayer(bool muted = false);
    virtual bool    GoToMenu(QString str);
    virtual int     GetNumChapters(void);
    virtual int     GetCurrentChapter(void);
    virtual void    GetChapterTimes(QList<long long> &times);
    virtual int64_t GetChapter(int chapter);

    virtual int GetNumTitles(void) const;
    virtual int GetNumAngles(void) const;
    virtual int GetCurrentTitle(void) const;
    virtual int GetCurrentAngle(void) const;
    virtual int GetTitleDuration(int title) const;
    virtual QString GetTitleName(int title) const;
    virtual QString GetAngleName(int angle) const;
    virtual bool SwitchTitle(int title);
    virtual bool PrevTitle(void);
    virtual bool NextTitle(void);
    virtual bool SwitchAngle(int angle);
    virtual bool PrevAngle(void);
    virtual bool NextAngle(void);

  protected:
    // Playback
    virtual bool VideoLoop(void);
    virtual void PreProcessNormalFrame(void);

  private:
    void DisplayMenu(void);

    bool m_inMenu;
};

#endif // MYTHBDPLAYER_H
