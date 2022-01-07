// MythTV
#include "mythlogging.h"
#include "http/mythhttpthread.h"
#include "http/mythhttpthreadpool.h"

#define LOC QString("HTTPPool: ")

MythHTTPThreadPool::MythHTTPThreadPool()
{
    // Number of connections processed concurrently
    m_maxThreads = static_cast<size_t>(std::max(QThread::idealThreadCount() * 2, 4));

    // Don't allow more connections than we can process, it causes browsers
    // to open lots of new connections instead of reusing existing ones
    setMaxPendingConnections(static_cast<int>(m_maxThreads));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using maximum %1 threads").arg(m_maxThreads));
}

MythHTTPThreadPool::~MythHTTPThreadPool()
{
    for (auto * thread : m_threads)
    {
        thread->Quit();
        thread->wait();
        delete thread;
    }
}

size_t MythHTTPThreadPool::AvailableThreads() const
{
    if (m_upgradedThreads.size() > m_threads.size())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Threadpool error: upgraded count is higher than total");
    return m_maxThreads - (m_threads.size() - m_upgradedThreads.size());
}

size_t MythHTTPThreadPool::MaxThreads() const
{
    return m_maxThreads;
}

size_t MythHTTPThreadPool::ThreadCount() const
{
    return m_threads.size();
}

void MythHTTPThreadPool::AddThread(MythHTTPThread *Thread)
{
    if (Thread)
        m_threads.emplace_back(Thread);
}

void MythHTTPThreadPool::ThreadFinished()
{
    auto * qthread = dynamic_cast<QThread*>(sender());
    auto found = std::find_if(m_threads.cbegin(), m_threads.cend(),
            [&qthread](MythHTTPThread* Thread) { return Thread->qthread() == qthread; });
    if (found == m_threads.cend())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown HTTP thread finished");
        return;
    }

    // Remove from list of upgraded threads if necessary
    auto upgraded = std::find_if(m_upgradedThreads.cbegin(), m_upgradedThreads.cend(),
            [&qthread](MythHTTPThread* Thread) { return Thread->qthread() == qthread; });
    if (upgraded != m_upgradedThreads.cend())
        m_upgradedThreads.erase(upgraded);

    LOG(VB_HTTP, LOG_INFO, LOC + QString("Deleting thread '%1'").arg((*found)->objectName()));
    delete *found;
    m_threads.erase(found);
}

void MythHTTPThreadPool::ThreadUpgraded(QThread* Thread)
{
    auto found = std::find_if(m_threads.cbegin(), m_threads.cend(),
            [&Thread](MythHTTPThread* QThread) { return QThread->qthread() == Thread; });
    if (found == m_threads.cend())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown thread '%1' upgraded").arg(Thread->objectName()));
        return;
    }

    m_upgradedThreads.emplace_back(*found);
    auto standard = m_threads.size() - m_upgradedThreads.size();
    auto upgraded = m_upgradedThreads.size();
    LOG(VB_HTTP, LOG_INFO, LOC + QString("Thread '%1' upgraded (Standard:%2 Upgraded:%3)")
        .arg(Thread->objectName()).arg(standard).arg(upgraded));

    // There is no reasonable way of limiting the number of upgraded sockets (i.e.
    // websockets) as we have no way of knowing what the client intends to use the
    // socket for when connecting. On the other hand, if we there are a sizeable
    // number of valid clients - do we want to restrict the number of connections...
    if (upgraded > m_maxThreads)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("%1 upgraded sockets - notional max is %2")
            .arg(upgraded).arg(m_maxThreads));
    }
}
