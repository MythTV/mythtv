#ifndef MYTHDVDPLAYER_H
#define MYTHDVDPLAYER_H

// MythTV
#include "mythplayerui.h"

// Std
#include <cstdint>

class MythDVDPlayer : public MythPlayerUI
{
    Q_OBJECT

  signals:
    void     DisableDVDSubtitles();

  public:
    MythDVDPlayer(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context,PlayerFlags Flags = kNoFlags);

    void     ReleaseNextVideoFrame(MythVideoFrame *Buffer, std::chrono::milliseconds Timecode, bool Wrap = true) override;
    bool     HasReachedEof(void) const override;
    bool     PrepareAudioSample(std::chrono::milliseconds &Timecode) override;
    uint64_t GetBookmark(void) override;
    std::chrono::milliseconds  GetMillisecondsPlayed(bool HonorCutList) override;
    std::chrono::milliseconds  GetTotalMilliseconds(bool HonorCutList) const override;
    bool     IsInStillFrame() const override;
    int      GetNumAngles(void) const override;
    int      GetCurrentAngle(void) const override;
    QString  GetAngleName(int Angle) const override;
    bool     SwitchAngle(int Angle) override;
    int      GetNumChapters(void) override;
    int      GetCurrentChapter(void) override;
    void     GetChapterTimes(QList<std::chrono::seconds> &Times) override;

    void     SetStillFrameTimeout(std::chrono::seconds Length);
    void     StillFrameCheck(void);

  protected:
    void     ResetPlaying(bool ResetFrames = true) override;
    bool     PrebufferEnoughFrames(int MinBuffers = 0) override;
    void     DecoderPauseCheck(void) override;
    void     DoFFRewSkip(void) override;
    void     ChangeSpeed(void) override;
    void     DisplayPauseFrame(void) override;
    void     PreProcessNormalFrame(void) override;
    void     VideoStart(void) override;
    bool     VideoLoop(void) override;
    void     EventStart(void) override;
    virtual void EventEnd(void);
    void     InitialSeek(void) override;
    void     AutoDeint(MythVideoFrame* Frame, MythVideoOutput* VideoOutput,
                       std::chrono::microseconds FrameInterval, bool AllowLock = true) override;
    long long CalcMaxFFTime(long long FastFwd, bool Setjump = true) const override;
    bool     FastForward(float Seconds) override;
    bool     Rewind(float Seconds) override;
    bool     JumpToFrame(uint64_t Frame) override;
    void     CreateDecoder(TestBufferVec & Testbuf) override;
    bool     DoJumpChapter(int Chapter) override;

  protected slots:
    void     GoToMenu(const QString& Menu);
    void     GoToDVDProgram(bool Direction);
    void     SetBookmark(bool Clear = false) override;
    void     DisableCaptions(uint Mode, bool OSDMsg = true) override;
    void     EnableCaptions(uint Mode, bool OSDMsg = true) override;
    void     SetTrack(uint Type, uint TrackNo) override;
    void     DoDisableDVDSubtitles();

  private:
    void     DisplayDVDButton(void);

    int      m_buttonVersion          { 0 };
    bool     m_dvdStillFrameShowing   { false };

    // additional bookmark seeking information
    int      m_initialTitle           { -1 };
    int      m_initialAudioTrack      { -1 };
    int      m_initialSubtitleTrack   { -1 };
    QString  m_initialDvdState;

    // still frame timing
    MythTimer m_stillFrameTimer;
    std::chrono::seconds  m_stillFrameLength  { 0s };
    QRecursiveMutex m_stillFrameTimerLock;
};

#endif // MYTHDVDPLAYER_H
