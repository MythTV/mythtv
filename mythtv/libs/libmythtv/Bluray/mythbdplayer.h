#ifndef MYTHBDPLAYER_H
#define MYTHBDPLAYER_H

// Qt
#include <QCoreApplication>

// MythTV
#include "mythplayerui.h"

class MythBDPlayer : public MythPlayerUI
{
    Q_OBJECT

  public:
    MythBDPlayer(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags = kNoFlags);
    bool     HasReachedEof     (void) const override;
    bool     GoToMenu          (const QString& Menu) override;
    int      GetNumChapters    (void) override;
    int      GetCurrentChapter (void) override;
    void     GetChapterTimes   (QList<long long> &ChapterTimes) override;
    int64_t  GetChapter        (int Chapter) override;
    int      GetNumTitles      (void) const override;
    int      GetNumAngles      (void) const override;
    int      GetCurrentTitle   (void) const override;
    int      GetCurrentAngle   (void) const override;
    int      GetTitleDuration  (int Title) const override;
    QString  GetTitleName      (int Title) const override;
    QString  GetAngleName      (int Angle) const override;
    bool     SwitchTitle       (int Title) override;
    bool     PrevTitle         (void) override;
    bool     NextTitle         (void) override;
    bool     SwitchAngle       (int Angle) override;
    bool     PrevAngle         (void) override;
    bool     NextAngle         (void) override;
    uint64_t GetBookmark       (void) override;

  protected slots:
    void     SetBookmark       (bool Clear) override;

  protected:
    void     VideoStart        (void) override;
    bool     VideoLoop         (void) override;
    void     EventStart        (void) override;
    void     DisplayPauseFrame (void) override;
    void     PreProcessNormalFrame(void) override;
    bool     JumpToFrame       (uint64_t Frame) override;
    void     CreateDecoder     (TestBufferVec & TestBuffer) override;

  private:
    Q_DISABLE_COPY(MythBDPlayer)
    void     DisplayMenu(void);

    bool     m_stillFrameShowing { false };
    QString  m_initialBDState;
};

#endif
