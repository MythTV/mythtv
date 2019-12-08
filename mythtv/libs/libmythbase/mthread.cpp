/*  -*- Mode: c++ -*-
 *
 *   Class MThread
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

#include <iostream>
using namespace std;

// Qt headers
#include <QStringList>
#include <QTimerEvent>
#include <QRunnable>
#include <QtGlobal>
#include <QSet>

// MythTV headers
#include "mythlogging.h"
#include "mythdbcon.h"
#include "mythtimer.h"
#include "mythdate.h"
#include "logging.h"
#include "mthread.h"
#include "mythdb.h"

bool is_current_thread(MThread *thread)
{
    if (!thread)
        return false;
    return QThread::currentThread() == thread->qthread();
}

bool is_current_thread(QThread *thread)
{
    if (!thread)
        return false;
    return QThread::currentThread() == thread;
}

bool is_current_thread(MThread &thread)
{
    return QThread::currentThread() == thread.qthread();
}

class DBPurgeHandler : public QObject
{
  public:
    DBPurgeHandler()
    {
        m_purgeTimer = startTimer(5 * 60000);
    }
    void timerEvent(QTimerEvent *event) override // QObject
    {
        if (event->timerId() == m_purgeTimer)
            GetMythDB()->GetDBManager()->PurgeIdleConnections(false);
    }
    int m_purgeTimer;
};

class MThreadInternal : public QThread
{
  public:
    explicit MThreadInternal(MThread &parent) : m_parent(parent) {}
    void run(void) override { m_parent.run(); } // QThread

    void QThreadRun(void) { QThread::run(); }
    int exec(void)
    {
        DBPurgeHandler ph;
        return QThread::exec();
    }

    static void SetTerminationEnabled(bool enabled = true)
    { QThread::setTerminationEnabled(enabled); }

    static void Sleep(unsigned long time) { QThread::sleep(time); }
    static void MSleep(unsigned long time) { QThread::msleep(time); }
    static void USleep(unsigned long time) { QThread::usleep(time); }

  private:
    MThread &m_parent;
};

static QMutex s_all_threads_lock;
static QSet<MThread*> s_all_threads;

MThread::MThread(const QString &objectName) :
    m_thread(new MThreadInternal(*this))
{
    m_thread->setObjectName(objectName);
    QMutexLocker locker(&s_all_threads_lock);
    s_all_threads.insert(this);
}

MThread::MThread(const QString &objectName, QRunnable *runnable) :
    m_thread(new MThreadInternal(*this)), m_runnable(runnable),
    m_prologExecuted(false), m_epilogExecuted(false)
{
    m_thread->setObjectName(objectName);
    QMutexLocker locker(&s_all_threads_lock);
    s_all_threads.insert(this);
}

MThread::~MThread()
{
    if (!m_prologExecuted)
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("'%1': MThread prolog was never run!").arg(objectName()));
    }
    if (!m_epilogExecuted)
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("'%1': MThread epilog was never run! (%1)").arg(objectName()));
    }
    if (m_thread->isRunning())
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("'%1': MThread destructor called while thread still running! (%1)")
                                          .arg(objectName()));
        m_thread->wait();
    }

    {
        QMutexLocker locker(&s_all_threads_lock);
        s_all_threads.remove(this);
    }

    delete m_thread;
    m_thread = nullptr;
}

void MThread::Cleanup(void)
{
    QMutexLocker locker(&s_all_threads_lock);
    QSet<MThread*> badGuys;
    QSet<MThread*>::const_iterator it;
    for (it = s_all_threads.begin(); it != s_all_threads.end(); ++it)
    {
        if ((*it)->isRunning())
        {
            badGuys.insert(*it);
            (*it)->exit(1);
        }
    }

    if (badGuys.empty())
        return;

    // logging has been stopped so we need to use iostream...
    cerr<<"Error: Not all threads were shut down properly: "<<endl;
    for (it = badGuys.begin(); it != badGuys.end(); ++it)
    {
        cerr<<"Thread "<<qPrintable((*it)->objectName())
            <<" is still running"<<endl;
    }
    cerr<<endl;

    static const int kTimeout = 5000;
    MythTimer t;
    t.start();
    for (it = badGuys.begin();
         it != badGuys.end() && t.elapsed() < kTimeout; ++it)
    {
        int left = kTimeout - t.elapsed();
        if (left > 0)
            (*it)->wait(left);
    }
}

void MThread::GetAllThreadNames(QStringList &list)
{
    QMutexLocker locker(&s_all_threads_lock);
    QSet<MThread*>::const_iterator it;
    for (it = s_all_threads.begin(); it != s_all_threads.end(); ++it)
        list.push_back((*it)->objectName());
}

void MThread::GetAllRunningThreadNames(QStringList &list)
{
    QMutexLocker locker(&s_all_threads_lock);
    QSet<MThread*>::const_iterator it;
    for (it = s_all_threads.begin(); it != s_all_threads.end(); ++it)
    {
        if ((*it)->isRunning())
            list.push_back((*it)->objectName());
    }
}

void MThread::RunProlog(void)
{
    if (QThread::currentThread() != m_thread)
    {
        LOG(VB_GENERAL, LOG_CRIT,
            "RunProlog can only be executed in the run() method of a thread.");
        return;
    }
    setTerminationEnabled(false);
    ThreadSetup(m_thread->objectName());
    m_prologExecuted = true;
}

void MThread::RunEpilog(void)
{
    if (QThread::currentThread() != m_thread)
    {
        LOG(VB_GENERAL, LOG_CRIT,
            "RunEpilog can only be executed in the run() method of a thread.");
        return;
    }
    ThreadCleanup();
    m_epilogExecuted = true;
}

void MThread::ThreadSetup(const QString &name)
{
    loggingRegisterThread(name);
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
    qsrand(MythDate::current().toTime_t() ^ QTime::currentTime().msec());
#elif QT_VERSION < QT_VERSION_CHECK(5,10,0)
    qsrand(MythDate::current().toSecsSinceEpoch() ^ QTime::currentTime().msec());
#endif
}

void MThread::ThreadCleanup(void)
{
    if (GetMythDB() && GetMythDB()->GetDBManager())
        GetMythDB()->GetDBManager()->CloseDatabases();
    loggingDeregisterThread();
}

QThread *MThread::qthread(void)
{
    return m_thread;
}

void MThread::setObjectName(const QString &name)
{
    m_thread->setObjectName(name);
}

QString MThread::objectName(void) const
{
    return m_thread->objectName();
}

void MThread::setPriority(QThread::Priority priority)
{
    m_thread->setPriority(priority);
}

QThread::Priority MThread::priority(void) const
{
    return m_thread->priority();
}

bool MThread::isFinished(void) const
{
    return m_thread->isFinished();
}

bool MThread::isRunning(void) const
{
    return m_thread->isRunning();
}

void MThread::setStackSize(uint stackSize)
{
    m_thread->setStackSize(stackSize);
}

uint MThread::stackSize(void) const
{
    return m_thread->stackSize();
}

void MThread::exit(int retcode)
{
    m_thread->exit(retcode);
}

void MThread::start(QThread::Priority p)
{
    m_prologExecuted = false;
    m_epilogExecuted = false;
    m_thread->start(p);
}

void MThread::terminate(void)
{
    m_thread->terminate();
}

void MThread::quit(void)
{
    m_thread->quit();
}

bool MThread::wait(unsigned long time)
{
    if (m_thread->isRunning())
        return m_thread->wait(time);
    return true;
}

void MThread::run(void)
{
    RunProlog();
    if (m_runnable)
        m_runnable->run();
    else
        m_thread->QThreadRun();
    RunEpilog();
}

int MThread::exec(void)
{
    return m_thread->exec();
}

void MThread::setTerminationEnabled(bool enabled)
{
    MThreadInternal::SetTerminationEnabled(enabled);
}

void MThread::sleep(unsigned long time)
{
    MThreadInternal::Sleep(time);
}

void MThread::msleep(unsigned long time)
{
    MThreadInternal::MSleep(time);
}

void MThread::usleep(unsigned long time)
{
    MThreadInternal::USleep(time);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
