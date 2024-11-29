#include "HLSPlaylistWorker.h"
#include "HLSReader.h"

const int PLAYLIST_FAILURE = 20;  // number of consecutive failures after which
                                  // an error will be propagated.

#define LOC QString("%1 playlist: ").arg(m_parent->StreamURL().isEmpty() ? "Worker" : m_parent->StreamURL())

HLSPlaylistWorker::HLSPlaylistWorker(HLSReader *parent)
    :  MThread("HLSPlaylist"), m_parent(parent)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSPlaylistWorker::~HLSPlaylistWorker(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "dtor");
}

void HLSPlaylistWorker::Cancel(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Cancel -- begin");
    m_lock.lock();
    m_cancel = true;
    m_waitcond.wakeAll();
    m_lock.unlock();
    wait();
    LOG(VB_RECORD, LOG_INFO, LOC + "Cancel -- end");
}

void HLSPlaylistWorker::run(void)
{
    std::chrono::milliseconds wakeup = 1s;
    double      delay = 0;

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- begin");

    RunProlog();

    auto *downloader = new MythSingleDownload;

    while (!m_cancel)
    {
        m_lock.lock();
        if (!m_wokenup)
        {
            std::chrono::milliseconds waittime = std::max(1000ms, wakeup);
            LOG(VB_RECORD, (waittime > 12s ? LOG_INFO : LOG_DEBUG), LOC +
                QString("refreshing in %2ms")
                .arg(waittime.count()));
            m_waitcond.wait(&m_lock, duration_cast<std::chrono::milliseconds>(
                                waittime).count());
        }
        m_wokenup = false;
        m_lock.unlock();

        if (m_parent->FatalError())
        {
            LOG(VB_GENERAL, LOG_CRIT, LOC + "Fatal error detected");
            break;
        }
        if (m_cancel)
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "canceled");
            break;
        }

        if (m_parent->LoadMetaPlaylists(*downloader))
        {
            if (m_parent->PlaylistRetryCount() > 0)
            {
                LOG(VB_RECORD, LOG_INFO, LOC +
                    QString("Playlist successfully downloaded.  Buffered: %1%")
                    .arg(m_parent->PercentBuffered()));
            }
            m_parent->PlaylistGood();
            delay = 0.5;
        }
        else
        {
            m_parent->PlaylistRetrying();
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Playlist download failed -- Retry #%1, "
                        "Buffered: %2%")
                .arg(m_parent->PlaylistRetryCount())
                .arg((m_parent->PercentBuffered())));

            if (m_parent->PlaylistRetryCount() > 1)
            {
                // Asking QNetworkAccessManager to redownload after a
                // failure seems to result in another failure, even if the
                // playlist is now available.  So, create a new instance.
                delete downloader;
                downloader = new MythSingleDownload;

                if (m_parent->PlaylistRetryCount() == 3)
                    m_parent->AllowPlaylistSwitch();
                if (m_parent->PlaylistRetryCount() < 4)
                    m_parent->EnableDebugging();
                if (m_parent->PlaylistRetryCount() == PLAYLIST_FAILURE)
                {
                    LOG(VB_RECORD, LOG_ERR, LOC + "Loading playlist failed. "
                        "Perform a complete reset.");
                    m_parent->ResetStream();
                }
            }

            if (m_parent->PercentBuffered() > 85)
            {
                // Don't wait, we need more segments to work on.
                if (m_parent->PlaylistRetryCount() == 1)
                    continue; // restart immediately if it's the first try
                delay = 0.5;
            }
            else if (m_parent->PlaylistRetryCount() == 1)
            {
                delay = 0.5;
            }
            else if (m_parent->PlaylistRetryCount() == 2)
            {
                delay = 1;
            }
            else
            {
                delay = 2;
            }
        }

        // When should the playlist be reloaded
        wakeup = m_parent->TargetDuration() > 0s ?
                 m_parent->TargetDuration() : 10s;

        wakeup = std::chrono::milliseconds(static_cast<int>(delay * wakeup.count()));

        if (wakeup > 60s)
            wakeup = 60s;

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString(" TargetDuration:%1s").arg(m_parent->TargetDuration().count()) +
            QString(" wakeup:%1ms delay:%2").arg(wakeup.count()).arg(delay));
    }

    if (downloader)
    {
        downloader->Cancel();
        delete downloader;
    }

    RunEpilog();

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
}
