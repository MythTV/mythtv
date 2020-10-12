#ifndef MYTHPLAYERAVSYNC_H
#define MYTHPLAYERAVSYNC_H

// Qt
#include <QElapsedTimer>

// MythTV
#include "mythtypes.h"
#include "mythframe.h"

class AudioPlayer;

class MythPlayerAVSync
{
  public:
    MythPlayerAVSync();

  public:
    void     InitAVSync          ();
    int64_t  AVSync              (AudioPlayer* Audio, MythVideoFrame* Frame, int FrameInterval,
                                  float PlaySpeed, bool HaveVideo, bool Force);
    void     WaitForFrame        (int64_t FrameDue);
    int64_t& DisplayTimecode     ();
    void     ResetAVSyncClockBase();
    void     GetAVSyncData       (InfoMap& Map) const;
    bool     GetAVSyncAudioPause () const;
    void     SetAVSyncAudioPause (bool Pause);
    bool     ResetAVSyncForLiveTV(AudioPlayer* Audio);
    void     SetAVSyncMusicChoice(AudioPlayer* Audio); // remove

  private:
    QElapsedTimer m_avTimer;
    bool       m_avsyncAudioPaused  { false };
    int        m_avsyncAvg          { 0 };
    int64_t    m_dispTimecode       { 0 };
    int64_t    m_rtcBase            { 0 }; // real time clock base for presentation time (microsecs)
    int64_t    m_maxTcVal           { 0 }; // maximum to date video tc
    int64_t    m_priorAudioTimecode { 0 }; // time code from prior frame
    int64_t    m_priorVideoTimecode { 0 }; // time code from prior frame
    int        m_maxTcFrames        { 0 }; // number of frames seen since max to date tc
    int        m_numDroppedFrames   { 0 }; // number of consecutive dropped frames.
    float      m_lastFix            { 0.0F }; //last sync adjustment to prior frame]
};

#endif
