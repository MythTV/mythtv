// Qt
#include <QRunnable>

// MythTV
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythlogging.h"
#include "mythcommflagplayer.h"

// Std
#include <unistd.h>
#include <iostream>

QMutex                   MythRebuildSaver::s_lock;
QWaitCondition           MythRebuildSaver::s_wait;
QHash<DecoderBase*,uint> MythRebuildSaver::s_count;

#define LOC QString("CommFlagPlayer: ")

MythRebuildSaver::MythRebuildSaver(DecoderBase* Decoder, uint64_t First, uint64_t Last)
  : m_decoder(Decoder),
    m_first(First),
    m_last(Last)
{
    QMutexLocker locker(&s_lock);
    s_count[Decoder]++;
}

void MythRebuildSaver::run()
{
    m_decoder->SavePositionMapDelta(static_cast<long long>(m_first), static_cast<long long>(m_last));

    QMutexLocker locker(&s_lock);
    s_count[m_decoder]--;
    if (!s_count[m_decoder])
        s_wait.wakeAll();
}

uint MythRebuildSaver::GetCount(DecoderBase* Decoder)
{
    QMutexLocker locker(&s_lock);
    return s_count[Decoder];
}

void MythRebuildSaver::Wait(DecoderBase*Decoder)
{
    QMutexLocker locker(&s_lock);
    if (!s_count[Decoder])
        return;

    while (s_wait.wait(&s_lock))
        if (!s_count[Decoder])
            return;
}

MythCommFlagPlayer::MythCommFlagPlayer(PlayerContext* Context, PlayerFlags Flags)
  : MythPlayer(Context, Flags)
{
}

bool MythCommFlagPlayer::RebuildSeekTable(bool ShowPercentage, StatusCallback Callback, void* Opaque)
{
    uint64_t myFramesPlayed = 0;
    uint64_t pmap_first = 0;
    uint64_t pmap_last  = 0;

    m_killDecoder = false;
    m_framesPlayed = 0;

    // clear out any existing seektables
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
    {
        m_playerCtx->m_playingInfo->ClearPositionMap(MARK_KEYFRAME);
        m_playerCtx->m_playingInfo->ClearPositionMap(MARK_GOP_START);
        m_playerCtx->m_playingInfo->ClearPositionMap(MARK_GOP_BYFRAME);
        m_playerCtx->m_playingInfo->ClearPositionMap(MARK_DURATION_MS);
    }
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (OpenFile() < 0)
        return false;

    SetPlaying(true);

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, "RebuildSeekTable unable to initialize video");
        SetPlaying(false);
        return false;
    }

    ClearAfterSeek();

    std::chrono::milliseconds save_timeout { 1s + 1ms };
    MythTimer flagTime;
    MythTimer ui_timer;
    MythTimer inuse_timer;
    MythTimer save_timer;
    flagTime.start();
    ui_timer.start();
    inuse_timer.start();
    save_timer.start();

    m_decoder->TrackTotalDuration(true);

    if (ShowPercentage)
        std::cout << "\r                         \r" << std::flush;

    int prevperc = -1;
    bool usingIframes = false;
    while (GetEof() == kEofStateNone)
    {
        if (inuse_timer.elapsed() > 2534ms)
        {
            inuse_timer.restart();
            m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
            if (m_playerCtx->m_playingInfo)
                m_playerCtx->m_playingInfo->UpdateInUseMark();
            m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        if (save_timer.elapsed() > save_timeout)
        {
            // Give DB some breathing room if it gets far behind..
            if (myFramesPlayed - pmap_last > 5000)
                std::this_thread::sleep_for(200ms);

            // If we're already saving, just save a larger block next time..
            if (MythRebuildSaver::GetCount(m_decoder) < 1)
            {
                pmap_last = myFramesPlayed;
                MThreadPool::globalInstance()->start(
                    new MythRebuildSaver(m_decoder, pmap_first, pmap_last),
                    "RebuildSaver");
                pmap_first = pmap_last + 1;
            }

            save_timer.restart();
        }

        if (ui_timer.elapsed() > 98ms)
        {
            ui_timer.restart();

            if (m_totalFrames)
            {
                float elapsed = flagTime.elapsed().count() * 0.001F;
                auto flagFPS = (elapsed > 0.0F) ? static_cast<int>(myFramesPlayed / elapsed) : 0;
                auto percentage = static_cast<int>(myFramesPlayed * 100 / m_totalFrames);
                if (Callback)
                    (*Callback)(percentage, Opaque);

                if (ShowPercentage)
                {
                    QString str = QString("\r%1%/%2fps  \r").arg(percentage,3).arg(flagFPS,5);
                    std::cout << qPrintable(str) << std::flush;
                }
                else if (percentage % 10 == 0 && prevperc != percentage)
                {
                    prevperc = percentage;
                    LOG(VB_COMMFLAG, LOG_INFO, QString("Progress %1% @ %2fps").arg(percentage,3).arg(flagFPS,5));
                }
            }
            else
            {
                if (ShowPercentage)
                {
                    QString str = QString("\r%1  \r").arg(myFramesPlayed,6);
                    std::cout << qPrintable(str) << std::flush;
                }
                else if (myFramesPlayed % 1000 == 0)
                {
                    LOG(VB_COMMFLAG, LOG_INFO, QString("Frames processed %1").arg(myFramesPlayed));
                }
            }
        }

        if (DecoderGetFrame(kDecodeNothing,true))
            myFramesPlayed = static_cast<uint64_t>(m_decoder->GetFramesRead());

        // H.264 recordings from an HD-PVR contain IDR keyframes,
        // which are the only valid cut points for lossless cuts.
        // However, DVB-S h.264 recordings may lack IDR keyframes, in
        // which case we need to allow non-IDR I-frames.  If we get
        // far enough into the rebuild without having created any
        // seektable entries, we can assume it is because of the IDR
        // keyframe setting, and so we rewind and allow h.264 non-IDR
        // I-frames to be treated as keyframes.
        auto frames = static_cast<uint64_t>(m_decoder->GetFramesRead());
        if (!usingIframes &&
            (GetEof() != kEofStateNone || (frames > 1000 && frames < 1100)) &&
            !m_decoder->HasPositionMap())
        {
            std::cout << "No I-frames found, rewinding..." << std::endl;
            m_decoder->DoRewind(0);
            m_decoder->Reset(true, true, true);
            pmap_first = pmap_last = myFramesPlayed = 0;
            m_decoder->SetIdrOnlyKeyframes(false);
            usingIframes = true;
        }
    }

    if (ShowPercentage)
        std::cout << "\r                         \r" << std::flush;

    SaveTotalDuration();
    SaveTotalFrames();

    SetPlaying(false);
    m_killDecoder = true;

    MThreadPool::globalInstance()->start(
        new MythRebuildSaver(m_decoder, pmap_first, myFramesPlayed),
        "RebuildSaver");
    MythRebuildSaver::Wait(m_decoder);

    return true;
}

/*! \brief Returns a specific frame from the video.
 *
 *   NOTE: You must call DiscardVideoFrame(VideoFrame*) on
 *         the frame returned, as this marks the frame as
 *         being used and hence unavailable for decoding.
 */
MythVideoFrame* MythCommFlagPlayer::GetRawVideoFrame(long long FrameNumber)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->UpdateInUseMark();
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (!m_decoderThread)
        DecoderStart(false);

    if (FrameNumber >= 0)
    {
        DoJumpToFrame(static_cast<uint64_t>(FrameNumber), kInaccuracyNone);
        ClearAfterSeek();
    }

    int tries = 0;
    while (!m_videoOutput->ValidVideoFrames() && ((tries++) < 100))
    {
        m_decodeOneFrame = true;
        std::this_thread::sleep_for(10ms);
        if ((tries & 10) == 10)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Waited 100ms for video frame");
    }

    m_videoOutput->StartDisplayingFrame();
    return m_videoOutput->GetLastShownFrame();
}

