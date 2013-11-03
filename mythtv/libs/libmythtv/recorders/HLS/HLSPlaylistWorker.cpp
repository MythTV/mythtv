#include "HLSPlaylistWorker.h"
#include "HLSReader.h"

const int PLAYLIST_FAILURE = 60;  // number of consecutive failures after which
                                  // recording will abort

#define LOC QString("%1 playlist: ").arg(m_parent->StreamURL().isEmpty() ? "Worker" : m_parent->StreamURL())

HLSPlaylistWorker::HLSPlaylistWorker(HLSReader *parent)
    :  MThread("HLSPlaylist"), m_parent(parent),
       m_cancel(false), m_wokenup(false)
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
    int64_t     wakeup = 1000;
    int         retries = 0;
    double      delay = 0;

    LOG(VB_RECORD, LOG_DEBUG, LOC + "run -- begin");

    RunProlog();

    MythSingleDownload downloader;

    while (!m_cancel)
    {
        m_lock.lock();
        if (!m_wokenup)
        {
            unsigned long waittime = wakeup < 1000 ? 1000 : wakeup;
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("refreshing in %2s")
                .arg(waittime / 1000.0));
            m_waitcond.wait(&m_lock, waittime);
        }
        m_wokenup = false;
        m_lock.unlock();

        if (m_cancel)
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "canceled");
            break;
        }

        if (m_parent->LoadMetaPlaylists(downloader))
        {
            retries = 0;
            delay = 0.5;
        }
        else
        {
            if (++retries > PLAYLIST_FAILURE)
            {
                LOG(VB_RECORD, LOG_ERR, LOC +
                    QString("Reloading failed %1 times."
                                "aborting.").arg(PLAYLIST_FAILURE));
                m_parent->HadError();
            }

            if (m_parent->PercentBuffered() > 85)
            {
                // Can't wait
                if (retries == 1)
                    continue; // restart immediately if it's the first try
                retries = 0;
                delay = 0.5;
            }
            else if (retries == 1)
                delay = 0.5;
            else if (retries == 2)
                delay = 1;
            else
                delay = 2;
        }

        // When should the playlist be reloaded
        wakeup = m_parent->TargetDuration() * delay * (int64_t)1000;
        if (wakeup < 0)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "Reader in bad state.  Aborting");
            m_parent->HadError();
            m_cancel = true;
            break;
        }
    }

    downloader.Cancel();

    RunEpilog();

    LOG(VB_RECORD, LOG_DEBUG, LOC + "run -- end");
}
