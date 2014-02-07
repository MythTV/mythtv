/* -*- Mode: c++ -*-
*
* Class HouseKeeperTask
* Class HouseKeeperThread
* Class HouseKeeper
*
* Copyright (C) Raymond Wagner 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/



/** \defgroup housekeeper HouseKeeper
 *  \ingroup libmythbase
 *  \brief Framework for handling frequently run background tasks
 *
 *  This utility provides the ability to call maintenance tasks in the
 *  background at regular intervals. The basic operation consists of a
 *  manager class to store the definitions for the various tasks, the
 *  task definitions themselves, and one or more threads to run them in.
 *
 *  The manager does not get its own thread, but rather is called at one
 *  minute granularity by a timer and the main application thread. This
 *  necessitates that the tasks performed by the manager be kept to a
 *  minimum. The manager loops through all task definitions, and checks
 *  to see if any are ready to be run. If any are ready, they are copied
 *  to the queue, and the run thread is woken up.
 *
 *  The run thread sequentially steps through all tasks in the queue and
 *  then goes back to sleep. If one run thread is still active by the time
 *  the timer triggers a new pass, the existing thread will be discarded,
 *  and a new thread will be started to process remaining tasks in the queue.
 *  The old thread will terminate itself once finished with its current task,
 *  rather than following through with the next item in the queue, or going
 *  into wait. This allows the manager to deal with both time-sensitive
 *  tasks and long duration tasks, without any special knowledge of behavior.
 */


#include <QMutexLocker>

#include "mythevent.h"
#include "mythdbcon.h"
#include "housekeeper.h"
#include "mythcorecontext.h"

/** \class HouseKeeperTask
 *  \ingroup housekeeper
 *  \brief Definition for a single task to be run by the HouseKeeper
 *
 *  This class contains instructions for tasks to be run periodically by the 
 *  housekeeper. Each task requires an indentification tag, and can be given
 *  a two options to control scope and startup behavior. Each child class
 *  should override at least two methods: DoCheckRun() and DoRun().
 *
 *  DoCheckRun() is to perform a check as to whether the task should be run,
 *  returning a boolean 'true' if it should.
 *
 *  DoRun() actually implements the task to be run.
 *
 *  Child classes can also implement a Terminate() method, which is to be used
 *  to stop in-progress tasks when the application is shutting down.
 *
 *  The HouseKeeperScope attribute passed to the class in the constructor
 *  controls in what scope the task should operate. <b>kHKGlobal</b> means the
 *  task should only operate once globally. If another housekeeper in another
 *  or on another machine runs a task, this instance should track that and not
 *  run itself until the next time to run comes. <b>kHKLocal</b> has similar
 *  meaning, but only on the local machine, rather than the whole MythTV
 *  cluster. <b>kHKInst</b> means no communication is necessary, as each
 *  process instance is to operate independently.
 *
 *  The HouseKeeperStartup attribute passed to the class in the constructor
 *  controls the startup behavior of the task. <b>kHKNormal</b> means the task
 *  will never run until application control has passed to the main event loop
 *  and the HouseKeeper's check scan has run at least once.
 *  <b>kHKRunOnStartup</b> means the task is queued to run when on startup,
 *  and will be run as soon as a HouseKeeperThread is started to handle it.
 *  <b>kHKRunImmediatelyOnStartup</b> means the task will be run immediately
 *  in the primary thread. <b>Do not use this for long tasks as it will delay
 *  application startup and handoff to the main event loop.</b>
 */
HouseKeeperTask::HouseKeeperTask(const QString &dbTag, HouseKeeperScope scope,
                                 HouseKeeperStartup startup):
    ReferenceCounter(dbTag), m_dbTag(dbTag), m_confirm(false), m_scope(scope),
    m_startup(startup), m_running(false), m_lastRun(MythDate::fromTime_t(0)),
    m_lastSuccess(MythDate::fromTime_t(0)),
    m_lastUpdate(MythDate::fromTime_t(0))
{
}

bool HouseKeeperTask::CheckRun(QDateTime now)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Checking to run %1").arg(GetTag()));
    bool check = false;
    if (!m_confirm && !m_running && (check = DoCheckRun(now)))
        // if m_confirm is already set, the task is already in the queue
        // and should not be queued a second time
        m_confirm = true;
    return check;
}

bool HouseKeeperTask::CheckImmediate(void)
{
    return ((m_startup == kHKRunImmediateOnStartup) &&
            DoCheckRun(MythDate::current()));
}

bool HouseKeeperTask::CheckStartup(void)
{
    if ((m_startup == kHKRunOnStartup) && DoCheckRun(MythDate::current()))
    {
        m_confirm = true;
        return true;
    }
    return false;
}

bool HouseKeeperTask::Run(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Running HouseKeeperTask '%1'.")
                                .arg(m_dbTag));
    if (m_running)
    {
        // something else is already running me, bail out
        LOG(VB_GENERAL, LOG_WARNING, QString("HouseKeeperTask '%1' already "
                "running. Refusing to run concurrently").arg(m_dbTag));
        return false;
    }

    m_running = true;
    bool res = DoRun();
    m_running = false;
    if (!res)
        LOG(VB_GENERAL, LOG_INFO, QString("HouseKeeperTask '%1' Failed.")
                                .arg(m_dbTag));
    else
        LOG(VB_GENERAL, LOG_INFO,
                QString("HouseKeeperTask '%1' Finished Successfully.")
                    .arg(m_dbTag));
    return res;
}

QDateTime HouseKeeperTask::QueryLastRun(void)
{
    QueryLast();
    return m_lastRun;
}

QDateTime HouseKeeperTask::QueryLastSuccess(void)
{
    QueryLast();
    return m_lastRun;
}

void HouseKeeperTask::QueryLast(void)
{
    if (m_scope != kHKInst)
    {
        if (m_lastUpdate.addSecs(30) > MythDate::current())
            // just to cut down on unnecessary queries
            return;

        MSqlQuery query(MSqlQuery::InitCon());

        m_lastRun = MythDate::fromTime_t(0);
        m_lastSuccess = MythDate::fromTime_t(0);

        if (m_scope == kHKGlobal)
        {
            query.prepare("SELECT lastrun,lastsuccess FROM housekeeping"
                          " WHERE tag = :TAG"
                          "   AND hostname IS NULL");
        }
        else
        {
            query.prepare("SELECT lastrun,lastsuccess FROM housekeeping"
                          " WHERE tag = :TAG"
                          "   AND hostname = :HOST");
            query.bindValue(":HOST", gCoreContext->GetHostName());
        }

        query.bindValue(":TAG", m_dbTag);

        if (query.exec() && query.next())
        {
            m_lastRun = MythDate::as_utc(query.value(0).toDateTime());
            m_lastSuccess = MythDate::as_utc(query.value(1).toDateTime());
        }
    }

    m_lastUpdate = MythDate::current();
}

QDateTime HouseKeeperTask::UpdateLastRun(QDateTime last, bool successful)
{
    m_lastRun = last;
    if (successful)
        m_lastSuccess = last;
    m_confirm = false;

    if (m_scope != kHKInst)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        if (!query.isConnected())
            return last;

        if (m_lastRun == MythDate::fromTime_t(0))
        {
            // not previously set, perform insert

            if (m_scope == kHKGlobal)
                query.prepare("INSERT INTO housekeeping"
                              "         (tag, lastrun, lastsuccess)"
                              "     VALUES (:TAG, :TIME, :STIME)");
            else
                query.prepare("INSERT INTO housekeeping"
                              "         (tag, hostname, lastrun, lastsuccess)"
                              "     VALUES (:TAG, :HOST, :TIME, :STIME)");
        }
        else
        {
            // previously set, perform update

            if (m_scope == kHKGlobal)
                query.prepare("UPDATE housekeeping SET lastrun=:TIME,"
                              "                        lastsuccess=:STIME"
                              " WHERE tag = :TAG"
                              "   AND hostname IS NULL");
            else
                query.prepare("UPDATE housekeeping SET lastrun=:TIME,"
                              "                        lastsuccess=:STIME"
                              " WHERE tag = :TAG"
                              "   AND hostname = :HOST");
        }

        if (m_scope == kHKGlobal)
            LOG(VB_GENERAL, LOG_DEBUG,
                    QString("Updating global run time for %1").arg(m_dbTag));
        else
            LOG(VB_GENERAL, LOG_DEBUG,
                    QString("Updating local run time for %1").arg(m_dbTag));

        if (m_scope == kHKLocal)
            query.bindValue(":HOST", gCoreContext->GetHostName());
        query.bindValue(":TAG", m_dbTag);
        query.bindValue(":TIME", MythDate::as_utc(m_lastRun));
        query.bindValue(":STIME", MythDate::as_utc(m_lastSuccess));

        if (!query.exec())
            MythDB::DBError("HouseKeeperTask::updateLastRun", query);
    }

    QString msg;
    if (successful)
        msg = QString("HOUSE_KEEPER_SUCCESSFUL %1 %2 %3");
    else
        msg = QString("HOUSE_KEEPER_RUNNING %1 %2 %3");
    msg = msg.arg(gCoreContext->GetHostName())
             .arg(m_dbTag)
             .arg(MythDate::toString(last, MythDate::ISODate));
    gCoreContext->SendEvent(MythEvent(msg));
    m_lastUpdate = MythDate::current();

    return last;
}

void HouseKeeperTask::SetLastRun(QDateTime last, bool successful)
{
    m_lastRun = last;
    if (successful)
        m_lastSuccess = last;

    m_lastUpdate = MythDate::current();
}

/** \class PeriodicHouseKeeperTask
 *  \ingroup housekeeper
 *  \brief Modified HouseKeeperTask for tasks to be run at a regular interval.
 *
 *  This task type is to be used for tasks that need to be run at a regular
 *  interval. The <i>period</i> argument in the constructor defines this
 *  interval in integer seconds. The <i>min</i> and <i>max</i> attributes
 *  control the region over which the task can be run, given as a float
 *  multiple of the period. The defaults are 0.5-1.1. The test for running
 *  is a linearly increasing probability from the start to end of this period,
 *  and the task is forced to run if the end of the period is reached.
 *
 */
PeriodicHouseKeeperTask::PeriodicHouseKeeperTask(const QString &dbTag,
            int period, float min, float max, int retry,
            HouseKeeperScope scope, HouseKeeperStartup startup) :
    HouseKeeperTask(dbTag, scope, startup), m_period(period), m_retry(retry),
    m_windowPercent(min, max), m_currentProb(1.0)
{
    CalculateWindow();
    if (m_retry == 0)
        m_retry = m_period;
}

void PeriodicHouseKeeperTask::CalculateWindow(void)
{
    int period = m_period;
    if (GetLastRun() > GetLastSuccess())
        // last attempt was not successful
        // try shortened period
        period = m_retry;

    m_windowElapsed.first =
                    (uint32_t)((float)period * m_windowPercent.first);
    m_windowElapsed.second =
                    (uint32_t)((float)period * m_windowPercent.second);
}

void PeriodicHouseKeeperTask::SetWindow(float min, float max)
{
    m_windowPercent.first = min;
    m_windowPercent.second = max;
    CalculateWindow();
}

QDateTime PeriodicHouseKeeperTask::UpdateLastRun(QDateTime last,
                                                 bool successful)
{
    QDateTime res = HouseKeeperTask::UpdateLastRun(last, successful);
    CalculateWindow();
    m_currentProb = 1.0;
    return res;
}

void PeriodicHouseKeeperTask::SetLastRun(QDateTime last, bool successful)
{
    HouseKeeperTask::SetLastRun(last, successful);
    CalculateWindow();
    m_currentProb = 1.0;
}

bool PeriodicHouseKeeperTask::DoCheckRun(QDateTime now)
{
    int elapsed = GetLastRun().secsTo(now);

    if (elapsed < 0)
        // something bad has happened. let's just move along
        return false;

    if (elapsed < m_windowElapsed.first)
        // insufficient time elapsed to test
        return false;
    if (elapsed > m_windowElapsed.second)
        // too much time has passed. force run
        return true;

    // calculate probability that task should not have yet run
    // it's backwards, but it makes the math simplier
    float prob = 1.0 - ((float)(elapsed - m_windowElapsed.first) / 
                    (float)(m_windowElapsed.second - m_windowElapsed.first));
    if (m_currentProb < prob)
        // more bad stuff
        return false;

    // calculate current probability to achieve overall probability
    // this should be nearly one
    float prob2 = prob/m_currentProb;
    // so rand() should have to return nearly RAND_MAX to get a positive
    // remember, this is computing the probability that up to this point, one
    //      of these tests has returned positive, so each individual test has
    //      a necessarily low probability
    bool res = (rand() > (int)(prob2 * RAND_MAX));
    m_currentProb = prob;
//  if (res)
//      LOG(VB_GENERAL, LOG_DEBUG, QString("%1 will run: this=%2; total=%3")
//                  .arg(GetTag()).arg(1-prob2).arg(1-prob));
//  else
//      LOG(VB_GENERAL, LOG_DEBUG, QString("%1 will not run: this=%2; total=%3")
//                  .arg(GetTag()).arg(1-prob2).arg(1-prob));
    return res;
}

bool PeriodicHouseKeeperTask::InWindow(QDateTime now)
{
    int elapsed = GetLastRun().secsTo(now);

    if (elapsed < 0)
        // something bad has happened. let's just move along
        return false;

    if ((elapsed > m_windowElapsed.first) &&
        (elapsed < m_windowElapsed.second))
            return true;

    return false;
}

bool PeriodicHouseKeeperTask::PastWindow(QDateTime now)
{
    if (GetLastRun().secsTo(now) > m_windowElapsed.second)
        return true;
    return false;
}

/** \class DailyHouseKeeperTask
 *  \ingroup housekeeper
 *  \brief Modified PeriodicHouseKeeperTask for tasks to be run once daily.
 *
 *  This task type is for tasks that should only be run once daily. It follows
 *  the same behavior as the PeriodicHouseKeeperTask, but will restrict the
 *  run window to prevent a task from running a second time on the same day,
 *  and forces a task to at least thirty minutes before midnight if it has not
 *  yet run that day. This class supports a <i>minhour</i> and <i>maxhour<i>
 *  integer inputs to the constructer to allow further limiting of the window
 *  in which the task is allowed to run.
 *
 */
DailyHouseKeeperTask::DailyHouseKeeperTask(const QString &dbTag,
        HouseKeeperScope scope, HouseKeeperStartup startup) :
    PeriodicHouseKeeperTask(dbTag, 86400, .5, 1.5, 0, scope, startup),
    m_windowHour(0, 23)
{
    CalculateWindow();
}

DailyHouseKeeperTask::DailyHouseKeeperTask(const QString &dbTag, int minhour,
        int maxhour, HouseKeeperScope scope, HouseKeeperStartup startup) :
    PeriodicHouseKeeperTask(dbTag, 86400, .5, 1.5, 0, scope, startup),
    m_windowHour(minhour, maxhour)
{
    CalculateWindow();
}

void DailyHouseKeeperTask::CalculateWindow(void)
{
    PeriodicHouseKeeperTask::CalculateWindow();
    QDate date = GetLastRun().addDays(1).date();

    QDateTime tmp = QDateTime(date, QTime(m_windowHour.first, 0));
    if (GetLastRun().addSecs(m_windowElapsed.first) < tmp)
        m_windowElapsed.first = GetLastRun().secsTo(tmp);

    tmp = QDateTime(date, QTime(m_windowHour.second, 30));
    // we want to make sure this gets run before the end of the day
    // so add a 30 minute buffer prior to the end of the window
    if (GetLastRun().addSecs(m_windowElapsed.second) > tmp)
        m_windowElapsed.second = GetLastRun().secsTo(tmp);

    LOG(VB_GENERAL, LOG_DEBUG, QString("%1 Run window between %2 - %3.")
        .arg(GetTag()).arg(m_windowElapsed.first).arg(m_windowElapsed.second));
}

void DailyHouseKeeperTask::SetHourWindow(int min, int max)
{
    m_windowHour.first = min;
    m_windowHour.second = max;
    CalculateWindow();
}

bool DailyHouseKeeperTask::InWindow(QDateTime now)
{
    if (PeriodicHouseKeeperTask::InWindow(now))
        // parent says we're in the window
        return true;

    int hour = now.time().hour();
    if (PastWindow(now) && (m_windowHour.first <= hour)
                        && (m_windowHour.second > hour))
        // we've missed the window, but we're within our time constraints
        return true;

    return false;
}

/** \class HouseKeepingThread
 *  \ingroup housekeeper
 *  \brief Thread used to perform queued HouseKeeper tasks.
 *
 *  This class is a long-running thread that pulls tasks out of the
 *  HouseKeeper queue and runs them sequentially. It performs one last check
 *  of the task to make sure something else in the same scope has not
 *  pre-empted it, before running the task.
 *
 */
void HouseKeepingThread::run(void)
{
    RunProlog();
    m_waitMutex.lock();
    HouseKeeperTask *task = NULL;

    while (m_keepRunning)
    {
        m_idle = false;

        while ((task = m_parent->GetQueuedTask()))
        {
            // pull task from housekeeper and process it
            ReferenceLocker rlock(task);

            if (!task->ConfirmRun())
            {
                // something else has caused the lastrun time to
                // change since this was requested to run. abort.
                task = NULL;
                continue;
            }

            task->UpdateLastRun(false);
            if (task->Run())
                task->UpdateLastRun(task->GetLastRun(), true);
            task = NULL;

            if (!m_keepRunning)
                // thread has been discarded, don't try to start another task
                break;
        }

        m_idle = true;

        if (!m_keepRunning)
            // short out rather than potentially hitting another sleep cycle
            break;

        m_waitCondition.wait(&m_waitMutex);
    }

    m_waitMutex.unlock();
    RunEpilog();
}

/** \class HouseKeeper
 *  \ingroup housekeeper
 *  \brief Manages registered HouseKeeperTasks and queues tasks for operation
 *
 *  This class operates threadless, in the main application event loop. Its
 *  only role is to check whether tasks are ready to be run, so computation
 *  in those methods should be kept to a minimum to not stall the event loop.
 *  Tasks ready to run are placed into a queue for the run thread to run.
 *
 *  To use, one instance of this should be created at application init. All
 *  housekeeping tasks should be registered using <b>RegisterTask()</b>, and
 *  then the housekeeper started using <b>Start</b>. Tasks <i>can</i> be added
 *  at a later time, however special startup behavior will not be honored.
 *  Tasks cannot be removed from the housekeeper once added.
 *
 */
HouseKeeper::HouseKeeper(void) : m_timer(NULL)
{
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(Run()));
    m_timer->setInterval(60000);
    m_timer->setSingleShot(false);
}

HouseKeeper::~HouseKeeper(void)
{
    gCoreContext->removeListener(this);

    if (m_timer)
    {
        m_timer->stop();
        disconnect(m_timer);
        delete m_timer;
        m_timer = NULL;
    }

    {
        // remove anything from the queue first, so it does not start
        QMutexLocker queueLock(&m_queueLock);
        while (!m_taskQueue.isEmpty())
            m_taskQueue.takeFirst()->DecrRef();
    }

    {
        // issue a terminate call to any long-running tasks
        // this is just a noop unless overwritten by a subclass
        QMutexLocker mapLock(&m_mapLock);
        QMap<QString,HouseKeeperTask*>::iterator it = m_taskMap.begin();
        for (; it != m_taskMap.end(); ++it)
            (*it)->Terminate();
    }

    if (!m_threadList.isEmpty())
    {
        QMutexLocker threadLock(&m_threadLock);
        // tell primary thread to self-terminate and wake it
        m_threadList.first()->Discard();
        m_threadList.first()->Wake();
        // wait for any remaining threads to self-terminate and close
        while (!m_threadList.isEmpty())
        {
            HouseKeepingThread *thread = m_threadList.takeFirst();
            thread->wait();
            delete thread;
        }
    }

    {
        // unload any registered tasks
        QMutexLocker mapLock(&m_mapLock);
        QMap<QString,HouseKeeperTask*>::iterator it = m_taskMap.begin();
        while (it != m_taskMap.end())
        {
            (*it)->DecrRef();
            it = m_taskMap.erase(it);
        }
    }
}

void HouseKeeper::RegisterTask(HouseKeeperTask *task)
{
    QMutexLocker mapLock(&m_mapLock);
    QString tag = task->GetTag();
    if (m_taskMap.contains(tag))
    {
        task->DecrRef();
        LOG(VB_GENERAL, LOG_ERR, 
                QString("HouseKeeperTask '%1' already registered. "
                        "Rejecting duplicate.").arg(tag));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO,
                QString("Registering HouseKeeperTask '%1'.").arg(tag));
        m_taskMap.insert(tag, task);
    }
}

HouseKeeperTask* HouseKeeper::GetQueuedTask(void)
{
    QMutexLocker queueLock(&m_queueLock);
    HouseKeeperTask *task = NULL;

    if (!m_taskQueue.isEmpty())
    {
        task = m_taskQueue.dequeue();
    }

    // returning NULL tells the thread that the queue is empty and
    // to go into standby
    return task;
}

void HouseKeeper::Start(void)
{
    // no need to be fine grained, nothing else should be accessing this map
    QMutexLocker mapLock(&m_mapLock);

    if (m_timer->isActive())
        // Start() should only be called once
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT tag,lastrun"
                  "  FROM housekeeping"
                  " WHERE hostname = :HOST"
                  "    OR hostname IS NULL");
    query.bindValue(":HOST", gCoreContext->GetHostName());

    if (!query.exec())
        MythDB::DBError("HouseKeeper::Run", query);
    else
    {
        while (query.next())
        {
            // loop through housekeeping table and load last run timestamps
            QString tag = query.value(0).toString();
            QDateTime lastrun = MythDate::as_utc(query.value(1).toDateTime());

            if (m_taskMap.contains(tag))
                m_taskMap[tag]->SetLastRun(lastrun);
        }
    }

    gCoreContext->addListener(this);

    QMap<QString,HouseKeeperTask*>::const_iterator it;
    for (it = m_taskMap.begin(); it != m_taskMap.end(); ++it)
    {
        if ((*it)->CheckImmediate())
        {
            // run any tasks marked for immediate operation in-thread
            (*it)->UpdateLastRun();
            (*it)->Run();
        }
        else if ((*it)->CheckStartup())
        {
            // queue any tasks marked for startup
            LOG(VB_GENERAL, LOG_INFO,
                QString("Queueing HouseKeeperTask '%1'.").arg(it.key()));
            QMutexLocker queueLock(&m_queueLock);
            (*it)->IncrRef();
            m_taskQueue.enqueue(*it);
        }
    }

    LOG(VB_GENERAL, LOG_INFO, "Starting HouseKeeper.");

    if (!m_taskQueue.isEmpty())
        StartThread();

    m_timer->start();
}

void HouseKeeper::Run(void)
{
    LOG(VB_GENERAL, LOG_DEBUG, "Running HouseKeeper.");

    QDateTime now = MythDate::current();

    QMutexLocker mapLock(&m_mapLock);
    QMap<QString,HouseKeeperTask*>::const_iterator it;
    for (it = m_taskMap.begin(); it != m_taskMap.end(); ++it)
    {
        if ((*it)->CheckRun(now))
        {
            // check if any tasks are ready to run, and add to queue
            LOG(VB_GENERAL, LOG_INFO,
                QString("Queueing HouseKeeperTask '%1'.").arg(it.key()));
            QMutexLocker queueLock(&m_queueLock);
            (*it)->IncrRef();
            m_taskQueue.enqueue(*it);
        }
    }

    if (!m_taskQueue.isEmpty())
        StartThread();

    if (m_threadList.size() > 1)
    {
        // spent threads exist in the thread list
        // check to see if any have finished up their task and terminated
        QMutexLocker threadLock(&m_threadLock);
        int count1 = m_threadList.size();

        QList<HouseKeepingThread*>::iterator it = m_threadList.begin();
        ++it; // skip the primary thread
        while (it != m_threadList.end())
        {
            if ((*it)->isRunning())
                ++it;
            else
            {
                delete *it;
                it = m_threadList.erase(it);
            }
        }

        int count2 = m_threadList.size();
        if (count1 > count2)
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("Discarded HouseKeepingThreads have completed and "
                        "been deleted. Current count %1 -> %2.")
                            .arg(count1).arg(count2));
    }
}

/** \brief Wake the primary run thread, or create a new one
 *
 *  The HouseKeeper maintains one primary thread, but allows for multiple to
 *  allow task processing to continue should one long-running thread stall.
 *
 *  The HouseKeeper scans to queue tasks once a minute. If any tasks are in
 *  the queue at the end of the scan, this method will be run. If the primary
 *  thread is still in use from the previous scan, it will be marked for
 *  destruction and a replacement thread will be started to pick up the
 *  remainder of the queue. The in-progress thread will terminate itself once
 *  its current task has finished, and will be eventually cleaned up by this
 *  method.
 */
void HouseKeeper::StartThread(void)
{
    QMutexLocker threadLock(&m_threadLock);

    if (m_threadList.isEmpty())
    {
        // we're running for the first time
        // start up a new thread
        LOG(VB_GENERAL, LOG_DEBUG, "Running initial HouseKeepingThread.");
        HouseKeepingThread *thread = new HouseKeepingThread(this);
        m_threadList.append(thread);
        thread->start();
    }

    else if (!m_threadList.first()->isIdle())
    {
        // the old thread is still off processing something
        // discard it and start a new one because we have more stuff
        // that wants to run
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Current HouseKeepingThread is delayed on task, "
                    "spawning replacement. Current count %1.")
                            .arg(m_threadList.size()));
        m_threadList.first()->Discard();
        HouseKeepingThread *thread = new HouseKeepingThread(this);
        m_threadList.prepend(thread);
        thread->start();
    }

    else
    {
        // the old thread is idle, so just wake it for processing
        LOG(VB_GENERAL, LOG_DEBUG, "Waking HouseKeepingThread.");
        m_threadList.first()->Wake();
    }
}

void HouseKeeper::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent*)e;
        if ((me->Message().left(20) == "HOUSE_KEEPER_RUNNING") ||
            (me->Message().left(23) == "HOUSE_KEEPER_SUCCESSFUL"))
        {
            QStringList tokens = me->Message()
                                    .split(" ", QString::SkipEmptyParts);
            if (tokens.size() != 4)
                return;

            QString hostname = tokens[1];
            QString tag = tokens[2];
            QDateTime last = MythDate::fromString(tokens[3]);
            bool successful = me->Message().contains("SUCCESSFUL");

            QMutexLocker mapLock(&m_mapLock);
            if (m_taskMap.contains(tag))
            {
                if ((m_taskMap[tag]->GetScope() == kHKGlobal) ||
                        ((m_taskMap[tag]->GetScope() == kHKLocal) &&
                         (gCoreContext->GetHostName() == hostname)))
                    // task being run in the same scope as us.
                    // update the run time so we don't attempt to run
                    //      it ourselves
                    m_taskMap[tag]->SetLastRun(last, successful);
            }
        }
    }
}

