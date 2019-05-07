#ifndef MYTHDVDPLAYER_H
#define MYTHDVDPLAYER_H

#include <cstdint>

#include "mythplayer.h"

class MythDVDPlayer : public MythPlayer
{
    Q_DECLARE_TR_FUNCTIONS(MythDVDPlayer);

  public:
    explicit MythDVDPlayer(PlayerFlags flags = kNoFlags)
        : MythPlayer(flags) {}

    // Decoder stuff..
    void ReleaseNextVideoFrame(VideoFrame *buffer, int64_t timecode,
                               bool wrap = true) override; // MythPlayer
    bool HasReachedEof(void) const override; // MythPlayer

    // Add data
    bool PrepareAudioSample(int64_t &timecode) override; // MythPlayer

    // Gets
    uint64_t GetBookmark(void) override; // MythPlayer
    int64_t GetSecondsPlayed(bool honorCutList,
                             int divisor = 1000) const override; // MythPlayer
    int64_t GetTotalSeconds(bool honorCutList,
                            int divisor = 1000) const override; // MythPlayer

    // DVD public stuff
    bool GoToMenu(QString str) override; // MythPlayer
    void GoToDVDProgram(bool direction) override; // MythPlayer
    bool IsInStillFrame() const override; // MythPlayer

    // DVD ringbuffer methods
    void ResetStillFrameTimer(void);
    void SetStillFrameTimeout(int length);
    void StillFrameCheck(void);

    // Angle public stuff
    int GetNumAngles(void) const override; // MythPlayer
    int GetCurrentAngle(void) const override; // MythPlayer
    QString GetAngleName(int angle) const override; // MythPlayer
    bool SwitchAngle(int angle) override; // MythPlayer

    // Chapter public stuff
    int  GetNumChapters(void) override; // MythPlayer
    int  GetCurrentChapter(void) override; // MythPlayer
    void GetChapterTimes(QList<long long> &times) override; // MythPlayer

  protected:
    // Non-public sets
    void SetBookmark(bool clear = false) override; // MythPlayer

    // Start/Reset/Stop playing
    void ResetPlaying(bool resetframes = true) override; // MythPlayer

    // Private decoder stuff
    bool PrebufferEnoughFrames(int min_buffers = 0) override; // MythPlayer
    void DecoderPauseCheck(void) override; // MythPlayer
    bool DecoderGetFrameFFREW(void) override; // MythPlayer
    bool DecoderGetFrameREW(void) override; // MythPlayer

    // These actually execute commands requested by public members
    void ChangeSpeed(void) override; // MythPlayer

    // Playback
    void AVSync(VideoFrame *frame, bool limit_delay = false) override; // MythPlayer
    void DisplayPauseFrame(void) override; // MythPlayer
    void PreProcessNormalFrame(void) override; // MythPlayer
    void VideoStart(void) override; // MythPlayer
    bool VideoLoop(void) override; // MythPlayer
    void EventStart(void) override; // MythPlayer
    virtual void EventEnd(void);
    void InitialSeek(void) override; // MythPlayer

    // Non-const gets
    void SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                           bool absolute) override; // MythPlayer

    // Private initialization stuff
    void AutoDeint(VideoFrame* frame, bool allow_lock = true) override; // MythPlayer

    // Complicated gets
    long long CalcMaxFFTime(long long ff, bool setjump = true) const override; // MythPlayer

    // Seek stuff
    bool FastForward(float seconds) override; // MythPlayer
    bool Rewind(float seconds) override; // MythPlayer
    bool JumpToFrame(uint64_t frame) override; // MythPlayer

    // Private Closed caption and teletext stuff
    void DisableCaptions(uint mode, bool osd_msg=true) override; // MythPlayer
    void EnableCaptions(uint mode, bool osd_msg=true) override; // MythPlayer

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    int  SetTrack(uint type, int trackNo) override; // MythPlayer

    // Private decoder stuff
    void CreateDecoder(char *testbuf, int testreadsize) override; // MythPlayer

    // Private chapter stuff
    bool DoJumpChapter(int chapter) override; // MythPlayer

  private:
    void DoChangeDVDTrack(void);
    void DisplayDVDButton(void);

    void DisplayLastFrame(void);

    int       m_buttonVersion          {0};
    bool      m_dvd_stillframe_showing {false};

    // additional bookmark seeking information
    int       m_initial_title          {-1};
    int       m_initial_audio_track    {-1};
    int       m_initial_subtitle_track {-1};
    QString   m_initial_dvdstate;

    // still frame timing
    MythTimer m_stillFrameTimer;
    int       m_stillFrameLength       {0};
    QMutex    m_stillFrameTimerLock;
};

#endif // MYTHDVDPLAYER_H
