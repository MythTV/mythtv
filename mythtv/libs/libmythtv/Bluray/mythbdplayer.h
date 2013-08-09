#ifndef MYTHBDPLAYER_H
#define MYTHBDPLAYER_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythplayer.h"

class MythBDPlayer : public MythPlayer
{
    Q_DECLARE_TR_FUNCTIONS(MythBDPlayer)

  public:
    MythBDPlayer(PlayerFlags flags = kNoFlags);
    virtual bool    HasReachedEof(void) const;
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

    // Non-const gets
    // Disable screen grabs for Bluray
    virtual char *GetScreenGrabAtFrame(uint64_t frameNum, bool absolute,
        int &buflen, int &vw, int &vh, float &ar) { return NULL; }
    virtual char *GetScreenGrab(int secondsin, int &buflen,
        int &vw, int &vh, float &ar) { return NULL; }

  protected:
    // Playback
    virtual bool VideoLoop(void);
    virtual void EventStart(void);
    virtual void DisplayPauseFrame(void);
    virtual void PreProcessNormalFrame(void);

    // Private decoder stuff
    virtual void CreateDecoder(char *testbuf, int testreadsize);

    // Non-const gets
    // Disable screen grabs for Bluray
    virtual void SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                   bool absolute) { return; }

  private:
    void DisplayMenu(void);
    bool m_stillFrameShowing;
};

#endif // MYTHBDPLAYER_H
