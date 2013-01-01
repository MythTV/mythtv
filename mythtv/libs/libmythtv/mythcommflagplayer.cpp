#include <iostream>
using namespace std;

#include <QRunnable>

#include "mythcommflagplayer.h"
#include "mthreadpool.h"
#include "mythlogging.h"

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

    DecoderGetFrame(kDecodeNothing,true);

    if (showPercentage)
        cout << "\r                         \r" << flush;

    int prevperc = -1;
    while (!GetEof())
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
                    LOG(VB_GENERAL, LOG_INFO, QString("Progress %1% @ %2fps")
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
                    LOG(VB_GENERAL, LOG_INFO, QString("Frames processed %1")
                        .arg(myFramesPlayed));
                }
            }
        }

        if (DecoderGetFrame(kDecodeNothing,true))
            myFramesPlayed = decoder->GetFramesRead();
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
