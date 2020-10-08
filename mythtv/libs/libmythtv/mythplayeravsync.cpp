// MythTV
#include "mythlogging.h"
#include "audioplayer.h"
#include "mythplayeravsync.h"

#define LOC QString("AVSync: ")

MythPlayerAVSync::MythPlayerAVSync()
{
    m_avTimer.start();
    InitAVSync();
}

void MythPlayerAVSync::InitAVSync(void)
{
    m_rtcBase = 0;
    m_priorAudioTimecode = 0;
    m_priorVideoTimecode = 0;
    m_lastFix = 0.0;
    m_avsyncAvg = 0;
    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC + "Reset");
}

void MythPlayerAVSync::WaitForFrame(int64_t FrameDue)
{
    int64_t unow = m_avTimer.nsecsElapsed() / 1000;
    int64_t delay = FrameDue - unow;
    if (delay > 0)
        QThread::usleep(static_cast<unsigned long>(delay));
}

int64_t& MythPlayerAVSync::DisplayTimecode()
{
    return m_dispTimecode;
}

void MythPlayerAVSync::ResetAVSyncClockBase()
{
    m_rtcBase = 0;
}

bool MythPlayerAVSync::GetAVSyncAudioPause()
{
    return m_avsyncAudioPaused;
}

void MythPlayerAVSync::SetAVSyncAudioPause(bool Pause)
{
    m_avsyncAudioPaused = Pause;
}

bool MythPlayerAVSync::ResetAVSyncForLiveTV(AudioPlayer* Audio)
{
    bool result = m_rtcBase != 0;
    Audio->Pause(true);
    m_avsyncAudioPaused = true;
    m_rtcBase = 0;
    return result;
}

void MythPlayerAVSync::SetAVSyncMusicChoice(AudioPlayer* Audio)
{
    m_avsyncAudioPaused = false;
    Audio->Pause(false);
}

void MythPlayerAVSync::GetAVSyncData(InfoMap& Map)
{
    Map.insert("avsync", QObject::tr("%1 ms").arg(m_avsyncAvg / 1000));
}

#define AVSYNC_MAX_LATE 10000000
int64_t MythPlayerAVSync::AVSync(AudioPlayer *Audio, VideoFrame *Frame,
                                 int FrameInterval, float PlaySpeed,
                                 bool HaveVideo, bool Force)
{
    int64_t videotimecode = 0;
    bool dropframe = false;
    bool pause_audio = false;
    int64_t framedue = 0;
    int64_t audio_adjustment = 0;
    int64_t unow = 0;
    int64_t lateness = 0;
    auto playspeed1000 = static_cast<int64_t>(1000.0F / PlaySpeed);
    bool reset = false;
    int intervalms = FrameInterval / 1000;
    // controller gain
    static float const s_av_control_gain = 0.4F;
    // time weighted exponential filter coefficient
    static float const s_sync_fc = 0.9F;

    while (framedue == 0)
    {
        if (Frame)
        {
            videotimecode = Frame->timecode & 0x0000ffffffffffff;
            // Detect bogus timecodes from DVD and ignore them.
            if (videotimecode != Frame->timecode)
                videotimecode = m_maxTcVal;
        }

        unow = m_avTimer.nsecsElapsed() / 1000;

        if (Force)
        {
            framedue = unow + FrameInterval;
            break;
        }

        // first time or after a seek - setup of m_rtcBase
        if (m_rtcBase == 0)
        {
            // cater for DVB radio
            if (videotimecode == 0)
                videotimecode = Audio->GetAudioTime();;

            // cater for data only streams (i.e. MHEG)
            bool dataonly = !Audio->HasAudioIn() && !HaveVideo;

            // On first frame we get nothing, so exit out.
            // FIXME - does this mean we skip the first frame? Should be avoidable.
            if (videotimecode == 0 && !dataonly)
                return 0;

            m_rtcBase = unow - videotimecode * playspeed1000;
            m_maxTcVal = 0;
            m_maxTcFrames = 0;
            m_numDroppedFrames = 0;
        }

        if (videotimecode == 0)
            videotimecode = m_maxTcVal + intervalms;
        int64_t tcincr = videotimecode - m_maxTcVal;
        if (tcincr > 0 || tcincr < -100)
        {
            m_maxTcVal = videotimecode;
            m_maxTcFrames = 0;
        }
        else
        {
            m_maxTcFrames++;
            videotimecode = m_maxTcVal + m_maxTcFrames * intervalms;
        }

        if (PlaySpeed > 0.0F)
            framedue = m_rtcBase + videotimecode * playspeed1000;
        else
            framedue = unow + FrameInterval / 2;

        lateness = unow - framedue;
        dropframe = false;
        if (lateness > 30000)
            dropframe = m_numDroppedFrames < 10;

        if (lateness <= 30000 && m_priorAudioTimecode > 0 && m_priorVideoTimecode > 0)
        {
            // Get video in sync with audio
            audio_adjustment = m_priorAudioTimecode - m_priorVideoTimecode;
            // If there is excess audio - throw it away.
            if (audio_adjustment < -200)
            {
                Audio->Reset();
                audio_adjustment = 0;
            }
            int sign = audio_adjustment < 0 ? -1 : 1;
            float fix_amount = (m_lastFix * s_sync_fc + (1 - s_sync_fc) * audio_adjustment) * sign * s_av_control_gain;
            m_lastFix = fix_amount * sign;
            auto speedup1000 = static_cast<int64_t>(1000 * PlaySpeed);
            m_rtcBase -= static_cast<int64_t>(1000000 * fix_amount * sign / speedup1000);

            if ((audio_adjustment * sign) > intervalms)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Audio %1 by %2 ms")
                    .arg(audio_adjustment > 0 ? "ahead" : "behind").arg(abs(audio_adjustment)));
            }
            if (audio_adjustment > 200)
                pause_audio = true;
        }

        // sanity check - reset m_rtcBase if time codes have gone crazy.
        if ((lateness > AVSYNC_MAX_LATE || lateness < - AVSYNC_MAX_LATE))
        {
            framedue = 0;
            m_rtcBase = 0;
            if (reset)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Resetting: lateness %1").arg(lateness));
                return -1;
            }
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Resetting: lateness = %1").arg(lateness));
            reset = true;
        }
    }
    m_priorVideoTimecode = videotimecode;
    m_dispTimecode = videotimecode;

    m_avsyncAvg = static_cast<int>(m_lastFix * 1000 / s_av_control_gain);

    if (!pause_audio && m_avsyncAudioPaused)
    {
        m_avsyncAudioPaused = false;
        Audio->Pause(false);
    }
    else if (pause_audio && !m_avsyncAudioPaused)
    {
        m_avsyncAudioPaused = true;
        Audio->Pause(true);
    }

    // get time codes for calculating difference next time
    m_priorAudioTimecode = Audio->GetAudioTime();

    if (dropframe)
    {
        m_numDroppedFrames++;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Dropping frame: Video is behind by %1ms")
            .arg(lateness / 1000));
        return -1;
    }

    m_numDroppedFrames = 0;

    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
        QString("A/V timecodes audio=%1 video=%2 frameinterval=%3 audioadj=%4 unow=%5 udue=%6 ")
            .arg(m_priorAudioTimecode).arg(m_priorVideoTimecode).arg(FrameInterval)
            .arg(audio_adjustment).arg(unow).arg(framedue));
    return framedue;
}
