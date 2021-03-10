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
    std::chrono::microseconds AVSync (AudioPlayer* Audio, MythVideoFrame* Frame,
                                      std::chrono::microseconds FrameInterval,
                                      float PlaySpeed, bool HaveVideo, bool Force);
    void     WaitForFrame        (std::chrono::microseconds FrameDue);
    std::chrono::milliseconds& DisplayTimecode     ();
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
    std::chrono::milliseconds m_dispTimecode       { 0ms };
    std::chrono::microseconds m_rtcBase { 0us }; // real time clock base for presentation time
    std::chrono::milliseconds m_maxTcVal           { 0ms }; // maximum to date video tc
    std::chrono::milliseconds m_priorAudioTimecode { 0ms }; // time code from prior frame
    std::chrono::milliseconds m_priorVideoTimecode { 0ms }; // time code from prior frame
    int        m_maxTcFrames        { 0 }; // number of frames seen since max to date tc
    int        m_numDroppedFrames   { 0 }; // number of consecutive dropped frames.
    float      m_lastFix            { 0.0F }; //last sync adjustment to prior frame]
};

#endif
