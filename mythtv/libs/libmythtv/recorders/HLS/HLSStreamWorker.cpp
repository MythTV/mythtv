#include "HLSReader.h"
#include "HLSStreamWorker.h"

#define LOC QString("%1 worker: ").arg(m_parent->StreamURL().isEmpty() ? "Stream" : m_parent->StreamURL())

HLSStreamWorker::HLSStreamWorker(HLSReader *parent)
    : MThread("HLSStream"),
      m_parent(parent), m_downloader(NULL),
      m_cancel(false), m_wokenup(false)
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
    wait();
    LOG(VB_RECORD, LOG_INFO, LOC + "Cancel -- end");
}

void HLSStreamWorker::CancelCurrentDownload(void)
{
    if (m_downloader)
        m_downloader->Cancel();
}

void HLSStreamWorker::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run -- begin");
    RunProlog();

    m_downloader = new MythSingleDownload;

    int retries = 0;
    while (!m_cancel)
    {
        if (!m_parent->LoadSegments(*m_downloader))
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("download failed, retry #%1").arg(++retries));

            if (retries == 1)   // first error
                continue;       // will retry immediately
            usleep(500000);     // sleep 0.5s
            if (retries == 2)   // and retry once again
                continue;

            // TODO: Switch to another stream?
            retries = 0;
        }

        m_lock.lock();
        if (!m_wokenup && !m_cancel)
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC + "waiting for work");
            m_waitcond.wait(&m_lock, 600000);
        }
        m_wokenup = false;
        m_lock.unlock();
    }

    m_downloader->Cancel();
    delete m_downloader;
    m_downloader = NULL;

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
    RunEpilog();
}
