#ifndef MYTHDVDPLAYER_H
#define MYTHDVDPLAYER_H

// MythTV
#include "mythplayer.h"

// Std
#include <cstdint>

class MythDVDPlayer : public MythPlayer
{
    Q_DECLARE_TR_FUNCTIONS(MythDVDPlayer)

  public:
    explicit MythDVDPlayer(PlayerFlags flags = kNoFlags)
        : MythPlayer(flags) {}

    void     ReleaseNextVideoFrame(VideoFrame *Buffer, int64_t Timecode, bool Wrap = true) override;
    bool     HasReachedEof(void) const override;
    bool     PrepareAudioSample(int64_t &Timecode) override;
    uint64_t GetBookmark(void) override;
    int64_t  GetSecondsPlayed(bool HonorCutList, int Divisor = 1000) override;
    int64_t  GetTotalSeconds(bool HonorCutList, int Divisor = 1000) const override;
    bool     GoToMenu(QString Menu) override;
    void     GoToDVDProgram(bool Direction) override;
    bool     IsInStillFrame() const override;
    int      GetNumAngles(void) const override;
    int      GetCurrentAngle(void) const override;
    QString  GetAngleName(int Angle) const override;
    bool     SwitchAngle(int Angle) override;
    int      GetNumChapters(void) override;
    int      GetCurrentChapter(void) override;
    void     GetChapterTimes(QList<long long> &Times) override;

    void     ResetStillFrameTimer(void);
    void     SetStillFrameTimeout(int Length);
    void     StillFrameCheck(void);

  protected:
    void     SetBookmark(bool Clear = false) override;
    void     ResetPlaying(bool ResetFrames = true) override;
    bool     PrebufferEnoughFrames(int MinBuffers = 0) override;
    void     DecoderPauseCheck(void) override;
    bool     DecoderGetFrameFFREW(void) override;
    bool     DecoderGetFrameREW(void) override;
    void     ChangeSpeed(void) override;
    void     DisplayPauseFrame(void) override;
    void     PreProcessNormalFrame(void) override;
    void     VideoStart(void) override;
    bool     VideoLoop(void) override;
    void     EventStart(void) override;
    virtual void EventEnd(void);
    void     InitialSeek(void) override;
    void     SeekForScreenGrab(uint64_t &Number, uint64_t FrameNum,
                               bool Absolute) override;
    void     AutoDeint(VideoFrame* Frame, bool AllowLock = true) override;
    long long CalcMaxFFTime(long long FastFwd, bool Setjump = true) const override;
    bool     FastForward(float Seconds) override;
    bool     Rewind(float Seconds) override;
    bool     JumpToFrame(uint64_t Frame) override;
    void     DisableCaptions(uint Mode, bool OSDMsg = true) override;
    void     EnableCaptions(uint Mode, bool OSDMsg = true) override;
    int      SetTrack(uint Type, int TrackNo) override;
    void     CreateDecoder(char *Testbuf, int TestReadSize) override;
    bool     DoJumpChapter(int Chapter) override;

  private:
    void     DoChangeDVDTrack(void);
    void     DisplayDVDButton(void);
    void     DisplayLastFrame(void);

    int      m_buttonVersion          { 0 };
    bool     m_dvdStillFrameShowing   { false };

    // additional bookmark seeking information
    int      m_initialTitle           { -1 };
    int      m_initialAudioTrack      { -1 };
    int      m_initialSubtitleTrack   { -1 };
    QString  m_initialDvdState        { };

    // still frame timing
    MythTimer m_stillFrameTimer       { };
    int      m_stillFrameLength       { 0 };
    QMutex   m_stillFrameTimerLock    { QMutex::Recursive };
};

#endif // MYTHDVDPLAYER_H
