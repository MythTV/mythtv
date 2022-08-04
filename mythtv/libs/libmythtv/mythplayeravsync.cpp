// Qt
#include <QThread>

// MythTV
#include "libmythbase/mythlogging.h"
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
    m_rtcBase = 0us;
    m_priorAudioTimecode = 0ms;
    m_priorVideoTimecode = 0ms;
    m_lastFix = 0.0;
    m_avsyncAvg = 0;
    m_shortFrameDeltas = 0us;
    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC + "Reset");
}

void MythPlayerAVSync::WaitForFrame(std::chrono::microseconds FrameDue)
{
    auto unow = std::chrono::microseconds(m_avTimer.nsecsElapsed() / 1000);
    auto delay = FrameDue - unow;
    if (delay > 0us)
        QThread::usleep(delay.count());
}

void MythPlayerAVSync::ResetAVSyncForLiveTV(AudioPlayer* Audio)
{
    m_avsyncAudioPaused = kAVSyncAudioPausedLiveTV;
    Audio->Pause(true);
    m_rtcBase = 0us;
}

void MythPlayerAVSync::SetAVSyncMusicChoice(AudioPlayer* Audio)
{
    m_avsyncAudioPaused = kAVSyncAudioNotPaused;
    Audio->Pause(false);
}

void MythPlayerAVSync::GetAVSyncData(InfoMap& Map) const
{
    Map.insert("avsync", QObject::tr("%1 ms").arg(m_avsyncAvg / 1000));
}

static constexpr std::chrono::microseconds AVSYNC_MAX_LATE { 10s };
std::chrono::microseconds MythPlayerAVSync::AVSync(AudioPlayer *Audio, MythVideoFrame *Frame,
                                 std::chrono::microseconds FrameInterval, float PlaySpeed,
                                 bool HaveVideo, bool Force)
{
    // If the frame interval is less than the refresh interval, we
    // have to drop frames.  Do that here deterministically.  This
    // produces much smoother playback than doing in the normal, A/V,
    // sync code below.  It is done by tracking the difference between
    // the refresh and frame intervals.  When the accumlated
    // difference exceeds the refresh interval, drop the frame.
    if (FrameInterval < m_refreshInterval)
    {
        m_shortFrameDeltas += (m_refreshInterval - FrameInterval);
        if (m_shortFrameDeltas >= m_refreshInterval)
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Dropping short frame"));
            m_shortFrameDeltas -= m_refreshInterval;
            return -1ms;
        }
    }

    std::chrono::milliseconds videotimecode = 0ms;
    bool dropframe = false;
    bool pause_audio = false;
    std::chrono::microseconds framedue = 0us;
    std::chrono::milliseconds audio_adjustment = 0ms;
    std::chrono::microseconds unow = 0ms;
    std::chrono::microseconds lateness = 0us;
    bool reset = false;
    auto intervalms = duration_cast<std::chrono::milliseconds>(FrameInterval);
    // controller gain
    static float const s_av_control_gain = 0.4F;
    // time weighted exponential filter coefficient
    static float const s_sync_fc = 0.9F;

    if (m_avsyncAudioPaused == kAVSyncAudioPausedLiveTV)
        m_rtcBase = 0us;

    while (framedue == 0us)
    {
        if (Frame)
        {
            videotimecode = std::chrono::milliseconds(Frame->m_timecode.count() & 0x0000ffffffffffff);
            // Detect bogus timecodes from DVD and ignore them.
            if (videotimecode != Frame->m_timecode)
                videotimecode = m_maxTcVal;
        }

        unow = std::chrono::microseconds(m_avTimer.nsecsElapsed() / 1000);

        if (Force)
        {
            framedue = unow + FrameInterval;
            break;
        }

        // first time or after a seek - setup of m_rtcBase
        if (m_rtcBase == 0us)
        {
            // cater for DVB radio
            if (videotimecode == 0ms)
                videotimecode = Audio->GetAudioTime();

            // cater for data only streams (i.e. MHEG)
            bool dataonly = !Audio->HasAudioIn() && !HaveVideo;

            // On first frame we get nothing, so exit out.
            // FIXME - does this mean we skip the first frame? Should be avoidable.
            if (videotimecode == 0ms && !dataonly)
                return 0us;

            m_rtcBase = unow - chronodivide<std::chrono::microseconds>(videotimecode, PlaySpeed);
            m_maxTcVal = 0ms;
            m_maxTcFrames = 0;
            m_numDroppedFrames = 0;
        }

        if (videotimecode == 0ms)
            videotimecode = m_maxTcVal + intervalms;
        std::chrono::milliseconds tcincr = videotimecode - m_maxTcVal;
        if (tcincr > 0ms || tcincr < -100ms)
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
            framedue = m_rtcBase + chronodivide<std::chrono::microseconds>(videotimecode, PlaySpeed);
        else
            framedue = unow + FrameInterval / 2;

        lateness = unow - framedue;
        dropframe = false;
        if (lateness > 30ms)
            dropframe = m_numDroppedFrames < 10;

        if (lateness <= 30ms && m_priorAudioTimecode > 0ms && m_priorVideoTimecode > 0ms)
        {
            // Get video in sync with audio
            audio_adjustment = m_priorAudioTimecode - m_priorVideoTimecode;
            // If there is excess audio - throw it away.
            if (audio_adjustment < -200ms
                && m_avsyncAudioPaused != kAVSyncAudioPausedLiveTV)
            {
                Audio->Reset();
                audio_adjustment = 0ms;
            }
            int sign = audio_adjustment < 0ms ? -1 : 1;
            float fix_amount_ms = (m_lastFix * s_sync_fc + (1 - s_sync_fc) * audio_adjustment.count()) * sign * s_av_control_gain;
            m_lastFix = fix_amount_ms * sign;
            m_rtcBase -= microsecondsFromFloat(1000 * fix_amount_ms * sign / PlaySpeed);

            if ((audio_adjustment * sign) > intervalms)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Audio %1 by %2 ms")
                    .arg(audio_adjustment > 0ms ? "ahead" : "behind").arg(abs(audio_adjustment.count())));
            }
            if (audio_adjustment > 200ms)
                pause_audio = true;
        }

        // sanity check - reset m_rtcBase if time codes have gone crazy.
        if ((lateness > AVSYNC_MAX_LATE || lateness < - AVSYNC_MAX_LATE))
        {
            framedue = 0us;
            m_rtcBase = 0us;
            if (reset)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Resetting: lateness %1").arg(lateness.count()));
                return -1us;
            }
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Resetting: lateness = %1").arg(lateness.count()));
            reset = true;
        }
    }
    m_priorVideoTimecode = videotimecode;
    m_dispTimecode = videotimecode;

    m_avsyncAvg = static_cast<int>(m_lastFix * 1000 / s_av_control_gain);

    if (!pause_audio && m_avsyncAudioPaused)
    {
        // If the audio was paused due to playing too close to live,
        // don't unpause it until the video catches up.  This helps to
        // quickly achieve smooth playback.
        if (m_avsyncAudioPaused != kAVSyncAudioPausedLiveTV
            || audio_adjustment < 0ms)
        {
            m_avsyncAudioPaused = kAVSyncAudioNotPaused;
            Audio->Pause(false);
        }
    }
    else if (pause_audio && !m_avsyncAudioPaused)
    {
        m_avsyncAudioPaused = kAVSyncAudioPaused;
        Audio->Pause(true);
    }

    // get time codes for calculating difference next time
    m_priorAudioTimecode = Audio->GetAudioTime();

    if (dropframe)
    {
        m_numDroppedFrames++;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Dropping frame: Video is behind by %1ms")
            .arg(duration_cast<std::chrono::milliseconds>(lateness).count()));
        return -1us;
    }

    m_numDroppedFrames = 0;

    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
        QString("A/V timecodes audio=%1 video=%2 frameinterval=%3 audioadj=%4 unow=%5 udue=%6 ")
            .arg(m_priorAudioTimecode.count()).arg(m_priorVideoTimecode.count()).arg(FrameInterval.count())
            .arg(audio_adjustment.count()).arg(unow.count()).arg(framedue.count()));
    return framedue;
}
