#ifndef MYTHDVDPLAYER_H
#define MYTHDVDPLAYER_H

#include <stdint.h>

#include "mythplayer.h"

class MythDVDPlayer : public MythPlayer
{
  public:
    MythDVDPlayer(PlayerFlags flags = kNoFlags);

    // Decoder stuff..
    virtual void ReleaseNextVideoFrame(VideoFrame *buffer, int64_t timecode,
                                       bool wrap = true);
    virtual bool HasReachedEof(void) const;

    // Add data
    virtual bool PrepareAudioSample(int64_t &timecode);

    // Gets
    virtual uint64_t GetBookmark(void);
    virtual  int64_t GetSecondsPlayed(bool honorCutList);
    virtual  int64_t GetTotalSeconds(void) const;

    // DVD public stuff
    virtual bool GoToMenu(QString str);
    virtual void GoToDVDProgram(bool direction);

    // DVD ringbuffer methods
    void ResetStillFrameTimer(void);
    void SetStillFrameTimeout(int length);
    void StillFrameCheck(void);

    // Angle public stuff
    virtual int GetNumAngles(void) const;
    virtual int GetCurrentAngle(void) const;
    virtual QString GetAngleName(int angle) const;
    virtual bool SwitchAngle(int angle);

    // Chapter public stuff
    virtual int  GetNumChapters(void);
    virtual int  GetCurrentChapter(void);
    virtual void GetChapterTimes(QList<long long> &times);

  protected:
    // Non-public sets
    virtual void SetBookmark(bool clear = false);

    // Start/Reset/Stop playing
    virtual void ResetPlaying(bool resetframes = true);

    // Private decoder stuff
    virtual bool PrebufferEnoughFrames(int min_buffers = 0);
    virtual void DecoderPauseCheck(void);
    virtual bool DecoderGetFrameFFREW(void);
    virtual bool DecoderGetFrameREW(void);

    // These actually execute commands requested by public members
    virtual void ChangeSpeed(void);

    // Playback
    virtual void AVSync(VideoFrame *buffer, bool limit_delay = false);
    virtual void DisplayPauseFrame(void);
    virtual void PreProcessNormalFrame(void);
    virtual bool VideoLoop(void);
    virtual void EventStart(void);
    virtual void EventEnd(void);
    virtual void InitialSeek(void);

    // Non-const gets
    virtual void SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                   bool absolute);

    // Private initialization stuff
    virtual void AutoDeint(VideoFrame* frame, bool allow_lock = true);

    // Complicated gets
    virtual long long CalcMaxFFTime(long long ff, bool setjump = true) const;

    // Seek stuff
    virtual bool FastForward(float seconds);
    virtual bool Rewind(float seconds);
    virtual bool JumpToFrame(uint64_t frame);

    // Private Closed caption and teletext stuff
    virtual void DisableCaptions(uint mode, bool osd_msg=true);
    virtual void EnableCaptions(uint mode, bool osd_msg=true);

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    virtual int  SetTrack(uint type, int trackNo);

    // Private decoder stuff
    virtual void CreateDecoder(char *testbuf, int testreadsize);

    // Private chapter stuff
    virtual bool DoJumpChapter(int chapter);

  private:
    void DoChangeDVDTrack(void);
    void SetDVDBookmark(uint64_t frame);
    void DisplayDVDButton(void);

    void DisplayLastFrame(void);

    int  m_buttonVersion;
    bool dvd_stillframe_showing;

    // additional bookmark seeking information
    int m_initial_title;
    int m_initial_audio_track;
    int m_initial_subtitle_track;

    // still frame timing
    MythTimer m_stillFrameTimer;
    int       m_stillFrameLength;
    QMutex    m_stillFrameTimerLock;
};

#endif // MYTHDVDPLAYER_H
