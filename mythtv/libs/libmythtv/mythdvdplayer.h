#ifndef MYTHDVDPLAYER_H
#define MYTHDVDPLAYER_H

#include <stdint.h>

#include "mythplayer.h"

class MythDVDPlayer : public MythPlayer
{
  public:
    MythDVDPlayer(bool muted = false);
    virtual void ReleaseNextVideoFrame(VideoFrame *buffer, int64_t timecode,
                                       bool wrap = true);
    virtual void DisableCaptions(uint mode, bool osd_msg=true);
    virtual void EnableCaptions(uint mode, bool osd_msg=true);
    virtual bool FastForward(float seconds);
    virtual bool Rewind(float seconds);
    virtual bool JumpToFrame(uint64_t frame);
    virtual void EventStart(void);
    virtual void EventLoop(void);
    virtual void InitialSeek(void);
    virtual void ResetPlaying(bool resetframes = true);
    virtual void EventEnd(void);
    virtual bool PrepareAudioSample(int64_t &timecode);
    virtual uint64_t GetBookmark(void) const;
    virtual void SetBookmark(void);
    virtual void ClearBookmark(bool message = true);
    virtual long long CalcMaxFFTime(long long ff, bool setjump = true) const;
    virtual int  SetTrack(uint type, int trackNo);
    virtual void DoChangeDVDTrack(void);
    virtual void ChangeDVDTrack(bool ffw);
    virtual bool GoToDVDMenu(QString str);
    virtual void GoToDVDProgram(bool direction);
    virtual void AutoDeint(VideoFrame* frame, bool allow_lock = true);
    virtual bool PrebufferEnoughFrames(bool pause_audio = true,
                                       int  min_buffers = 0);
    virtual bool DecoderGetFrameFFREW(void);
    virtual bool DecoderGetFrameREW(void);
    virtual void ChangeSpeed(void);
    virtual void AVSync(bool limit_delay = false);
    virtual void DisplayPauseFrame(void);
    virtual void DisplayNormalFrame(bool allow_pause = true);
    virtual void PreProcessNormalFrame(void);
    virtual bool VideoLoop(void);
    virtual void calcSliderPos(osdInfo &info, bool paddedFields = false);
    virtual void SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                   bool absolute);

    void HideDVDButton(bool hide) { hidedvdbutton = hide; }

    virtual int GetNumAngles(void) const;
    virtual int GetCurrentAngle(void) const;
    virtual QString GetAngleName(int angle) const;
    virtual bool SwitchAngle(int angle);

  private:
    void SetDVDBookmark(uint64_t frame);
    void DisplayDVDButton(void);

    bool hidedvdbutton;
    bool dvd_stillframe_showing;
    int  need_change_dvd_track;
};

#endif // MYTHDVDPLAYER_H
