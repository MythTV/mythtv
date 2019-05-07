#ifndef MYTHBDPLAYER_H
#define MYTHBDPLAYER_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythplayer.h"

class MythBDPlayer : public MythPlayer
{
    Q_DECLARE_TR_FUNCTIONS(MythBDPlayer);

  public:
    explicit MythBDPlayer(PlayerFlags flags = kNoFlags)
        : MythPlayer(flags) {}
    bool    HasReachedEof(void) const override; // MythPlayer
    bool    GoToMenu(QString str) override; // MythPlayer
    int     GetNumChapters(void) override; // MythPlayer
    int     GetCurrentChapter(void) override; // MythPlayer
    void    GetChapterTimes(QList<long long> &times) override; // MythPlayer
    int64_t GetChapter(int chapter) override; // MythPlayer

    int GetNumTitles(void) const override; // MythPlayer
    int GetNumAngles(void) const override; // MythPlayer
    int GetCurrentTitle(void) const override; // MythPlayer
    int GetCurrentAngle(void) const override; // MythPlayer
    int GetTitleDuration(int title) const override; // MythPlayer
    QString GetTitleName(int title) const override; // MythPlayer
    QString GetAngleName(int angle) const override; // MythPlayer
    bool SwitchTitle(int title) override; // MythPlayer
    bool PrevTitle(void) override; // MythPlayer
    bool NextTitle(void) override; // MythPlayer
    bool SwitchAngle(int angle) override; // MythPlayer
    bool PrevAngle(void) override; // MythPlayer
    bool NextAngle(void) override; // MythPlayer
    void SetBookmark(bool clear) override; // MythPlayer
    uint64_t GetBookmark(void) override; // MythPlayer

    // Non-const gets
    // Disable screen grabs for Bluray
    char *GetScreenGrabAtFrame(uint64_t /*frameNum*/, bool /*absolute*/,
                               int &/*buflen*/, int &/*vw*/, int &/*vh*/,
                               float &/*ar*/) override // MythPlayer
        { return nullptr; }
    char *GetScreenGrab(int /*secondsin*/, int &/*bufflen*/,
                        int &/*vw*/, int &/*vh*/, float &/*ar*/) override // MythPlayer
        { return nullptr; }

  protected:
    // Playback
    void VideoStart(void) override; // MythPlayer
    bool VideoLoop(void) override; // MythPlayer
    void EventStart(void) override; // MythPlayer
    void DisplayPauseFrame(void) override; // MythPlayer
    void PreProcessNormalFrame(void) override; // MythPlayer

    // Seek stuff
    bool JumpToFrame(uint64_t frame) override; // MythPlayer

    // Private decoder stuff
    void CreateDecoder(char *testbuf, int testreadsize) override; // MythPlayer

    // Non-const gets
    // Disable screen grabs for Bluray
    void SeekForScreenGrab(uint64_t &/*number*/, uint64_t /*frameNum*/,
                           bool /*absolute*/) override // MythPlayer
        {}

  private:
    void    DisplayMenu(void);
    bool    m_stillFrameShowing {false};
    QString m_initialBDState;
};

#endif // MYTHBDPLAYER_H
