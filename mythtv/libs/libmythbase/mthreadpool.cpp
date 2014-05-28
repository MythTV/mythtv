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
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QWaitCondition>
#include <QMutexLocker>
#include <QRunnable>
#include <QMutex>
#include <QList>
#include <QPair>
#include <QMap>
#include <QSet>

// MythTV headers
#include "mthreadpool.h"
#include "mythlogging.h"
#include "mythtimer.h"
#include "logging.h"
#include "mthread.h"
#include "mythdb.h"

typedef QPair<QRunnable*,QString> MPoolEntry;
typedef QList<MPoolEntry> MPoolQueue;
typedef QMap<int, MPoolQueue> MPoolQueues;

class MPoolThread : public MThread
{
  public:
    MPoolThread(MThreadPool &pool, int timeout) :
        MThread("PT"), m_pool(pool), m_expiry_timeout(timeout),
        m_do_run(true), m_reserved(false)
    {
        QMutexLocker locker(&s_lock);
        setObjectName(QString("PT%1").arg(s_thread_num));
        s_thread_num++;
    }

    void run(void)
    {
        RunProlog();

        MythTimer t;
        t.start();
        QMutexLocker locker(&m_lock);
        while (true)
        {
            if (m_do_run && !m_runnable)
                m_wait.wait(locker.mutex(), m_expiry_timeout+1);

            if (!m_runnable)
            {
                m_do_run = false;

                locker.unlock();
                m_pool.NotifyDone(this);
                locker.relock();
                break;
            }

            if (!m_runnable_name.isEmpty())
                loggingRegisterThread(m_runnable_name);

            bool autodelete = m_runnable->autoDelete();
            m_runnable->run();
            if (autodelete)
                delete m_runnable;
            if (m_reserved)
                m_pool.ReleaseThread();
            m_reserved = false;
            m_runnable = NULL;

            loggingDeregisterThread();
            loggingRegisterThread(objectName());

            GetMythDB()->GetDBManager()->PurgeIdleConnections(false);
            qApp->processEvents();
            qApp->sendPostedEvents(NULL, QEvent::DeferredDelete);

            t.start();

            if (m_do_run)
            {
                locker.unlock();
                m_pool.NotifyAvailable(this);
                locker.relock();
            }
            else
            {
                locker.unlock();
                m_pool.NotifyDone(this);
                locker.relock();
                break;
            }
        }

        RunEpilog();
    }

    bool SetRunnable(QRunnable *runnable, QString runnableName,
                     bool reserved)
    {
        QMutexLocker locker(&m_lock);
        if (m_do_run && (m_runnable == NULL))
        {
            m_runnable = runnable;
            m_runnable_name = runnableName;
            m_reserved = reserved;
            m_wait.wakeAll();
            return true;
        }
        return false;
    }

    void Shutdown(void)
    {
        QMutexLocker locker(&m_lock);
        m_do_run = false;
        m_wait.wakeAll();
    }

    QMutex m_lock;
    QWaitCondition m_wait;
    MThreadPool &m_pool;
    int m_expiry_timeout;
    bool m_do_run;
    QString m_runnable_name;
    bool m_reserved;

    static QMutex s_lock;
    static uint s_thread_num;
};
QMutex MPoolThread::s_lock;
uint MPoolThread::s_thread_num = 0;

//////////////////////////////////////////////////////////////////////

class MThreadPoolPrivate
{
  public:
    MThreadPoolPrivate(const QString &name) :
        m_name(name),
        m_running(true),
        m_expiry_timeout(120 * 1000),
        m_max_thread_count(QThread::idealThreadCount()),
        m_reserve_thread(0)
    {
    }

    int GetRealMaxThread(void)
    {
        return max(m_max_thread_count,1) + m_reserve_thread;
    }

    mutable QMutex m_lock;
    QString m_name;
    QWaitCondition m_wait;
    bool m_running;
    int m_expiry_timeout;
    int m_max_thread_count;
    int m_reserve_thread;

    MPoolQueues m_run_queues;
    QSet<MPoolThread*> m_avail_threads;
    QSet<MPoolThread*> m_running_threads;
    QList<MPoolThread*> m_delete_threads;

    static QMutex s_pool_lock;
    static MThreadPool *s_pool;
    static QList<MThreadPool*> s_all_pools;
};

QMutex MThreadPoolPrivate::s_pool_lock(QMutex::Recursive);
MThreadPool *MThreadPoolPrivate::s_pool = NULL;
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
    m_priv = NULL;
}

void MThreadPool::Stop(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_running = false;
    QSet<MPoolThread*>::iterator it = m_priv->m_avail_threads.begin();
    for (; it != m_priv->m_avail_threads.end(); ++it)
        (*it)->Shutdown();
    it = m_priv->m_running_threads.begin();
    for (; it != m_priv->m_running_threads.end(); ++it)
        (*it)->Shutdown();
    m_priv->m_wait.wakeAll();
}

void MThreadPool::DeletePoolThreads(void)
{
    waitForDone();

    QMutexLocker locker(&m_priv->m_lock);
    QSet<MPoolThread*>::iterator it = m_priv->m_avail_threads.begin();
    for (; it != m_priv->m_avail_threads.end(); ++it)
    {
        m_priv->m_delete_threads.push_front(*it);
    }
    m_priv->m_avail_threads.clear();

    while (!m_priv->m_delete_threads.empty())
    {
        MPoolThread *thread = m_priv->m_delete_threads.back();
        locker.unlock();

        thread->wait();

        locker.relock();
        delete thread;
        if (m_priv->m_delete_threads.back() == thread)
            m_priv->m_delete_threads.pop_back();
        else
            m_priv->m_delete_threads.removeAll(thread);
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

void MThreadPool::start(QRunnable *runnable, QString debugName, int priority)
{
    QMutexLocker locker(&m_priv->m_lock);
    if (TryStartInternal(runnable, debugName, false))
        return;

    MPoolQueues::iterator it = m_priv->m_run_queues.find(priority);
    if (it != m_priv->m_run_queues.end())
    {
        (*it).push_back(MPoolEntry(runnable,debugName));
    }
    else
    {
        MPoolQueue list;
        list.push_back(MPoolEntry(runnable,debugName));
        m_priv->m_run_queues[priority] = list;
    }
}

void MThreadPool::startReserved(
    QRunnable *runnable, QString debugName, int waitForAvailMS)
{
    QMutexLocker locker(&m_priv->m_lock);
    if (waitForAvailMS > 0 && m_priv->m_avail_threads.empty() &&
        m_priv->m_running_threads.size() >= m_priv->m_max_thread_count)
    {
        MythTimer t;
        t.start();
        int left = waitForAvailMS - t.elapsed();
        while (left > 0 && m_priv->m_avail_threads.empty() &&
               m_priv->m_running_threads.size() >= m_priv->m_max_thread_count)
        {
            m_priv->m_wait.wait(locker.mutex(), left);
            left = waitForAvailMS - t.elapsed();
        }
    }
    TryStartInternal(runnable, debugName, true);
}


bool MThreadPool::tryStart(QRunnable *runnable, QString debugName)
{
    QMutexLocker locker(&m_priv->m_lock);
    return TryStartInternal(runnable, debugName, false);
}

bool MThreadPool::TryStartInternal(
    QRunnable *runnable, QString debugName, bool reserved)
{
    if (!m_priv->m_running)
        return false;

    while (!m_priv->m_delete_threads.empty())
    {
        m_priv->m_delete_threads.back()->wait();
        delete m_priv->m_delete_threads.back();
        m_priv->m_delete_threads.pop_back();
    }

    while (m_priv->m_avail_threads.begin() != m_priv->m_avail_threads.end())
    {
        MPoolThread *thread = *m_priv->m_avail_threads.begin();
        m_priv->m_avail_threads.erase(m_priv->m_avail_threads.begin());
        m_priv->m_running_threads.insert(thread);
        if (reserved)
            m_priv->m_reserve_thread++;
        if (thread->SetRunnable(runnable, debugName, reserved))
        {
            return true;
        }
        else
        {
            if (reserved)
                m_priv->m_reserve_thread--;
            thread->Shutdown();
            m_priv->m_running_threads.remove(thread);
            m_priv->m_delete_threads.push_front(thread);
        }
    }

    if (reserved ||
        m_priv->m_running_threads.size() < m_priv->GetRealMaxThread())
    {
        if (reserved)
            m_priv->m_reserve_thread++;
        MPoolThread *thread = new MPoolThread(*this, m_priv->m_expiry_timeout);
        m_priv->m_running_threads.insert(thread);
        thread->SetRunnable(runnable, debugName, reserved);
        thread->start();
        if (thread->isRunning())
        {
            return true;
        }
        else
        {
            // Thread failed to run, OOM?
            // QThread will print an error, so we don't have to
            if (reserved)
                m_priv->m_reserve_thread--;
            thread->Shutdown();
            m_priv->m_running_threads.remove(thread);
            m_priv->m_delete_threads.push_front(thread);
        }
    }

    return false;
}

void MThreadPool::NotifyAvailable(MPoolThread *thread)
{
    QMutexLocker locker(&m_priv->m_lock);

    if (!m_priv->m_running)
    {
        m_priv->m_running_threads.remove(thread);
        thread->Shutdown();
        m_priv->m_delete_threads.push_front(thread);
        m_priv->m_wait.wakeAll();
        return;
    }

    MPoolQueues::iterator it = m_priv->m_run_queues.begin();
    if (it == m_priv->m_run_queues.end())
    {
        m_priv->m_running_threads.remove(thread);
        m_priv->m_avail_threads.insert(thread);
        m_priv->m_wait.wakeAll();
        return;
    }

    MPoolEntry e = (*it).front();
    if (!thread->SetRunnable(e.first, e.second, false))
    {
        m_priv->m_running_threads.remove(thread);
        m_priv->m_wait.wakeAll();
        if (!TryStartInternal(e.first, e.second, false))
        {
            thread->Shutdown();
            m_priv->m_delete_threads.push_front(thread);
            return;
        }
        thread->Shutdown();
        m_priv->m_delete_threads.push_front(thread);
    }

    (*it).pop_front();
    if ((*it).empty())
        m_priv->m_run_queues.erase(it);
}

void MThreadPool::NotifyDone(MPoolThread *thread)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_running_threads.remove(thread);
    m_priv->m_avail_threads.remove(thread);
    if (!m_priv->m_delete_threads.contains(thread))
        m_priv->m_delete_threads.push_front(thread);
    m_priv->m_wait.wakeAll();
}

int MThreadPool::expiryTimeout(void) const
{
    QMutexLocker locker(&m_priv->m_lock);
    return m_priv->m_expiry_timeout;
}

void MThreadPool::setExpiryTimeout(int expiryTimeout)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_expiry_timeout = expiryTimeout;
}

int MThreadPool::maxThreadCount(void) const
{
    QMutexLocker locker(&m_priv->m_lock);
    return m_priv->m_max_thread_count;
}

void MThreadPool::setMaxThreadCount(int maxThreadCount)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_max_thread_count = maxThreadCount;
}

int MThreadPool::activeThreadCount(void) const
{
    QMutexLocker locker(&m_priv->m_lock);
    return m_priv->m_avail_threads.size() + m_priv->m_running_threads.size();
}

/*
void MThreadPool::reserveThread(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    m_priv->m_reserve_thread++;
}

void MThreadPool::releaseThread(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    if (m_priv->m_reserve_thread > 0)
        m_priv->m_reserve_thread--;
}
*/

void MThreadPool::ReleaseThread(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    if (m_priv->m_reserve_thread > 0)
        m_priv->m_reserve_thread--;
}

#if 0
static void print_set(QString title, QSet<MPoolThread*> set)
{
    LOG(VB_GENERAL, LOG_INFO, title);
    QSet<MPoolThread*>::iterator it = set.begin();
    for (; it != set.end(); ++it)
    {
        LOG(VB_GENERAL, LOG_INFO, QString(" : 0x%1")
            .arg((quint64)(*it),0,16));
    }
    LOG(VB_GENERAL, LOG_INFO, "");
}
#endif

void MThreadPool::waitForDone(void)
{
    QMutexLocker locker(&m_priv->m_lock);
    while (true)
    {
        while (!m_priv->m_delete_threads.empty())
        {
            m_priv->m_delete_threads.back()->wait();
            delete m_priv->m_delete_threads.back();
            m_priv->m_delete_threads.pop_back();
        }

        if (m_priv->m_running && !m_priv->m_run_queues.empty())
        {
            m_priv->m_wait.wait(locker.mutex());
            continue;
        }

        QSet<MPoolThread*> working = m_priv->m_running_threads;
        working = working.subtract(m_priv->m_avail_threads);
        if (working.empty())
            break;
        m_priv->m_wait.wait(locker.mutex());
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
