
#include "mythcommflagplayer.h"

#include <QRunnable>

#include "mthreadpool.h"
#include "mythlogging.h"

#include <unistd.h> // for usleep()
#include <iostream> // for cout()

using namespace std;

class RebuildSaver : public QRunnable
{
  public:
    RebuildSaver(DecoderBase *d, uint64_t f, uint64_t l)
        : m_decoder(d), m_first(f), m_last(l)
    {
        QMutexLocker locker(&s_lock);
        s_cnt[d]++;
    }

    virtual void run(void)
    {
        m_decoder->SavePositionMapDelta(m_first, m_last);

        QMutexLocker locker(&s_lock);
        s_cnt[m_decoder]--;
        if (!s_cnt[m_decoder])
            s_wait.wakeAll();
    }

    static uint GetCount(DecoderBase *d)
    {
        QMutexLocker locker(&s_lock);
        return s_cnt[d];
    }

    static void Wait(DecoderBase *d)
    {
        QMutexLocker locker(&s_lock);
        if (!s_cnt[d])
            return;
        while (s_wait.wait(&s_lock))
        {
            if (!s_cnt[d])
                return;
        }
    }

  private:
    DecoderBase *m_decoder;
    uint64_t     m_first;
    uint64_t     m_last;

    static QMutex                  s_lock;
    static QWaitCondition          s_wait;
    static QMap<DecoderBase*,uint> s_cnt;
};
QMutex                  RebuildSaver::s_lock;
QWaitCondition          RebuildSaver::s_wait;
QMap<DecoderBase*,uint> RebuildSaver::s_cnt;

bool MythCommFlagPlayer::RebuildSeekTable(
    bool showPercentage, StatusCallback cb, void* cbData)
{
    int percentage = 0;
    uint64_t myFramesPlayed = 0, pmap_first = 0,  pmap_last  = 0;

    killdecoder = false;
    framesPlayed = 0;

    // clear out any existing seektables
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        player_ctx->playingInfo->ClearPositionMap(MARK_KEYFRAME);
        player_ctx->playingInfo->ClearPositionMap(MARK_GOP_START);
        player_ctx->playingInfo->ClearPositionMap(MARK_GOP_BYFRAME);
        player_ctx->playingInfo->ClearPositionMap(MARK_DURATION_MS);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (OpenFile() < 0)
        return false;

    SetPlaying(true);

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "RebuildSeekTable unable to initialize video");
        SetPlaying(false);
        return false;
    }

    ClearAfterSeek();

    int save_timeout = 1001;
    MythTimer flagTime, ui_timer, inuse_timer, save_timer;
    flagTime.start();
    ui_timer.start();
    inuse_timer.start();
    save_timer.start();

    decoder->TrackTotalDuration(true);

    if (showPercentage)
        cout << "\r                         \r" << flush;

    int prevperc = -1;
    bool usingIframes = false;
    while (GetEof() == kEofStateNone)
    {
        if (inuse_timer.elapsed() > 2534)
        {
            inuse_timer.restart();
            player_ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (player_ctx->playingInfo)
                player_ctx->playingInfo->UpdateInUseMark();
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        if (save_timer.elapsed() > save_timeout)
        {
            // Give DB some breathing room if it gets far behind..
            if (myFramesPlayed - pmap_last > 5000)
                usleep(200 * 1000);

            // If we're already saving, just save a larger block next time..
            if (RebuildSaver::GetCount(decoder) < 1)
            {
                pmap_last = myFramesPlayed;
                MThreadPool::globalInstance()->start(
                    new RebuildSaver(decoder, pmap_first, pmap_last),
                    "RebuildSaver");
                pmap_first = pmap_last + 1;
            }

            save_timer.restart();
        }

        if (ui_timer.elapsed() > 98)
        {
            ui_timer.restart();

            if (totalFrames)
            {
                float elapsed = flagTime.elapsed() * 0.001f;
                int flagFPS = (elapsed > 0.0f) ?
                    (int)(myFramesPlayed / elapsed) : 0;

                percentage = myFramesPlayed * 100 / totalFrames;
                if (cb)
                    (*cb)(percentage, cbData);

                if (showPercentage)
                {
                    QString str = QString("\r%1%/%2fps  \r")
                        .arg(percentage,3).arg(flagFPS,5);
                    cout << qPrintable(str) << flush;
                }
                else if (percentage % 10 == 0 && prevperc != percentage)
                {
                    prevperc = percentage;
                    LOG(VB_COMMFLAG, LOG_INFO, QString("Progress %1% @ %2fps")
                        .arg(percentage,3).arg(flagFPS,5));
                }
            }
            else
            {
                if (showPercentage)
                {
                    QString str = QString("\r%1  \r").arg(myFramesPlayed,6);
                    cout << qPrintable(str) << flush;
                }
                else if (myFramesPlayed % 1000 == 0)
                {
                    LOG(VB_COMMFLAG, LOG_INFO, QString("Frames processed %1")
                        .arg(myFramesPlayed));
                }
            }
        }

        if (DecoderGetFrame(kDecodeNothing,true))
            myFramesPlayed = decoder->GetFramesRead();

        // H.264 recordings from an HD-PVR contain IDR keyframes,
        // which are the only valid cut points for lossless cuts.
        // However, DVB-S h.264 recordings may lack IDR keyframes, in
        // which case we need to allow non-IDR I-frames.  If we get
        // far enough into the rebuild without having created any
        // seektable entries, we can assume it is because of the IDR
        // keyframe setting, and so we rewind and allow h.264 non-IDR
        // I-frames to be treated as keyframes.
        uint64_t frames = decoder->GetFramesRead();
        if (!usingIframes &&
            (GetEof() != kEofStateNone || (frames > 1000 && frames < 1100)) &&
            !decoder->HasPositionMap())
        {
            cout << "No I-frames found, rewinding..." << endl;
            decoder->DoRewind(0);
            decoder->Reset(true, true, true);
            pmap_first = pmap_last = myFramesPlayed = 0;
            decoder->SetIdrOnlyKeyframes(false);
            usingIframes = true;
        }
    }

    if (showPercentage)
        cout << "\r                         \r" << flush;

    SaveTotalDuration();
    SaveTotalFrames();

    SetPlaying(false);
    killdecoder = true;

    MThreadPool::globalInstance()->start(
        new RebuildSaver(decoder, pmap_first, myFramesPlayed),
        "RebuildSaver");
    RebuildSaver::Wait(decoder);

    return true;
}
