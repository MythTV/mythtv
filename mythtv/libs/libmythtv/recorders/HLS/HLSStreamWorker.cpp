#include "HLSReader.h"
#include "HLSStreamWorker.h"

#define LOC QString("%1 worker: ").arg(m_parent->StreamURL().isEmpty() ? "Stream" : m_parent->StreamURL())

HLSStreamWorker::HLSStreamWorker(HLSReader *parent)
    : MThread("HLSStream"),
      m_parent(parent)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSStreamWorker::~HLSStreamWorker(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "dtor");
}

void HLSStreamWorker::Cancel(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Cancel -- begin");
    m_cancel = true;
    Wakeup();
    CancelCurrentDownload();
    wait();
    LOG(VB_RECORD, LOG_INFO, LOC + "Cancel -- end");
}

void HLSStreamWorker::CancelCurrentDownload(void)
{
    QMutexLocker locker(&m_downloaderLock);
    if (m_downloader)
        m_downloader->Cancel();
}

void HLSStreamWorker::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run -- begin");
    RunProlog();

    m_downloaderLock.lock();
    m_downloader = new MythSingleDownload;
    m_downloaderLock.unlock();

    std::chrono::milliseconds delay = 0ms;
    int retries = 0;
    while (!m_cancel)
    {
        if (m_parent->FatalError())
        {
            LOG(VB_GENERAL, LOG_CRIT, LOC + "Fatal error detected");
            break;
        }
        if (!m_parent->LoadSegments(*m_downloader))
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("download failed, retry #%1").arg(++retries));

            // Asking QNetworkAccessManager to redownload after a
            // failure seems to result in another failure, even if the
            // segment is now available.  So, create a new instance.
            m_downloaderLock.lock();
            delete m_downloader;
            m_downloader = new MythSingleDownload;
            m_downloaderLock.unlock();

            if (retries == 1)   // first error
                continue;       // will retry immediately
            if (retries > 2)
                m_parent->EnableDebugging();
            if (retries == 10)
                m_parent->ResetSequence();

            delay = 500ms * retries * retries;
            if (delay > 20s)
                delay = 20s;
        }
        else
        {
            retries = 0;
            delay = 11s;
        }

        m_lock.lock();
        if (!m_wokenup && !m_cancel)
        {
            if (delay < 1s)
                LOG(VB_RECORD, LOG_WARNING, LOC + "waiting to retry");
            else
                LOG(VB_RECORD, LOG_DEBUG, LOC + "waiting for work");
            m_waitCond.wait(&m_lock, delay.count());
        }
        m_wokenup = false;
        m_lock.unlock();
    }

    m_downloader->Cancel();
    delete m_downloader;
    m_downloader = nullptr;

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
    RunEpilog();
}
