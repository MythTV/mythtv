/*  -*- Mode: c++ -*-
 *
 *   Class MThreadPool
 *
 *   Copyright (C) Daniel Kristjansson 2011
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/** \defgroup mthreadpool MThreadPool
 *  \ingroup libmythbase
 *  \brief Framework for managing threads.
 *
 *  Commit 47d67e6722 added two new classes MThread and MThreadPool to be used
 *  instead of QThread and QThreadPool. These mirror the API's of the Qt classes
 *  but also add a requirement for a QString naming the thread or naming the
 *  runnable which are used for debugging. These wrappers also make sure mysql
 *  connections are torn down properly and have a number of safety checks and
 *  assurances. The destructor for MThread also makes sure that a thread has
 *  stopped running before we allow the QThread to be deleted. And MThreaPool
 *  makes sure all it's threads have stopped running. MThread has a Cleanup()
 *  method that attempts to shut down any threads that are running, it only
 *  works for threads that exit on a call to exit(), which includes any threads
 *  using the Qt event loop. It will also print out the names of the threads
 *  that are still running for debugging purposes.
 *
 *  MThread has one special constructor. It takes a QRunnable* and runs it
 *  instead of the Qt event loop in the default run() implementation. Unlike
 *  MThreadPool and QThreadPool, MThread does not delete this runnable when it
 *  exits. This allows you to run something with less boilerplate code. Just
 *  like with QThread in Qt4 it really is better to use the Qt event loop rather
 *  than write your own. As you don't use the Qt event loop QObject
 *  deleteLater's are not executed until the thread exits, Qt events and timers
 *  do not work making Qt networking classes misbehave and signals/slots are
 *  generally unsafe. However, many of the threads with event loops in MythTV
 *  were written before Qt allowed an event loop outside the main thread so this
 *  syntactic sugar just lets us handle that with less lines of code.
 *
 *  There are a few other differences with respect to Qt's classes other than
 *  requiring a name.
 *
 *  \li MThread does not inherit from QObject, if you need QObject services
 *  in a threading class you will need to inherit from QObject separately. Note
 *  when you inherit from QObject, you must inherit from no more than one
 *  QObject based class and it must be first in the list of classes your class
 *  inherits from (a Qt restriction).
 *
 *  \li QThread doesn't implement most of QThread's static methods, notably
 *  QThread::currentThread(). It does make the underlying QThread available via
 *  the qthread() method. But the usual reason to use QThread::currentThread()
 *  is to check which thread you are running in. For this there is a set of
 *  helper functions is_current_thread(MThread&), is_current_thread(QThread*),
 *  and is_current_thread(MThread*) which you can use instead.
 *
 *  \li MThreadPool does not have reserveThread() and releaseThread()
 *  calls; instead use startReserved(). The reserveThread() and releaseThread()
 *  API is not a good API because it doesn't ensure that your runnable starts
 *  right away even though the thread will eventually not be counted amoung
 *  those forcing other threads to go to the queue rather than running right
 *  away.
 */

// C++ headers
#include <algorithm>

// Qt headers
#include <QCoreApplication>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QPair>
#include <QRunnable>
#include <QRecursiveMutex>
#include <QSet>
#include <QWaitCondition>
#include <utility>

// MythTV headers
#include "mthreadpool.h"
#include "mythlogging.h"
#include "mythtimer.h"
#include "logging.h"
#include "mthread.h"
#include "mythdb.h"

using MPoolEntry = QPair<QRunnable*,QString>;
using MPoolQueue = QList<MPoolEntry>;
using MPoolQueues = QMap<int, MPoolQueue>;

class MPoolThread : public MThread
{
  public:
    MPoolThread(const QString& objectName, MThreadPool &pool, std::chrono::milliseconds timeout) :
        MThread(objectName), m_pool(pool), m_expiryTimeout(timeout)
    {
    }

    void run(void) override // MThread
    {
        RunProlog();

        QMutexLocker locker(&m_lock);
        if (m_doRun && m_runnable == nullptr)
        {
            m_wait.wait(locker.mutex(), (m_expiryTimeout+1ms).count());
        }
        while (m_runnable != nullptr)
        {
            if (!m_runnableName.isEmpty())
                loggingRegisterThread(m_runnableName);

            bool autodelete = m_runnable->autoDelete();
            locker.unlock();
            m_runnable->run();
            locker.relock();
            if (autodelete)
                delete m_runnable;
            if (m_reserved)
            {
                locker.unlock();
                m_pool.ReleaseThread();
                locker.relock();
            }
            m_reserved = false;
            m_runnable = nullptr;

            loggingDeregisterThread();
            loggingRegisterThread(objectName());

            GetMythDB()->GetDBManager()->PurgeIdleConnections(false);
            qApp->processEvents();
            qApp->sendPostedEvents(nullptr, QEvent::DeferredDelete);

            if (m_doRun)
            {
                locker.unlock();
                m_pool.NotifyAvailable(this);
                locker.relock();
            }
            if (m_doRun && m_runnable == nullptr)
            {
                m_wait.wait(locker.mutex(), (m_expiryTimeout+1ms).count());
            }
        }

        m_doRun = false;

        locker.unlock();
        m_pool.NotifyDone(this);
        locker.relock();

        RunEpilog();
    }

    bool SetRunnable(QRunnable *runnable, QString runnableName,
                     bool reserved)
    {
        QMutexLocker locker(&m_lock);
        if (m_doRun && (m_runnable == nullptr))
        {
            m_runnable = runnable;
            m_runnableName = std::move(runnableName);
            m_reserved = reserved;
            m_wait.wakeAll();
            return true;
        }
        return false;
    }

    void Shutdown(void)
    {
        QMutexLocker locker(&m_lock);
        m_doRun = false;
        m_wait.wakeAll();
    }

    QMutex          m_lock;
    QWaitCondition  m_wait;
    MThreadPool    &m_pool;
    std::chrono::milliseconds m_expiryTimeout;
    bool            m_doRun          {true};
    QString         m_runnableName;
    bool            m_reserved       {false};
};

//////////////////////////////////////////////////////////////////////

class MThreadPoolPrivate
{
  public:
    explicit MThreadPoolPrivate(QString name) :
        m_name(std::move(name)) {}

    int GetRealMaxThread(void) const
    {
        return std::max(m_maxThreadCount,1) + m_reserveThread;
    }

    mutable QMutex m_lock;
    QString m_name;
    QWaitCondition m_wait;
    bool m_running          {true};
    std::chrono::milliseconds m_expiryTimeout {2min};
    int  m_maxThreadCount   {QThread::idealThreadCount()};
    int  m_reserveThread    {0};
    int  m_threadsCreated   {0};

    MPoolQueues         m_runQueues;
    QSet<MPoolThread*>  m_availThreads;
    QSet<MPoolThread*>  m_runningThreads;
    QList<MPoolThread*> m_deleteThreads;

    static QRecursiveMutex s_pool_lock;
    static MThreadPool *s_pool;
    static QList<MThreadPool*> s_all_pools;
};

QRecursiveMutex MThreadPoolPrivate::s_pool_lock;
MThreadPool *MThreadPoolPrivate::s_pool = nullptr;
QList<MThreadPool*> MThreadPoolPrivate::s_all_pools;

//////////////////////////////////////////////////////////////////////

MThreadPool::MThreadPool(const QString &name) :
    m_priv(new MThreadPoolPrivate(name))
{
    QMutexLocker locker(&MThreadPoolPrivate::s_pool_lock);
    MThreadPoolPrivate::s_all_pools.push_back(this);
}

MThreadPool::~MThreadPool()
{
    Stop();
    DeletePoolThreads();
    {
        QMutexLocker locker(&MThreadPoolPrivate::s_pool_lock);
        MThreadPoolPrivate::s_all_pools.removeAll(this);
    }
    delete m_priv;
    m_priv = nullptr;
}

void MThreadPool::Stop(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_running = false;
    QSet<MPoolThread*>::iterator it = m_priv->m_availThreads.begin();
    for (; it != m_priv->m_availThreads.end(); ++it)
        (*it)->Shutdown();
    it = m_priv->m_runningThreads.begin();
    for (; it != m_priv->m_runningThreads.end(); ++it)
        (*it)->Shutdown();
    m_priv->m_wait.wakeAll();
}

void MThreadPool::DeletePoolThreads(void)
{
    waitForDone();

    QMutexLocker locker(&m_priv->m_lock);
    for (auto *thread : std::as_const(m_priv->m_availThreads))
    {
        m_priv->m_deleteThreads.push_front(thread);
    }
    m_priv->m_availThreads.clear();

    while (!m_priv->m_deleteThreads.empty())
    {
        MPoolThread *thread = m_priv->m_deleteThreads.back();
        locker.unlock();

        thread->wait();

        locker.relock();
        delete thread;
        if (m_priv->m_deleteThreads.back() == thread)
            m_priv->m_deleteThreads.pop_back();
        else
            m_priv->m_deleteThreads.removeAll(thread);
    }
}

MThreadPool *MThreadPool::globalInstance(void)
{
    QMutexLocker locker(&MThreadPoolPrivate::s_pool_lock);
    if (!MThreadPoolPrivate::s_pool)
        MThreadPoolPrivate::s_pool = new MThreadPool("GlobalPool");
    return MThreadPoolPrivate::s_pool;
}

void MThreadPool::StopAllPools(void)
{
    QMutexLocker locker(&MThreadPoolPrivate::s_pool_lock);
    QList<MThreadPool*>::iterator it;
    for (it = MThreadPoolPrivate::s_all_pools.begin();
         it != MThreadPoolPrivate::s_all_pools.end(); ++it)
    {
        (*it)->Stop();
    }
}

void MThreadPool::ShutdownAllPools(void)
{
    QMutexLocker locker(&MThreadPoolPrivate::s_pool_lock);
    QList<MThreadPool*>::iterator it;
    for (it = MThreadPoolPrivate::s_all_pools.begin();
         it != MThreadPoolPrivate::s_all_pools.end(); ++it)
    {
        (*it)->Stop();
    }
    for (it = MThreadPoolPrivate::s_all_pools.begin();
         it != MThreadPoolPrivate::s_all_pools.end(); ++it)
    {
        (*it)->DeletePoolThreads();
    }
}

void MThreadPool::start(QRunnable *runnable, const QString& debugName, int priority)
{
    QMutexLocker locker(&m_priv->m_lock);
    if (TryStartInternal(runnable, debugName, false))
        return;

    MPoolQueues::iterator it = m_priv->m_runQueues.find(priority);
    if (it != m_priv->m_runQueues.end())
    {
        (*it).push_back(MPoolEntry(runnable,debugName));
    }
    else
    {
        MPoolQueue list;
        list.push_back(MPoolEntry(runnable,debugName));
        m_priv->m_runQueues[priority] = list;
    }
}

void MThreadPool::startReserved(
    QRunnable *runnable, const QString& debugName,
    std::chrono::milliseconds waitForAvailMS)
{
    QMutexLocker locker(&m_priv->m_lock);
    if (waitForAvailMS > 0ms && m_priv->m_availThreads.empty() &&
        m_priv->m_runningThreads.size() >= m_priv->m_maxThreadCount)
    {
        MythTimer t;
        t.start();
        auto left = waitForAvailMS - t.elapsed();
        while (left > 0ms && m_priv->m_availThreads.empty() &&
               m_priv->m_runningThreads.size() >= m_priv->m_maxThreadCount)
        {
            m_priv->m_wait.wait(locker.mutex(), left.count());
            left = waitForAvailMS - t.elapsed();
        }
    }
    TryStartInternal(runnable, debugName, true);
}


bool MThreadPool::tryStart(QRunnable *runnable, const QString& debugName)
{
    QMutexLocker locker(&m_priv->m_lock);
    return TryStartInternal(runnable, debugName, false);
}

bool MThreadPool::TryStartInternal(
    QRunnable *runnable, const QString& debugName, bool reserved)
{
    if (!m_priv->m_running)
        return false;

    while (!m_priv->m_deleteThreads.empty())
    {
        m_priv->m_deleteThreads.back()->wait();
        delete m_priv->m_deleteThreads.back();
        m_priv->m_deleteThreads.pop_back();
    }

    for (auto iter = m_priv->m_availThreads.begin();
         iter != m_priv->m_availThreads.end(); )
    {
        MPoolThread *thread = *iter;
        iter = m_priv->m_availThreads.erase(iter);
        m_priv->m_runningThreads.insert(thread);
        if (reserved)
            m_priv->m_reserveThread++;
        if (thread->SetRunnable(runnable, debugName, reserved))
        {
            return true;
        }

        if (reserved)
            m_priv->m_reserveThread--;
        thread->Shutdown();
        m_priv->m_runningThreads.remove(thread);
        m_priv->m_deleteThreads.push_front(thread);
    }

    if (reserved ||
        m_priv->m_runningThreads.size() < m_priv->GetRealMaxThread())
    {
        if (reserved)
            m_priv->m_reserveThread++;
        QString name {QString("%1%2").arg(m_priv->m_name, QString::number(m_priv->m_threadsCreated))};
        m_priv->m_threadsCreated++;
        auto *thread = new MPoolThread(name, *this, m_priv->m_expiryTimeout);
        m_priv->m_runningThreads.insert(thread);
        thread->SetRunnable(runnable, debugName, reserved);
        thread->start();
        if (thread->isRunning())
        {
            return true;
        }

        // Thread failed to run, OOM?
        // QThread will print an error, so we don't have to
        if (reserved)
            m_priv->m_reserveThread--;
        thread->Shutdown();
        m_priv->m_runningThreads.remove(thread);
        m_priv->m_deleteThreads.push_front(thread);
    }

    return false;
}

void MThreadPool::NotifyAvailable(MPoolThread *thread)
{
    QMutexLocker locker(&m_priv->m_lock);

    if (!m_priv->m_running)
    {
        m_priv->m_runningThreads.remove(thread);
        thread->Shutdown();
        m_priv->m_deleteThreads.push_front(thread);
        m_priv->m_wait.wakeAll();
        return;
    }

    MPoolQueues::iterator it = m_priv->m_runQueues.begin();
    if (it == m_priv->m_runQueues.end())
    {
        m_priv->m_runningThreads.remove(thread);
        m_priv->m_availThreads.insert(thread);
        m_priv->m_wait.wakeAll();
        return;
    }

    MPoolEntry e = (*it).front();
    if (!thread->SetRunnable(e.first, e.second, false))
    {
        m_priv->m_runningThreads.remove(thread);
        m_priv->m_wait.wakeAll();
        if (!TryStartInternal(e.first, e.second, false))
        {
            thread->Shutdown();
            m_priv->m_deleteThreads.push_front(thread);
            return;
        }
        thread->Shutdown();
        m_priv->m_deleteThreads.push_front(thread);
    }

    (*it).pop_front();
    if ((*it).empty())
        m_priv->m_runQueues.erase(it);
}

void MThreadPool::NotifyDone(MPoolThread *thread)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_runningThreads.remove(thread);
    m_priv->m_availThreads.remove(thread);
    if (!m_priv->m_deleteThreads.contains(thread))
        m_priv->m_deleteThreads.push_front(thread);
    m_priv->m_wait.wakeAll();
}

std::chrono::milliseconds MThreadPool::expiryTimeout(void) const
{
    QMutexLocker locker(&m_priv->m_lock);
    return m_priv->m_expiryTimeout;
}

void MThreadPool::setExpiryTimeout(std::chrono::milliseconds expiryTimeout)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_expiryTimeout = expiryTimeout;
}

int MThreadPool::maxThreadCount(void) const
{
    QMutexLocker locker(&m_priv->m_lock);
    return m_priv->m_maxThreadCount;
}

void MThreadPool::setMaxThreadCount(int maxThreadCount)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_maxThreadCount = maxThreadCount;
}

int MThreadPool::activeThreadCount(void) const
{
    QMutexLocker locker(&m_priv->m_lock);
    return m_priv->m_availThreads.size() + m_priv->m_runningThreads.size();
}

/*
void MThreadPool::reserveThread(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_reserveThread++;
}

void MThreadPool::releaseThread(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    if (m_priv->m_reserveThread > 0)
        m_priv->m_reserveThread--;
}
*/

void MThreadPool::ReleaseThread(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    if (m_priv->m_reserveThread > 0)
        m_priv->m_reserveThread--;
}

#if 0
static void print_set(QString title, QSet<MPoolThread*> set)
{
    LOG(VB_GENERAL, LOG_INFO, title);
    for (auto item : std::as_const(set))
    {
        LOG(VB_GENERAL, LOG_INFO, QString(" : 0x%1")
            .arg((quint64)item,0,16));
    }
    LOG(VB_GENERAL, LOG_INFO, "");
}
#endif

void MThreadPool::waitForDone(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    while (true)
    {
        while (!m_priv->m_deleteThreads.empty())
        {
            m_priv->m_deleteThreads.back()->wait();
            delete m_priv->m_deleteThreads.back();
            m_priv->m_deleteThreads.pop_back();
        }

        if (m_priv->m_running && !m_priv->m_runQueues.empty())
        {
            m_priv->m_wait.wait(locker.mutex());
            continue;
        }

        QSet<MPoolThread*> working = m_priv->m_runningThreads;
        working = working.subtract(m_priv->m_availThreads);
        if (working.empty())
            break;
        m_priv->m_wait.wait(locker.mutex());
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
