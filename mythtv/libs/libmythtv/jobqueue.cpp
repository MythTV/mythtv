
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <cstdlib>
#include <fcntl.h>
#include <pthread.h>
using namespace std;

#include <QDateTime>
#include <QFileInfo>
#include <QRegExp>
#include <QEvent>
#include <QCoreApplication>

#include "mythconfig.h"

#include "exitcodes.h"
#include "jobqueue.h"
#include "programinfo.h"
#include "mythcorecontext.h"
#include "mythdate.h"
#include "previewgenerator.h"
#include "compat.h"
#include "recordingprofile.h"
#include "recordinginfo.h"
#include "mthread.h"

#include "mythdb.h"
#include "mythdirs.h"
#include "mythsystem.h"
#include "mythlogging.h"
#include "mythmiscutil.h"

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#define LOC     QString("JobQueue: ")

JobQueue::JobQueue(bool master) :
    m_hostname(gCoreContext->GetHostName()),
    jobsRunning(0),
    jobQueueCPU(0),
    m_pginfo(NULL),
    runningJobsLock(new QMutex(QMutex::Recursive)),
    isMaster(master),
    queueThread(new MThread("JobQueue", this)),
    processQueue(false)
{
    jobQueueCPU = gCoreContext->GetNumSetting("JobQueueCPU", 0);

#ifndef USING_VALGRIND
    QMutexLocker locker(&queueThreadCondLock);
    processQueue = true;
    queueThread->start();
#else
    LOG(VB_GENERAL, LOG_ERR, LOC +
        "The JobQueue has been disabled because "
        "you compiled with the --enable-valgrind option.");
#endif // USING_VALGRIND

    gCoreContext->addListener(this);
}

JobQueue::~JobQueue(void)
{
    queueThreadCondLock.lock();
    processQueue = false;
    queueThreadCond.wakeAll();
    queueThreadCondLock.unlock();

    queueThread->wait();
    delete queueThread;
    queueThread = NULL;

    gCoreContext->removeListener(this);

    delete runningJobsLock;
}

void JobQueue::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message.left(9) == "LOCAL_JOB")
        {
            // LOCAL_JOB action ID jobID
            // LOCAL_JOB action type chanid recstartts hostname
            QString msg;
            message = message.simplified();
            QStringList tokens = message.split(" ", QString::SkipEmptyParts);
            QString action = tokens[1];
            int jobID = -1;

            if (tokens[2] == "ID")
                jobID = tokens[3].toInt();
            else
            {
                jobID = GetJobID(
                    tokens[2].toInt(),
                    tokens[3].toUInt(),
                    MythDate::fromString(tokens[4]));
            }

            runningJobsLock->lock();
            if (!runningJobs.contains(jobID))
            {
                msg = QString("Unable to determine jobID for message: "
                              "%1.  Program will not be flagged.")
                              .arg(message);
                LOG(VB_GENERAL, LOG_ERR, LOC + msg);
                runningJobsLock->unlock();
                return;
            }
            runningJobsLock->unlock();

            msg = QString("Received message '%1'").arg(message);
            LOG(VB_JOBQUEUE, LOG_INFO, LOC + msg);

            if ((action == "STOP") ||
                (action == "PAUSE") ||
                (action == "RESTART") ||
                (action == "RESUME" ))
            {
                runningJobsLock->lock();

                if (action == "STOP")
                    runningJobs[jobID].flag = JOB_STOP;
                else if (action == "PAUSE")
                    runningJobs[jobID].flag = JOB_PAUSE;
                else if (action == "RESUME")
                    runningJobs[jobID].flag = JOB_RUN;
                else if (action == "RESTART")
                    runningJobs[jobID].flag = JOB_RESTART;

                runningJobsLock->unlock();
            }
        }
    }
}

void JobQueue::run(void)
{
    queueThreadCondLock.lock();
    queueThreadCond.wakeAll();
    queueThreadCondLock.unlock();

    RecoverQueue();

    queueThreadCondLock.lock();
    queueThreadCond.wait(&queueThreadCondLock, 10 * 1000);
    queueThreadCondLock.unlock();

    ProcessQueue();
}

void JobQueue::ProcessQueue(void)
{
    LOG(VB_JOBQUEUE, LOG_INFO, LOC + "ProcessQueue() started");

    QString logInfo;
    int jobID;
    int cmds;
    //int flags;
    int status;
    QString hostname;
    int sleepTime;

    QMap<int, int> jobStatus;
    int maxJobs;
    QString message;
    QMap<int, JobQueueEntry> jobs;
    bool atMax = false;
    bool inTimeWindow = true;
    bool startedJobAlready = false;
    QMap<int, RunningJobInfo>::Iterator rjiter;

    QMutexLocker locker(&queueThreadCondLock);
    while (processQueue)
    {
        locker.unlock();

        startedJobAlready = false;
        sleepTime = gCoreContext->GetNumSetting("JobQueueCheckFrequency", 30);
        maxJobs = gCoreContext->GetNumSetting("JobQueueMaxSimultaneousJobs", 3);
        LOG(VB_JOBQUEUE, LOG_INFO, LOC +
            QString("Currently set to run up to %1 job(s) max.")
                        .arg(maxJobs));

        jobStatus.clear();

        runningJobsLock->lock();
        for (rjiter = runningJobs.begin(); rjiter != runningJobs.end();
            ++rjiter)
        {
            if ((*rjiter).pginfo)
                (*rjiter).pginfo->UpdateInUseMark();
        }
        runningJobsLock->unlock();

        jobsRunning = 0;
        GetJobsInQueue(jobs);

        if (jobs.size())
        {
            inTimeWindow = InJobRunWindow();
            for (int x = 0; x < jobs.size(); x++)
            {
                status = jobs[x].status;
                hostname = jobs[x].hostname;

                if (((status == JOB_RUNNING) ||
                     (status == JOB_STARTING) ||
                     (status == JOB_PAUSED)) &&
                    (hostname == m_hostname))
                jobsRunning++;
            }

            message = QString("Currently Running %1 jobs.")
                              .arg(jobsRunning);
            if (!inTimeWindow)
            {
                message += QString("  Jobs in Queue, but we are outside of the "
                                   "Job Queue time window, no new jobs can be "
                                   "started.");
                LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
            }
            else if (jobsRunning >= maxJobs)
            {
                message += " (At Maximum, no new jobs can be started until "
                           "a running job completes)";

                if (!atMax)
                    LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);

                atMax = true;
            }
            else
            {
                LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
                atMax = false;
            }


            for ( int x = 0;
                 (x < jobs.size()) && (jobsRunning < maxJobs); x++)
            {
                jobID = jobs[x].id;
                cmds = jobs[x].cmds;
                //flags = jobs[x].flags;
                status = jobs[x].status;
                hostname = jobs[x].hostname;

                if (!jobs[x].chanid)
                    logInfo = QString("jobID #%1").arg(jobID);
                else
                    logInfo = QString("chanid %1 @ %2").arg(jobs[x].chanid)
                                      .arg(jobs[x].startts);

                // Should we even be looking at this job?
                if ((inTimeWindow) &&
                    (!hostname.isEmpty()) &&
                    (hostname != m_hostname))
                {
                    // Setting the status here will prevent us from processing
                    // any other jobs for this recording until this one is
                    // completed on the remote host.
                    jobStatus[jobID] = status;

                    message = QString("Skipping '%1' job for %2, "
                                      "should run on '%3' instead")
                                      .arg(JobText(jobs[x].type)).arg(logInfo)
                                      .arg(hostname);
                    LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
                    continue;
                }

                // Check to see if there was a previous job that is not done
                if (inTimeWindow)
                {
                    int otherJobID = GetRunningJobID(jobs[x].chanid,
                                                     jobs[x].recstartts);
                    if (otherJobID && (jobStatus.contains(otherJobID)) &&
                        (!(jobStatus[otherJobID] & JOB_DONE)))
                    {
                        message =
                            QString("Skipping '%1' job for %2, "
                                    "Job ID %3 is already running for "
                                    "this recording with a status of '%4'")
                                    .arg(JobText(jobs[x].type)).arg(logInfo)
                                    .arg(otherJobID)
                                    .arg(StatusText(jobStatus[otherJobID]));
                        LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
                        continue;
                    }
                }

                jobStatus[jobID] = status;

                // Are we allowed to run this job?
                if ((inTimeWindow) && (!AllowedToRun(jobs[x])))
                {
                    message = QString("Skipping '%1' job for %2, "
                                      "not allowed to run on this backend.")
                                      .arg(JobText(jobs[x].type)).arg(logInfo);
                    LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
                    continue;
                }

                // Is this job scheduled for the future
                if (jobs[x].schedruntime > MythDate::current())
                {
                    message = QString("Skipping '%1' job for %2, this job is "
                                      "not scheduled to run until %3.")
                                      .arg(JobText(jobs[x].type)).arg(logInfo)
                                      .arg(jobs[x].schedruntime
                                           .toString(Qt::ISODate));
                    LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
                    continue;
                }

                if (cmds & JOB_STOP)
                {
                    // if we're trying to stop a job and it's not queued
                    //  then lets send a STOP command
                    if (status != JOB_QUEUED) {
                        message = QString("Stopping '%1' job for %2")
                                          .arg(JobText(jobs[x].type))
                                          .arg(logInfo);
                        LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);

                        runningJobsLock->lock();
                        if (runningJobs.contains(jobID))
                            runningJobs[jobID].flag = JOB_STOP;
                        runningJobsLock->unlock();

                        // ChangeJobCmds(m_db, jobID, JOB_RUN);
                        continue;

                    // if we're trying to stop a job and it's still queued
                    //  then let's just change the status to cancelled so
                    //  we don't try to run it from the queue
                    } else {
                        message = QString("Cancelling '%1' job for %2")
                                          .arg(JobText(jobs[x].type))
                                          .arg(logInfo);
                        LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);

                        // at the bottom of this loop we requeue any jobs that
                        //  are not currently queued and also not associated
                        //  with a hostname so we must claim this job before we
                        //  can cancel it
                        if (!ChangeJobHost(jobID, m_hostname))
                        {
                            message = QString("Unable to claim '%1' job for %2")
                                              .arg(JobText(jobs[x].type))
                                              .arg(logInfo);
                            LOG(VB_JOBQUEUE, LOG_ERR, LOC + message);
                            continue;
                        }

                        ChangeJobStatus(jobID, JOB_CANCELLED, "");
                        ChangeJobCmds(jobID, JOB_RUN);
                        continue;
                    }
                }

                if ((cmds & JOB_PAUSE) && (status != JOB_QUEUED))
                {
                    message = QString("Pausing '%1' job for %2")
                                      .arg(JobText(jobs[x].type)).arg(logInfo);
                    LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);

                    runningJobsLock->lock();
                    if (runningJobs.contains(jobID))
                        runningJobs[jobID].flag = JOB_PAUSE;
                    runningJobsLock->unlock();

                    ChangeJobCmds(jobID, JOB_RUN);
                    continue;
                }

                if ((cmds & JOB_RESTART) && (status != JOB_QUEUED))
                {
                    message = QString("Restart '%1' job for %2")
                                      .arg(JobText(jobs[x].type)).arg(logInfo);
                    LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);

                    runningJobsLock->lock();
                    if (runningJobs.contains(jobID))
                        runningJobs[jobID].flag = JOB_RUN;
                    runningJobsLock->unlock();

                    ChangeJobCmds(jobID, JOB_RUN);
                    continue;
                }

                if (status != JOB_QUEUED)
                {

                    if (hostname.isEmpty())
                    {
                        message = QString("Resetting '%1' job for %2 to %3 "
                                          "status, because no hostname is set.")
                                          .arg(JobText(jobs[x].type))
                                          .arg(logInfo)
                                          .arg(StatusText(JOB_QUEUED));
                        LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);

                        ChangeJobStatus(jobID, JOB_QUEUED, "");
                        ChangeJobCmds(jobID, JOB_RUN);
                    }
                    else if (inTimeWindow)
                    {
                        message = QString("Skipping '%1' job for %2, "
                                          "current job status is '%3'")
                                          .arg(JobText(jobs[x].type))
                                          .arg(logInfo)
                                          .arg(StatusText(status));
                        LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
                    }
                    continue;
                }

                // never start or claim more than one job in a single run
                if (startedJobAlready)
                    continue;

                if ((inTimeWindow) &&
                    (hostname.isEmpty()) &&
                    (!ChangeJobHost(jobID, m_hostname)))
                {
                    message = QString("Unable to claim '%1' job for %2")
                                      .arg(JobText(jobs[x].type)).arg(logInfo);
                    LOG(VB_JOBQUEUE, LOG_ERR, LOC + message);
                    continue;
                }

                if (!inTimeWindow)
                {
                    message = QString("Skipping '%1' job for %2, "
                                      "current time is outside of the "
                                      "Job Queue processing window.")
                                      .arg(JobText(jobs[x].type)).arg(logInfo);
                    LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
                    continue;
                }

                message = QString("Processing '%1' job for %2, "
                                  "current status is '%3'")
                                  .arg(JobText(jobs[x].type)).arg(logInfo)
                                  .arg(StatusText(status));
                LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);

                ProcessJob(jobs[x]);

                startedJobAlready = true;
            }
        }

        if (QCoreApplication::applicationName() == MYTH_APPNAME_MYTHJOBQUEUE)
        {
            if (jobsRunning > 0)
            {
                if (!(gCoreContext->IsBlockingClient()))
                {
                    gCoreContext->BlockShutdown();
                    LOG(VB_JOBQUEUE, LOG_INFO, QString("%1 jobs running. "
                        "Blocking shutdown.").arg(jobsRunning));
                }
            }
            else
            {
                if (gCoreContext->IsBlockingClient())
                {
                    gCoreContext->AllowShutdown();
                    LOG(VB_JOBQUEUE, LOG_INFO, "No jobs running. "
                                               "Allowing shutdown.");
                }
            }
        }


        locker.relock();
        if (processQueue)
        {
            int st = (startedJobAlready) ? (5 * 1000) : (sleepTime * 1000);
            if (st > 0)
                queueThreadCond.wait(locker.mutex(), st);
        }
    }
}

bool JobQueue::QueueRecordingJobs(const RecordingInfo &recinfo, int jobTypes)
{
    if (jobTypes == JOB_NONE)
        jobTypes = recinfo.GetAutoRunJobs();

    if (recinfo.IsCommercialFree())
        jobTypes &= (~JOB_COMMFLAG);

    if (jobTypes != JOB_NONE)
    {
        QString jobHost = QString("");

        if (gCoreContext->GetNumSetting("JobsRunOnRecordHost", 0))
            jobHost = recinfo.GetHostname();

        return JobQueue::QueueJobs(
            jobTypes, recinfo.GetChanID(), recinfo.GetRecordingStartTime(),
            "", "", jobHost);
    }
    else
        return false;

    return true;
}

bool JobQueue::QueueJob(int jobType, uint chanid, const QDateTime &recstartts,
                        QString args, QString comment, QString host,
                        int flags, int status, QDateTime schedruntime)
{
    int tmpStatus = JOB_UNKNOWN;
    int tmpCmd = JOB_UNKNOWN;
    int jobID = -1;
    int chanidInt = -1;

    if(!schedruntime.isValid())
        schedruntime = MythDate::current();

    MSqlQuery query(MSqlQuery::InitCon());

    // In order to replace a job, we must have a chanid/recstartts combo
    if (chanid)
    {
        query.prepare("SELECT status, id, cmds FROM jobqueue "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                      "AND type = :JOBTYPE;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
        query.bindValue(":JOBTYPE", jobType);

        if (!query.exec())
        {
            MythDB::DBError("Error in JobQueue::QueueJob()", query);
            return false;
        }
        else
        {
            if (query.next())
            {
                tmpStatus = query.value(0).toInt();
                jobID = query.value(1).toInt();
                tmpCmd = query.value(2).toInt();
            }
        }
        switch (tmpStatus)
        {
            case JOB_UNKNOWN:
                     break;
            case JOB_STARTING:
            case JOB_RUNNING:
            case JOB_PAUSED:
            case JOB_STOPPING:
            case JOB_ERRORING:
            case JOB_ABORTING:
                     return false;
            default:
                     DeleteJob(jobID);
                     break;
        }
        if (! (tmpStatus & JOB_DONE) && (tmpCmd & JOB_STOP))
            return false;

        chanidInt = chanid;
    }

    if (host.isNull())
        host = QString("");

    query.prepare("INSERT INTO jobqueue (chanid, starttime, inserttime, type, "
                  "status, statustime, schedruntime, hostname, args, comment, "
                  "flags) "
                  "VALUES (:CHANID, :STARTTIME, now(), :JOBTYPE, :STATUS, "
                  "now(), :SCHEDRUNTIME, :HOST, :ARGS, :COMMENT, :FLAGS);");

    query.bindValue(":CHANID", chanidInt);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":JOBTYPE", jobType);
    query.bindValue(":STATUS", status);
    query.bindValue(":SCHEDRUNTIME", schedruntime);
    query.bindValue(":HOST", host);
    query.bindValue(":ARGS", args);
    query.bindValue(":COMMENT", comment);
    query.bindValue(":FLAGS", flags);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::StartJob()", query);
        return false;
    }

    return true;
}

bool JobQueue::QueueJobs(int jobTypes, uint chanid, const QDateTime &recstartts,
                         QString args, QString comment, QString host)
{
    if (gCoreContext->GetNumSetting("AutoTranscodeBeforeAutoCommflag", 0))
    {
        if (jobTypes & JOB_METADATA)
            QueueJob(JOB_METADATA, chanid, recstartts, args, comment, host);
        if (jobTypes & JOB_TRANSCODE)
            QueueJob(JOB_TRANSCODE, chanid, recstartts, args, comment, host);
        if (jobTypes & JOB_COMMFLAG)
            QueueJob(JOB_COMMFLAG, chanid, recstartts, args, comment, host);
    }
    else
    {
        if (jobTypes & JOB_METADATA)
            QueueJob(JOB_METADATA, chanid, recstartts, args, comment, host);
        if (jobTypes & JOB_COMMFLAG)
            QueueJob(JOB_COMMFLAG, chanid, recstartts, args, comment, host);
        if (jobTypes & JOB_TRANSCODE)
        {
            QDateTime schedruntime = MythDate::current();

            int defer = gCoreContext->GetNumSetting("DeferAutoTranscodeDays", 0);
            if (defer)
            {
                schedruntime = QDateTime(schedruntime.addDays(defer).date(),
                                         QTime(0,0,0), Qt::UTC);
            }

            QueueJob(JOB_TRANSCODE, chanid, recstartts, args, comment, host,
              0, JOB_QUEUED, schedruntime);
        }
    }

    if (jobTypes & JOB_USERJOB1)
        QueueJob(JOB_USERJOB1, chanid, recstartts, args, comment, host);
    if (jobTypes & JOB_USERJOB2)
        QueueJob(JOB_USERJOB2, chanid, recstartts, args, comment, host);
    if (jobTypes & JOB_USERJOB3)
        QueueJob(JOB_USERJOB3, chanid, recstartts, args, comment, host);
    if (jobTypes & JOB_USERJOB4)
        QueueJob(JOB_USERJOB4, chanid, recstartts, args, comment, host);

    return true;
}

int JobQueue::GetJobID(int jobType, uint chanid, const QDateTime &recstartts)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT id FROM jobqueue "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND type = :JOBTYPE;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":JOBTYPE", jobType);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::GetJobID()", query);
        return -1;
    }
    else
    {
        if (query.next())
            return query.value(0).toInt();
    }

    return -1;
}

bool JobQueue::GetJobInfoFromID(
    int jobID, int &jobType, uint &chanid, QDateTime &recstartts)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT type, chanid, starttime FROM jobqueue "
                  "WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::GetJobInfoFromID()", query);
        return false;
    }
    else
    {
        if (query.next())
        {
            jobType    = query.value(0).toInt();
            chanid     = query.value(1).toUInt();
            recstartts = MythDate::as_utc(query.value(2).toDateTime());
            return true;
        }
    }

    return false;
}

bool JobQueue::GetJobInfoFromID(
    int jobID, int &jobType, uint &chanid, QString &recstartts)
{
    QDateTime tmpStarttime;

    bool result = JobQueue::GetJobInfoFromID(
        jobID, jobType, chanid, tmpStarttime);

    if (result)
        recstartts = MythDate::toString(tmpStarttime, MythDate::kFilename);

    return result;
}

bool JobQueue::PauseJob(int jobID)
{
    QString message = QString("GLOBAL_JOB PAUSE ID %1").arg(jobID);
    MythEvent me(message);
    gCoreContext->dispatch(me);

    return ChangeJobCmds(jobID, JOB_PAUSE);
}

bool JobQueue::ResumeJob(int jobID)
{
    QString message = QString("GLOBAL_JOB RESUME ID %1").arg(jobID);
    MythEvent me(message);
    gCoreContext->dispatch(me);

    return ChangeJobCmds(jobID, JOB_RESUME);
}

bool JobQueue::RestartJob(int jobID)
{
    QString message = QString("GLOBAL_JOB RESTART ID %1").arg(jobID);
    MythEvent me(message);
    gCoreContext->dispatch(me);

    return ChangeJobCmds(jobID, JOB_RESTART);
}

bool JobQueue::StopJob(int jobID)
{
    QString message = QString("GLOBAL_JOB STOP ID %1").arg(jobID);
    MythEvent me(message);
    gCoreContext->dispatch(me);

    return ChangeJobCmds(jobID, JOB_STOP);
}

bool JobQueue::DeleteAllJobs(uint chanid, const QDateTime &recstartts)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString message;

    query.prepare("UPDATE jobqueue SET status = :CANCELLED "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND status = :QUEUED;");

    query.bindValue(":CANCELLED", JOB_CANCELLED);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":QUEUED", JOB_QUEUED);

    if (!query.exec())
        MythDB::DBError("Cancel Pending Jobs", query);

    query.prepare("UPDATE jobqueue SET cmds = :CMD "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND status <> :CANCELLED;");
    query.bindValue(":CMD", JOB_STOP);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":CANCELLED", JOB_CANCELLED);

    if (!query.exec())
    {
        MythDB::DBError("Stop Unfinished Jobs", query);
        return false;
    }

    // wait until running job(s) are done
    bool jobsAreRunning = true;
    int totalSlept = 0;
    int maxSleep = 90;
    while (jobsAreRunning && totalSlept < maxSleep)
    {
        usleep(1000);
        query.prepare("SELECT id FROM jobqueue "
                      "WHERE chanid = :CHANID and starttime = :STARTTIME "
                      "AND status NOT IN "
                      "(:FINISHED,:ABORTED,:ERRORED,:CANCELLED);");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
        query.bindValue(":FINISHED", JOB_FINISHED);
        query.bindValue(":ABORTED", JOB_ABORTED);
        query.bindValue(":ERRORED", JOB_ERRORED);
        query.bindValue(":CANCELLED", JOB_CANCELLED);

        if (!query.exec())
        {
            MythDB::DBError("Stop Unfinished Jobs", query);
            return false;
        }

        if (query.size() == 0)
        {
            jobsAreRunning = false;
            break;
        }
        else if ((totalSlept % 5) == 0)
        {
            message = QString("Waiting on %1 jobs still running for "
                              "chanid %2 @ %3").arg(query.size())
                .arg(chanid).arg(recstartts.toString(Qt::ISODate));
            LOG(VB_JOBQUEUE, LOG_INFO, LOC + message);
        }

        sleep(1);
        totalSlept++;
    }

    if (totalSlept <= maxSleep)
    {
        query.prepare("DELETE FROM jobqueue "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);

        if (!query.exec())
            MythDB::DBError("Delete All Jobs", query);
    }
    else
    {
        query.prepare("SELECT id, type, status, comment FROM jobqueue "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                      "AND status <> :CANCELLED ORDER BY id;");

        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
        query.bindValue(":CANCELLED", JOB_CANCELLED);

        if (!query.exec())
        {
            MythDB::DBError("Error in JobQueue::DeleteAllJobs(), Unable "
                            "to query list of Jobs left in Queue.", query);
            return 0;
        }

        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString( "In DeleteAllJobs: There are Jobs "
                     "left in the JobQueue that are still running for "
                     "chanid %1 @ %2.").arg(chanid)
            .arg(recstartts.toString(Qt::ISODate)));

        while (query.next())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Job ID %1: '%2' with status '%3' and comment '%4'")
                            .arg(query.value(0).toInt())
                            .arg(JobText(query.value(1).toInt()))
                            .arg(StatusText(query.value(2).toInt()))
                            .arg(query.value(3).toString()));
        }

        return false;
    }

    return true;
}

bool JobQueue::DeleteJob(int jobID)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::DeleteJob()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobCmds(int jobID, int newCmds)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET cmds = :CMDS WHERE id = :ID;");

    query.bindValue(":CMDS", newCmds);
    query.bindValue(":ID", jobID);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::ChangeJobCmds()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobCmds(int jobType, uint chanid,
                             const QDateTime &recstartts,  int newCmds)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET cmds = :CMDS WHERE type = :TYPE "
                  "AND chanid = :CHANID AND starttime = :STARTTIME;");

    query.bindValue(":CMDS", newCmds);
    query.bindValue(":TYPE", jobType);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::ChangeJobCmds()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobFlags(int jobID, int newFlags)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET flags = :FLAGS WHERE id = :ID;");

    query.bindValue(":FLAGS", newFlags);
    query.bindValue(":ID", jobID);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::ChangeJobFlags()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobStatus(int jobID, int newStatus, QString comment)
{
    if (jobID < 0)
        return false;

    LOG(VB_JOBQUEUE, LOG_INFO, LOC + QString("ChangeJobStatus(%1, %2, '%3')")
            .arg(jobID).arg(StatusText(newStatus)).arg(comment));

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET status = :STATUS, comment = :COMMENT "
                  "WHERE id = :ID AND status <> :NEWSTATUS;");

    query.bindValue(":STATUS", newStatus);
    query.bindValue(":COMMENT", comment);
    query.bindValue(":ID", jobID);
    query.bindValue(":NEWSTATUS", newStatus);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::ChangeJobStatus()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobComment(int jobID, QString comment)
{
    if (jobID < 0)
        return false;

    LOG(VB_JOBQUEUE, LOG_INFO, LOC + QString("ChangeJobComment(%1, '%2')")
            .arg(jobID).arg(comment));

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET comment = :COMMENT "
                  "WHERE id = :ID;");

    query.bindValue(":COMMENT", comment);
    query.bindValue(":ID", jobID);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::ChangeJobComment()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobArgs(int jobID, QString args)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET args = :ARGS "
                  "WHERE id = :ID;");

    query.bindValue(":ARGS", args);
    query.bindValue(":ID", jobID);

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::ChangeJobArgs()", query);
        return false;
    }

    return true;
}

int JobQueue::GetRunningJobID(uint chanid, const QDateTime &recstartts)
{
    runningJobsLock->lock();
    QMap<int, RunningJobInfo>::iterator it = runningJobs.begin();
    for (; it != runningJobs.end(); ++it)
    {
        RunningJobInfo jInfo = *it;

        if ((jInfo.pginfo->GetChanID()             == chanid) &&
            (jInfo.pginfo->GetRecordingStartTime() == recstartts))
        {
            runningJobsLock->unlock();

            return jInfo.id;
        }
    }
    runningJobsLock->unlock();

    return 0;
}

bool JobQueue::IsJobRunning(int jobType,
                            uint chanid, const QDateTime &recstartts)
{
    int tmpStatus = GetJobStatus(jobType, chanid, recstartts);

    if ((tmpStatus != JOB_UNKNOWN) && (tmpStatus != JOB_QUEUED) &&
        (!(tmpStatus & JOB_DONE)))
        return true;

    return false;
}

bool JobQueue::IsJobRunning(int jobType, const ProgramInfo &pginfo)
{
    return JobQueue::IsJobRunning(
        jobType, pginfo.GetChanID(), pginfo.GetRecordingStartTime());
}

bool JobQueue::IsJobQueuedOrRunning(
    int jobType, uint chanid, const QDateTime &recstartts)
{
    int tmpStatus = GetJobStatus(jobType, chanid, recstartts);

    if ((tmpStatus != JOB_UNKNOWN) && (!(tmpStatus & JOB_DONE)))
        return true;

    return false;
}

bool JobQueue::IsJobQueued(
    int jobType, uint chanid, const QDateTime &recstartts)
{
    int tmpStatus = GetJobStatus(jobType, chanid, recstartts);

    if (tmpStatus & JOB_QUEUED)
        return true;

    return false;
}

QString JobQueue::JobText(int jobType)
{
    switch (jobType)
    {
        case JOB_TRANSCODE:  return tr("Transcode");
        case JOB_COMMFLAG:   return tr("Flag Commercials");
        case JOB_METADATA:   return tr("Look up Metadata");
    }

    if (jobType & JOB_USERJOB)
    {
        QString settingName =
            QString("UserJobDesc%1").arg(UserJobTypeToIndex(jobType));
        return gCoreContext->GetSetting(settingName, settingName);
    }

    return tr("Unknown Job");
}

QString JobQueue::StatusText(int status)
{
    switch (status)
    {
#define JOBSTATUS_STATUSTEXT(A,B,C) case A: return C;
        JOBSTATUS_MAP(JOBSTATUS_STATUSTEXT)
        default: break;
    }
    return tr("Undefined");
}

bool JobQueue::InJobRunWindow(int orStartsWithinMins)
{
    QString queueStartTimeStr;
    QString queueEndTimeStr;
    QTime queueStartTime;
    QTime queueEndTime;
    QTime curTime = QTime::currentTime();
    bool inTimeWindow = false;
    orStartsWithinMins = orStartsWithinMins < 0 ? 0 : orStartsWithinMins;

    queueStartTimeStr = gCoreContext->GetSetting("JobQueueWindowStart", "00:00");
    queueEndTimeStr = gCoreContext->GetSetting("JobQueueWindowEnd", "23:59");

    queueStartTime = QTime::fromString(queueStartTimeStr, "hh:mm");
    if (!queueStartTime.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Invalid JobQueueWindowStart time '%1', using 00:00")
                        .arg(queueStartTimeStr));
        queueStartTime = QTime(0, 0);
    }

    queueEndTime = QTime::fromString(queueEndTimeStr, "hh:mm");
    if (!queueEndTime.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Invalid JobQueueWindowEnd time '%1', using 23:59")
                        .arg(queueEndTimeStr));
        queueEndTime = QTime(23, 59);
    }

    LOG(VB_JOBQUEUE, LOG_INFO, LOC +
        QString("Currently set to run new jobs from %1 to %2")
                    .arg(queueStartTimeStr).arg(queueEndTimeStr));

    if ((queueStartTime <= curTime) && (curTime < queueEndTime))
    {
        inTimeWindow = true;
    }
    else if ((queueStartTime > queueEndTime) &&
             ((curTime < queueEndTime) || (queueStartTime <= curTime)))
    {
        inTimeWindow = true;
    }
    else if (orStartsWithinMins > 0)
    {
        // Check if the window starts soon
        if (curTime <= queueStartTime)
        {
            // Start time hasn't passed yet today
            if (queueStartTime.secsTo(curTime) <= (orStartsWithinMins * 60))
            {
                LOG(VB_JOBQUEUE, LOG_INFO, LOC +
                    QString("Job run window will start within %1 minutes")
                            .arg(orStartsWithinMins));
                inTimeWindow = true;
            }
        }
        else
        {
            // We passed the start time for today, try tomorrow
            QDateTime curDateTime = MythDate::current();
            QDateTime startDateTime = QDateTime(
                curDateTime.date(), queueStartTime, Qt::UTC).addDays(1);

            if (curDateTime.secsTo(startDateTime) <= (orStartsWithinMins * 60))
            {
                LOG(VB_JOBQUEUE, LOG_INFO, LOC +
                    QString("Job run window will start "
                            "within %1 minutes (tomorrow)")
                        .arg(orStartsWithinMins));
                inTimeWindow = true;
            }
        }
    }

    return inTimeWindow;
}

bool JobQueue::HasRunningOrPendingJobs(int startingWithinMins)
{
    /* startingWithinMins <= 0 - look for any pending jobs
           > 0 -  only consider pending starting within this time */
    QMap<int, JobQueueEntry> jobs;
    QMap<int, JobQueueEntry>::Iterator it;
    QDateTime maxSchedRunTime = MythDate::current();
    int tmpStatus = 0;
    bool checkForQueuedJobs = (startingWithinMins <= 0
                                || InJobRunWindow(startingWithinMins));

    if (checkForQueuedJobs && startingWithinMins > 0) {
        maxSchedRunTime = maxSchedRunTime.addSecs(startingWithinMins * 60);
        LOG(VB_JOBQUEUE, LOG_INFO, LOC +
            QString("HasRunningOrPendingJobs: checking for jobs "
                    "starting before: %1")
            .arg(maxSchedRunTime.toString(Qt::ISODate)));
    }

    JobQueue::GetJobsInQueue(jobs, JOB_LIST_NOT_DONE);

    if (jobs.size()) {
        for (it = jobs.begin(); it != jobs.end(); ++it)
        {
            tmpStatus = (*it).status;
            if (tmpStatus == JOB_RUNNING) {
                LOG(VB_JOBQUEUE, LOG_INFO, LOC +
                    QString("HasRunningOrPendingJobs: found running job"));
                return true;
            }

            if (checkForQueuedJobs) {
                if ((tmpStatus != JOB_UNKNOWN) && (!(tmpStatus & JOB_DONE))) {
                    if (startingWithinMins <= 0) {
                        LOG(VB_JOBQUEUE, LOG_INFO, LOC +
                            "HasRunningOrPendingJobs: found pending job");
                        return true;
                    }
                    else if ((*it).schedruntime <= maxSchedRunTime) {
                        LOG(VB_JOBQUEUE, LOG_INFO, LOC +
                            QString("HasRunningOrPendingJobs: found pending "
                                    "job scheduled to start at: %1")
                            .arg((*it).schedruntime.toString(Qt::ISODate)));
                        return true;
                    }
                }
            }
        }
    }
    return false;
}


int JobQueue::GetJobsInQueue(QMap<int, JobQueueEntry> &jobs, int findJobs)
{
    JobQueueEntry thisJob;
    MSqlQuery query(MSqlQuery::InitCon());
    QDateTime recentDate = MythDate::current().addSecs(-4 * 3600);
    QString logInfo;
    int jobCount = 0;
    bool commflagWhileRecording =
             gCoreContext->GetNumSetting("AutoCommflagWhileRecording", 0);

    jobs.clear();

    query.prepare("SELECT j.id, j.chanid, j.starttime, j.inserttime, j.type, "
                      "j.cmds, j.flags, j.status, j.statustime, j.hostname, "
                      "j.args, j.comment, r.endtime, j.schedruntime "
                  "FROM jobqueue j "
                  "LEFT JOIN recorded r "
                  "  ON j.chanid = r.chanid AND j.starttime = r.starttime "
                  "ORDER BY j.schedruntime, j.id;");

    if (!query.exec())
    {
        MythDB::DBError("Error in JobQueue::GetJobs(), Unable to "
                        "query list of Jobs in Queue.", query);
        return 0;
    }

    LOG(VB_JOBQUEUE, LOG_INFO, LOC +
        QString("GetJobsInQueue: findJobs search bitmask %1, "
                "found %2 total jobs")
                    .arg(findJobs).arg(query.size()));

    while (query.next())
    {
        bool wantThisJob = false;

        thisJob.id = query.value(0).toInt();
        thisJob.recstartts = MythDate::as_utc(query.value(2).toDateTime());
        thisJob.schedruntime = MythDate::as_utc(query.value(13).toDateTime());
        thisJob.type = query.value(4).toInt();
        thisJob.status = query.value(7).toInt();
        thisJob.statustime = MythDate::as_utc(query.value(8).toDateTime());
        thisJob.startts = MythDate::toString(
            thisJob.recstartts, MythDate::kFilename);

        // -1 indicates the chanid is empty
        if (query.value(1).toInt() == -1)
        {
            thisJob.chanid = 0;
            logInfo = QString("jobID #%1").arg(thisJob.id);
        }
        else
        {
            thisJob.chanid = query.value(1).toUInt();
            logInfo = QString("chanid %1 @ %2").arg(thisJob.chanid)
                              .arg(thisJob.startts);
        }

        if ((MythDate::as_utc(query.value(12).toDateTime()) > MythDate::current()) &&
            ((!commflagWhileRecording) ||
             ((thisJob.type != JOB_COMMFLAG) &&
              (thisJob.type != JOB_METADATA))))
        {
            LOG(VB_JOBQUEUE, LOG_INFO, LOC +
                QString("GetJobsInQueue: Ignoring '%1' Job "
                        "for %2 in %3 state.  Endtime in future.")
                            .arg(JobText(thisJob.type))
                            .arg(logInfo).arg(StatusText(thisJob.status)));
            continue;
        }

        if ((findJobs & JOB_LIST_ALL) ||
            ((findJobs & JOB_LIST_DONE) &&
             (thisJob.status & JOB_DONE)) ||
            ((findJobs & JOB_LIST_NOT_DONE) &&
             (!(thisJob.status & JOB_DONE))) ||
            ((findJobs & JOB_LIST_ERROR) &&
             (thisJob.status == JOB_ERRORED)) ||
            ((findJobs & JOB_LIST_RECENT) &&
             (thisJob.statustime > recentDate)))
            wantThisJob = true;

        if (!wantThisJob)
        {
            LOG(VB_JOBQUEUE, LOG_INFO, LOC +
                QString("GetJobsInQueue: Ignore '%1' Job for %2 in %3 state.")
                            .arg(JobText(thisJob.type))
                            .arg(logInfo).arg(StatusText(thisJob.status)));
            continue;
        }

        LOG(VB_JOBQUEUE, LOG_INFO, LOC +
            QString("GetJobsInQueue: Found '%1' Job for %2 in %3 state.")
                        .arg(JobText(thisJob.type))
                        .arg(logInfo).arg(StatusText(thisJob.status)));

        thisJob.inserttime = MythDate::as_utc(query.value(3).toDateTime());
        thisJob.cmds = query.value(5).toInt();
        thisJob.flags = query.value(6).toInt();
        thisJob.hostname = query.value(9).toString();
        thisJob.args = query.value(10).toString();
        thisJob.comment = query.value(11).toString();

        if ((thisJob.type & JOB_USERJOB) &&
            (UserJobTypeToIndex(thisJob.type) == 0))
        {
            thisJob.type = JOB_NONE;
            LOG(VB_JOBQUEUE, LOG_INFO, LOC +
                QString("GetJobsInQueue: Unknown Job Type: %1")
                    .arg(thisJob.type));
        }

        if (thisJob.type != JOB_NONE)
            jobs[jobCount++] = thisJob;
    }

    return jobCount;
}

bool JobQueue::ChangeJobHost(int jobID, QString newHostname)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!newHostname.isEmpty())
    {
        query.prepare("UPDATE jobqueue SET hostname = :NEWHOSTNAME "
                      "WHERE hostname = :EMPTY AND id = :ID;");
        query.bindValue(":NEWHOSTNAME", newHostname);
        query.bindValue(":EMPTY", "");
        query.bindValue(":ID", jobID);
    }
    else
    {
        query.prepare("UPDATE jobqueue SET hostname = :EMPTY "
                      "WHERE id = :ID;");
        query.bindValue(":EMPTY", "");
        query.bindValue(":ID", jobID);
    }

    if (!query.exec())
    {
        MythDB::DBError(QString("Error in JobQueue::ChangeJobHost(), "
                                "Unable to set hostname to '%1' for "
                                "job %2.").arg(newHostname).arg(jobID),
                        query);
        return false;
    }

    if (query.numRowsAffected() > 0)
        return true;

    return false;
}

bool JobQueue::AllowedToRun(JobQueueEntry job)
{
    QString allowSetting;

    if ((!job.hostname.isEmpty()) &&
        (job.hostname != m_hostname))
        return false;

    if (job.type & JOB_USERJOB)
    {
        allowSetting =
            QString("JobAllowUserJob%1").arg(UserJobTypeToIndex(job.type));
    }
    else
    {
        switch (job.type)
        {
            case JOB_TRANSCODE:  allowSetting = "JobAllowTranscode";
                                 break;
            case JOB_COMMFLAG:   allowSetting = "JobAllowCommFlag";
                                 break;
            case JOB_METADATA:   allowSetting = "JobAllowMetadata";
                                 break;
            default:             return false;
        }
    }

    if (gCoreContext->GetNumSetting(allowSetting, 1))
        return true;

    return false;
}

enum JobCmds JobQueue::GetJobCmd(int jobID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cmds FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    if (query.exec())
    {
        if (query.next())
            return (enum JobCmds)query.value(0).toInt();
    }
    else
    {
        MythDB::DBError("Error in JobQueue::GetJobCmd()", query);
    }

    return JOB_RUN;
}

QString JobQueue::GetJobArgs(int jobID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT args FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    if (query.exec())
    {
        if (query.next())
            return query.value(0).toString();
    }
    else
    {
        MythDB::DBError("Error in JobQueue::GetJobArgs()", query);
    }

    return QString("");
}

enum JobFlags JobQueue::GetJobFlags(int jobID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT flags FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    if (query.exec())
    {
        if (query.next())
            return (enum JobFlags)query.value(0).toInt();
    }
    else
    {
        MythDB::DBError("Error in JobQueue::GetJobFlags()", query);
    }

    return JOB_NO_FLAGS;
}

enum JobStatus JobQueue::GetJobStatus(int jobID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT status FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    if (query.exec())
    {
        if (query.next())
            return (enum JobStatus)query.value(0).toInt();
    }
    else
    {
        MythDB::DBError("Error in JobQueue::GetJobStatus()", query);
    }
    return JOB_UNKNOWN;
}

enum JobStatus JobQueue::GetJobStatus(
    int jobType, uint chanid, const QDateTime &recstartts)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT status FROM jobqueue WHERE type = :TYPE "
                  "AND chanid = :CHANID AND starttime = :STARTTIME;");

    query.bindValue(":TYPE", jobType);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec())
    {
        if (query.next())
            return (enum JobStatus)query.value(0).toInt();
    }
    else
    {
        MythDB::DBError("Error in JobQueue::GetJobStatus()", query);
    }
    return JOB_UNKNOWN;
}

void JobQueue::RecoverQueue(bool justOld)
{
    QMap<int, JobQueueEntry> jobs;
    QString msg;
    QString logInfo;

    msg = QString("RecoverQueue: Checking for unfinished jobs to "
                  "recover.");
    LOG(VB_JOBQUEUE, LOG_INFO, LOC + msg);

    GetJobsInQueue(jobs);

    if (jobs.size())
    {
        QMap<int, JobQueueEntry>::Iterator it;
        QDateTime oldDate = MythDate::current().addDays(-1);
        QString hostname = gCoreContext->GetHostName();
        int tmpStatus;
        int tmpCmds;

        for (it = jobs.begin(); it != jobs.end(); ++it)
        {
            tmpCmds = (*it).cmds;
            tmpStatus = (*it).status;

            if (!(*it).chanid)
                logInfo = QString("jobID #%1").arg((*it).id);
            else
                logInfo = QString("chanid %1 @ %2").arg((*it).chanid)
                                  .arg((*it).startts);

            if (((tmpStatus == JOB_STARTING) ||
                 (tmpStatus == JOB_RUNNING) ||
                 (tmpStatus == JOB_PAUSED) ||
                 (tmpCmds & JOB_STOP) ||
                 (tmpStatus == JOB_STOPPING)) &&
                (((!justOld) &&
                  ((*it).hostname == hostname)) ||
                 ((*it).statustime < oldDate)))
            {
                msg = QString("RecoverQueue: Recovering '%1' for %2 "
                              "from '%3' state.")
                              .arg(JobText((*it).type))
                              .arg(logInfo).arg(StatusText((*it).status));
                LOG(VB_JOBQUEUE, LOG_INFO, LOC + msg);

                ChangeJobStatus((*it).id, JOB_QUEUED, "");
                ChangeJobCmds((*it).id, JOB_RUN);
                if (!gCoreContext->GetNumSetting("JobsRunOnRecordHost", 0))
                    ChangeJobHost((*it).id, "");
            }
            else
            {
#if 0
                msg = QString("RecoverQueue: Ignoring '%1' for %2 "
                              "in '%3' state.")
                              .arg(JobText((*it).type))
                              .arg(logInfo).arg(StatusText((*it).status));
                LOG(VB_JOBQUEUE, LOG_INFO, LOC + msg);
#endif
            }
        }
    }
}

void JobQueue::CleanupOldJobsInQueue()
{
    MSqlQuery delquery(MSqlQuery::InitCon());
    QDateTime donePurgeDate = MythDate::current().addDays(-2);
    QDateTime errorsPurgeDate = MythDate::current().addDays(-4);

    delquery.prepare("DELETE FROM jobqueue "
                     "WHERE (status in (:FINISHED, :ABORTED, :CANCELLED) "
                     "AND statustime < :DONEPURGEDATE) "
                     "OR (status in (:ERRORED) "
                     "AND statustime < :ERRORSPURGEDATE) ");
    delquery.bindValue(":FINISHED", JOB_FINISHED);
    delquery.bindValue(":ABORTED", JOB_ABORTED);
    delquery.bindValue(":CANCELLED", JOB_CANCELLED);
    delquery.bindValue(":ERRORED", JOB_ERRORED);
    delquery.bindValue(":DONEPURGEDATE", donePurgeDate);
    delquery.bindValue(":ERRORSPURGEDATE", errorsPurgeDate);

    if (!delquery.exec())
    {
        MythDB::DBError("JobQueue::CleanupOldJobsInQueue: Error deleting "
                        "old finished jobs.", delquery);
    }
}

void JobQueue::ProcessJob(JobQueueEntry job)
{
    int jobID = job.id;
    QString name = QString("jobqueue%1%2").arg(jobID).arg(random());

    if (!MSqlQuery::testDBConnection())
    {
        LOG(VB_JOBQUEUE, LOG_ERR, LOC +
                "ProcessJob(): Unable to open database connection");
        return;
    }

    ChangeJobStatus(jobID, JOB_PENDING);
    ProgramInfo *pginfo = NULL;

    if (job.chanid)
    {
        pginfo = new ProgramInfo(job.chanid, job.recstartts);

        if (!pginfo->GetChanID())
        {
            LOG(VB_JOBQUEUE, LOG_ERR, LOC +
                QString("Unable to retrieve program info for chanid %1 @ %2")
                .arg(job.chanid)
                .arg(job.recstartts.toString(Qt::ISODate)));

            ChangeJobStatus(jobID, JOB_ERRORED,
                            tr("Unable to retrieve program info from database"));

            delete pginfo;

            return;
        }

        pginfo->SetPathname(pginfo->GetPlaybackURL());
    }


    runningJobsLock->lock();

    ChangeJobStatus(jobID, JOB_STARTING);
    RunningJobInfo jInfo;
    jInfo.type    = job.type;
    jInfo.id      = jobID;
    jInfo.flag    = JOB_RUN;
    jInfo.desc    = GetJobDescription(job.type);
    jInfo.command = GetJobCommand(jobID, job.type, pginfo);
    jInfo.pginfo  = pginfo;

    runningJobs[jobID] = jInfo;

    if (pginfo)
        pginfo->MarkAsInUse(true, kJobQueueInUseID);

    if (pginfo && pginfo->GetRecordingGroup() == "Deleted")
    {
        ChangeJobStatus(jobID, JOB_CANCELLED,
                        tr("Program has been deleted"));
        RemoveRunningJob(jobID);
    }
    else if ((job.type == JOB_TRANSCODE) ||
        (runningJobs[jobID].command == "mythtranscode"))
    {
        StartChildJob(TranscodeThread, jobID);
    }
    else if ((job.type == JOB_COMMFLAG) ||
             (runningJobs[jobID].command == "mythcommflag"))
    {
        StartChildJob(FlagCommercialsThread, jobID);
    }
    else if ((job.type == JOB_METADATA) ||
             (runningJobs[jobID].command == "mythmetadatalookup"))
    {
        StartChildJob(MetadataLookupThread, jobID);
    }
    else if (job.type & JOB_USERJOB)
    {
        StartChildJob(UserJobThread, jobID);
    }
    else
    {
        ChangeJobStatus(jobID, JOB_ERRORED,
                        tr("UNKNOWN JobType, unable to process!"));
        RemoveRunningJob(jobID);
    }

    runningJobsLock->unlock();
}

void JobQueue::StartChildJob(void *(*ChildThreadRoutine)(void *), int jobID)
{
    JobThreadStruct *jts = new JobThreadStruct;
    jts->jq = this;
    jts->jobID = jobID;

    pthread_t childThread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&childThread, &attr, ChildThreadRoutine, jts);
    pthread_attr_destroy(&attr);
}

QString JobQueue::GetJobDescription(int jobType)
{
    if (jobType == JOB_TRANSCODE)
        return "Transcode";
    else if (jobType == JOB_COMMFLAG)
        return "Commercial Detection";
    else if (!(jobType & JOB_USERJOB))
        return "Unknown Job";

    QString descSetting =
        QString("UserJobDesc%1").arg(UserJobTypeToIndex(jobType));

    return gCoreContext->GetSetting(descSetting, "Unknown Job");
}

QString JobQueue::GetJobCommand(int id, int jobType, ProgramInfo *tmpInfo)
{
    QString command;
    MSqlQuery query(MSqlQuery::InitCon());

    if (jobType == JOB_TRANSCODE)
    {
        command = gCoreContext->GetSetting("JobQueueTranscodeCommand");
        if (command.trimmed().isEmpty())
            command = "mythtranscode";

        if (command == "mythtranscode")
            return command;
    }
    else if (jobType == JOB_COMMFLAG)
    {
        command = gCoreContext->GetSetting("JobQueueCommFlagCommand");
        if (command.trimmed().isEmpty())
            command = "mythcommflag";

        if (command == "mythcommflag")
            return command;
    }
    else if (jobType & JOB_USERJOB)
    {
        command = gCoreContext->GetSetting(
                    QString("UserJob%1").arg(UserJobTypeToIndex(jobType)), "");
    }

    if (!command.isEmpty())
    {
        command.replace("%JOBID%", QString("%1").arg(id));
    }

    if (!command.isEmpty() && tmpInfo)
    {
        tmpInfo->SubstituteMatches(command);

        command.replace("%VERBOSELEVEL%", QString("%1").arg(verboseMask));
        command.replace("%VERBOSEMODE%", QString("%1").arg(logPropagateArgs));

        uint transcoder = tmpInfo->QueryTranscoderID();
        command.replace("%TRANSPROFILE%",
                        (RecordingProfile::TranscoderAutodetect == transcoder) ?
                        "autodetect" : QString::number(transcoder));
    }

    return command;
}

void JobQueue::RemoveRunningJob(int id)
{
    runningJobsLock->lock();

    if (runningJobs.contains(id))
    {
        ProgramInfo *pginfo = runningJobs[id].pginfo;
        if (pginfo)
        {
            pginfo->MarkAsInUse(false, kJobQueueInUseID);
            delete pginfo;
        }

        runningJobs.remove(id);
    }

    runningJobsLock->unlock();
}

QString JobQueue::PrettyPrint(off_t bytes)
{
    // Pretty print "bytes" as KB, MB, GB, TB, etc., subject to the desired
    // number of units
    static const struct {
        const char      *suffix;
        unsigned int    max;
        int         precision;
    } pptab[] = {
        { "bytes", 9999, 0 },
        { "kB", 999, 0 },
        { "MB", 999, 1 },
        { "GB", 999, 1 },
        { "TB", 999, 1 },
        { "PB", 999, 1 },
        { "EB", 999, 1 },
        { "ZB", 999, 1 },
        { "YB", 0, 0 },
    };
    unsigned int    ii;
    float           fbytes = bytes;

    ii = 0;
    while (pptab[ii].max && fbytes > pptab[ii].max) {
        fbytes /= 1024;
        ii++;
    }

    return QString("%1 %2")
        .arg(fbytes, 0, 'f', pptab[ii].precision)
        .arg(pptab[ii].suffix);
}

void *JobQueue::TranscodeThread(void *param)
{
    JobThreadStruct *jts = (JobThreadStruct *)param;
    JobQueue *jq = jts->jq;

    MThread::ThreadSetup(QString("Transcode_%1").arg(jts->jobID));
    jq->DoTranscodeThread(jts->jobID);
    MThread::ThreadCleanup();

    delete jts;

    return NULL;
}

void JobQueue::DoTranscodeThread(int jobID)
{
    // We can't currently transcode non-recording files w/o a ProgramInfo
    runningJobsLock->lock();
    if (!runningJobs[jobID].pginfo)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, LOC +
            "The JobQueue cannot currently transcode files that do not "
            "have a chanid/starttime in the recorded table.");
        ChangeJobStatus(jobID, JOB_ERRORED, "ProgramInfo data not found");
        RemoveRunningJob(jobID);
        runningJobsLock->unlock();
        return;
    }

    ProgramInfo *program_info = runningJobs[jobID].pginfo;
    runningJobsLock->unlock();

    ChangeJobStatus(jobID, JOB_RUNNING);

    // make sure flags are up to date
    program_info->Reload();

    bool useCutlist = program_info->HasCutlist() &&
        !!(GetJobFlags(jobID) & JOB_USE_CUTLIST);

    uint transcoder = program_info->QueryTranscoderID();
    QString profilearg =
        (RecordingProfile::TranscoderAutodetect == transcoder) ?
        "autodetect" : QString::number(transcoder);

    QString path;
    QString command;

    runningJobsLock->lock();
    if (runningJobs[jobID].command == "mythtranscode")
    {
        path = GetInstallPrefix() + "/bin/mythtranscode";
        command = QString("%1 -j %2 --profile %3")
                  .arg(path).arg(jobID).arg(profilearg);
        if (useCutlist)
            command += " --honorcutlist";
        command += logPropagateArgs;
    }
    else
    {
        command = runningJobs[jobID].command;

        QStringList tokens = command.split(" ", QString::SkipEmptyParts);
        if (!tokens.empty())
            path = tokens[0];
    }
    runningJobsLock->unlock();

    if (jobQueueCPU < 2)
    {
        myth_nice(17);
        myth_ioprio((0 == jobQueueCPU) ? 8 : 7);
    }

    QString transcoderName;
    if (transcoder == RecordingProfile::TranscoderAutodetect)
    {
        transcoderName = "Autodetect";
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT name FROM recordingprofiles WHERE id = :ID;");
        query.bindValue(":ID", transcoder);
        if (query.exec() && query.next())
        {
            transcoderName = query.value(0).toString();
        }
        else
        {
            /* Unexpected value; log it. */
            transcoderName = QString("Autodetect(%1)").arg(transcoder);
        }
    }

    QString msg;
    bool retry = true;
    int retrylimit = 3;
    while (retry)
    {
        retry = false;

        ChangeJobStatus(jobID, JOB_STARTING);
        program_info->SaveTranscodeStatus(TRANSCODING_RUNNING);

        QString filename = program_info->GetPlaybackURL(false, true);

        long long filesize = 0;
        long long origfilesize = QFileInfo(filename).size();

        QString msg = QString("Transcode %1")
                              .arg(StatusText(GetJobStatus(jobID)));

        QString detailstr = QString("%1: %2 (%3)")
            .arg(program_info->toString(ProgramInfo::kTitleSubtitle))
            .arg(transcoderName)
            .arg(PrettyPrint(origfilesize));
        QByteArray details = detailstr.toLocal8Bit();

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 for %2")
                .arg(msg).arg(details.constData()));

        LOG(VB_JOBQUEUE, LOG_INFO, LOC + QString("Running command: '%1'")
                                           .arg(command));

        GetMythDB()->GetDBManager()->CloseDatabases();
        uint result = myth_system(command);
        int status = GetJobStatus(jobID);

        if ((result == GENERIC_EXIT_DAEMONIZING_ERROR) ||
            (result == GENERIC_EXIT_CMD_NOT_FOUND))
        {
            ChangeJobStatus(jobID, JOB_ERRORED,
                tr("ERROR: Unable to find mythtranscode, check backend logs."));
            program_info->SaveTranscodeStatus(TRANSCODING_NOT_TRANSCODED);

            msg = QString("Transcode %1").arg(StatusText(GetJobStatus(jobID)));
            detailstr = QString("%1: %2 does not exist or is not executable")
                .arg(program_info->toString(ProgramInfo::kTitleSubtitle))
                .arg(path);
            details = detailstr.toLocal8Bit();

            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("%1 for %2").arg(msg).arg(details.constData()));
        }
        else if (result == GENERIC_EXIT_RESTART && retrylimit > 0)
        {
            LOG(VB_JOBQUEUE, LOG_INFO, LOC + "Transcode command restarting");
            retry = true;
            retrylimit--;

            program_info->SaveTranscodeStatus(TRANSCODING_NOT_TRANSCODED);
        }
        else
        {
            if (status == JOB_FINISHED)
            {
                ChangeJobStatus(jobID, JOB_FINISHED, tr("Finished."));
                retry = false;

                filename = program_info->GetPlaybackURL(false, true);
                QFileInfo st(filename);

                if (st.exists())
                {
                    filesize = st.size();
                    /*: %1 is transcoder name, %2 is the original file size 
                        and %3 is the current file size */
                    QString comment = tr("%1: %2 => %3")
                                        .arg(transcoderName)
                                        .arg(PrettyPrint(origfilesize))
                                        .arg(PrettyPrint(filesize));
                    ChangeJobComment(jobID, comment);

                    if (filesize > 0)
                        program_info->SaveFilesize(filesize);

                    details = (QString("%1: %2 (%3)")
                               .arg(program_info->toString(
                                        ProgramInfo::kTitleSubtitle))
                               .arg(transcoderName)
                               .arg(PrettyPrint(filesize))).toLocal8Bit();
                }
                else
                {
                    QString comment =
                        QString("could not stat '%1'").arg(filename);

                    ChangeJobStatus(jobID, JOB_FINISHED, comment);

                    details = (QString("%1: %2")
                               .arg(program_info->toString(
                                        ProgramInfo::kTitleSubtitle))
                               .arg(comment)).toLocal8Bit();
                }

                program_info->SaveTranscodeStatus(TRANSCODING_COMPLETE);
            }
            else
            {
                program_info->SaveTranscodeStatus(TRANSCODING_NOT_TRANSCODED);

                QString comment = tr("exit status %1, job status was \"%2\"")
                                    .arg(result)
                                    .arg(StatusText(status));

                ChangeJobStatus(jobID, JOB_ERRORED, comment);

                details = (QString("%1: %2 (%3)")
                           .arg(program_info->toString(
                                    ProgramInfo::kTitleSubtitle))
                           .arg(transcoderName)
                           .arg(comment)).toLocal8Bit().constData();
            }

            msg = QString("Transcode %1").arg(StatusText(GetJobStatus(jobID)));
            LOG(VB_GENERAL, LOG_INFO, LOC + msg + ": " + details);
        }
    }

    if (retrylimit == 0)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, LOC + "Retry limit exceeded for transcoder, "
                                        "setting job status to errored.");
        ChangeJobStatus(jobID, JOB_ERRORED, tr("Retry limit exceeded"));
    }

    RemoveRunningJob(jobID);
}

void *JobQueue::MetadataLookupThread(void *param)
{
    JobThreadStruct *jts = (JobThreadStruct *)param;
    JobQueue *jq = jts->jq;

    MThread::ThreadSetup(QString("Metadata_%1").arg(jts->jobID));
    jq->DoMetadataLookupThread(jts->jobID);
    MThread::ThreadCleanup();

    delete jts;

    return NULL;
}

void JobQueue::DoMetadataLookupThread(int jobID)
{
    // We can't currently lookup non-recording files w/o a ProgramInfo
    runningJobsLock->lock();
    if (!runningJobs[jobID].pginfo)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, LOC +
            "The JobQueue cannot currently perform lookups for items which do "
            "not have a chanid/starttime in the recorded table.");
        ChangeJobStatus(jobID, JOB_ERRORED, "ProgramInfo data not found");
        RemoveRunningJob(jobID);
        runningJobsLock->unlock();
        return;
    }

    ProgramInfo *program_info = runningJobs[jobID].pginfo;
    runningJobsLock->unlock();

    QString detailstr = QString("%1 recorded from channel %3")
        .arg(program_info->toString(ProgramInfo::kTitleSubtitle))
        .arg(program_info->toString(ProgramInfo::kRecordingKey));
    QByteArray details = detailstr.toLocal8Bit();

    if (!MSqlQuery::testDBConnection())
    {
        QString msg = QString("Metadata Lookup failed.  Could not open "
                              "new database connection for %1. "
                              "Program cannot be looked up.")
                              .arg(details.constData());
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);

        ChangeJobStatus(jobID, JOB_ERRORED,
                        tr("Could not open new database connection for "
                        "metadata lookup."));

        delete program_info;
        return;
    }

    QString msg = tr("Metadata Lookup Starting");
    LOG(VB_GENERAL, LOG_INFO,
        LOC + "Metadata Lookup Starting for " + detailstr);

    uint retVal = 0;
    QString path;
    QString command;

    path = GetInstallPrefix() + "/bin/mythmetadatalookup";
    command = QString("%1 -j %2")
                      .arg(path).arg(jobID);
    command += logPropagateArgs;

    LOG(VB_JOBQUEUE, LOG_INFO, LOC + QString("Running command: '%1'")
            .arg(command));

    GetMythDB()->GetDBManager()->CloseDatabases();
    retVal = myth_system(command);
    int priority = LOG_NOTICE;
    QString comment;

    runningJobsLock->lock();

    if ((retVal == GENERIC_EXIT_DAEMONIZING_ERROR) ||
        (retVal == GENERIC_EXIT_CMD_NOT_FOUND))
    {
        comment = tr("Unable to find mythmetadatalookup");
        ChangeJobStatus(jobID, JOB_ERRORED, comment);
        priority = LOG_WARNING;
    }
    else if (runningJobs[jobID].flag == JOB_STOP)
    {
        comment = tr("Aborted by user");
        ChangeJobStatus(jobID, JOB_ABORTED, comment);
        priority = LOG_WARNING;
    }
    else if (retVal == GENERIC_EXIT_NO_RECORDING_DATA)
    {
        comment = tr("Unable to open file or init decoder");
        ChangeJobStatus(jobID, JOB_ERRORED, comment);
        priority = LOG_WARNING;
    }
    else if (retVal >= GENERIC_EXIT_NOT_OK) // 256 or above - error
    {
        comment = tr("Failed with exit status %1").arg(retVal);
        ChangeJobStatus(jobID, JOB_ERRORED, comment);
        priority = LOG_WARNING;
    }
    else
    {
        comment = tr("Metadata Lookup Complete.");
        ChangeJobStatus(jobID, JOB_FINISHED, comment);

        program_info->SendUpdateEvent();
    }

    msg = tr("Metadata Lookup %1", "Job ID")
        .arg(StatusText(GetJobStatus(jobID)));

    if (!comment.isEmpty())
    {
        detailstr += QString(" (%1)").arg(comment);
        details = detailstr.toLocal8Bit();
    }

    if (priority <= LOG_WARNING)
        LOG(VB_GENERAL, LOG_ERR, LOC + msg + ": " + details.constData());

    RemoveRunningJob(jobID);
    runningJobsLock->unlock();
}

void *JobQueue::FlagCommercialsThread(void *param)
{
    JobThreadStruct *jts = (JobThreadStruct *)param;
    JobQueue *jq = jts->jq;

    MThread::ThreadSetup(QString("Commflag_%1").arg(jts->jobID));
    jq->DoFlagCommercialsThread(jts->jobID);
    MThread::ThreadCleanup();

    delete jts;

    return NULL;
}

void JobQueue::DoFlagCommercialsThread(int jobID)
{
    // We can't currently commflag non-recording files w/o a ProgramInfo
    runningJobsLock->lock();
    if (!runningJobs[jobID].pginfo)
    {
        LOG(VB_JOBQUEUE, LOG_ERR, LOC +
                "The JobQueue cannot currently commflag files that do not "
                "have a chanid/starttime in the recorded table.");
        ChangeJobStatus(jobID, JOB_ERRORED, "ProgramInfo data not found");
        RemoveRunningJob(jobID);
        runningJobsLock->unlock();
        return;
    }

    ProgramInfo *program_info = runningJobs[jobID].pginfo;
    runningJobsLock->unlock();

    QString detailstr = QString("%1 recorded from channel %3")
        .arg(program_info->toString(ProgramInfo::kTitleSubtitle))
        .arg(program_info->toString(ProgramInfo::kRecordingKey));
    QByteArray details = detailstr.toLocal8Bit();

    if (!MSqlQuery::testDBConnection())
    {
        QString msg = QString("Commercial Detection failed.  Could not open "
                              "new database connection for %1. "
                              "Program cannot be flagged.")
                              .arg(details.constData());
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);

        ChangeJobStatus(jobID, JOB_ERRORED,
                        tr("Could not open new database connection for "
                        "commercial detector."));

        delete program_info;
        return;
    }

    QString msg = tr("Commercial Detection Starting");
    LOG(VB_GENERAL, LOG_INFO,
        LOC + "Commercial Detection Starting for " + detailstr);

    uint breaksFound = 0;
    QString path;
    QString command;

    runningJobsLock->lock();
    if (runningJobs[jobID].command == "mythcommflag")
    {
        path = GetInstallPrefix() + "/bin/mythcommflag";
        command = QString("%1 -j %2 --noprogress")
                          .arg(path).arg(jobID);
        command += logPropagateArgs;
    }
    else
    {
        command = runningJobs[jobID].command;
        QStringList tokens = command.split(" ", QString::SkipEmptyParts);
        if (!tokens.empty())
            path = tokens[0];
    }
    runningJobsLock->unlock();

    LOG(VB_JOBQUEUE, LOG_INFO, LOC + QString("Running command: '%1'")
            .arg(command));

    GetMythDB()->GetDBManager()->CloseDatabases();
    breaksFound = myth_system(command, kMSLowExitVal);
    int priority = LOG_NOTICE;
    QString comment;

    runningJobsLock->lock();

    if ((breaksFound == GENERIC_EXIT_DAEMONIZING_ERROR) ||
        (breaksFound == GENERIC_EXIT_CMD_NOT_FOUND))
    {
        comment = tr("Unable to find mythcommflag");
        ChangeJobStatus(jobID, JOB_ERRORED, comment);
        priority = LOG_WARNING;
    }
    else if (runningJobs[jobID].flag == JOB_STOP)
    {
        comment = tr("Aborted by user");
        ChangeJobStatus(jobID, JOB_ABORTED, comment);
        priority = LOG_WARNING;
    }
    else if (breaksFound == GENERIC_EXIT_NO_RECORDING_DATA)
    {
        comment = tr("Unable to open file or init decoder");
        ChangeJobStatus(jobID, JOB_ERRORED, comment);
        priority = LOG_WARNING;
    }
    else if (breaksFound >= GENERIC_EXIT_NOT_OK) // 256 or above - error
    {
        comment = tr("Failed with exit status %1").arg(breaksFound);
        ChangeJobStatus(jobID, JOB_ERRORED, comment);
        priority = LOG_WARNING;
    }
    else
    {
        comment = tr("%n commercial break(s)", "", breaksFound);
        ChangeJobStatus(jobID, JOB_FINISHED, comment);

        program_info->SendUpdateEvent();

        if (!program_info->IsLocal())
            program_info->SetPathname(program_info->GetPlaybackURL(false,true));
        if (program_info->IsLocal())
        {
            PreviewGenerator *pg = new PreviewGenerator(
                program_info, QString(), PreviewGenerator::kLocal);
            pg->Run();
            pg->deleteLater();
        }
    }

    msg = tr("Commercial Detection %1", "Job ID")
        .arg(StatusText(GetJobStatus(jobID)));

    if (!comment.isEmpty())
    {
        detailstr += QString(" (%1)").arg(comment);
        details = detailstr.toLocal8Bit();
    }

    if (priority <= LOG_WARNING)
        LOG(VB_GENERAL, LOG_ERR, LOC + msg + ": " + details.constData());

    RemoveRunningJob(jobID);
    runningJobsLock->unlock();
}

void *JobQueue::UserJobThread(void *param)
{
    JobThreadStruct *jts = (JobThreadStruct *)param;
    JobQueue *jq = jts->jq;

    MThread::ThreadSetup(QString("UserJob_%1").arg(jts->jobID));
    jq->DoUserJobThread(jts->jobID);
    MThread::ThreadCleanup();

    delete jts;

    return NULL;
}

void JobQueue::DoUserJobThread(int jobID)
{
    runningJobsLock->lock();
    ProgramInfo *pginfo = runningJobs[jobID].pginfo;
    QString jobDesc = runningJobs[jobID].desc;
    QString command = runningJobs[jobID].command;
    runningJobsLock->unlock();

    ChangeJobStatus(jobID, JOB_RUNNING);

    QString msg;

    if (pginfo)
    {
        msg = QString("Started %1 for %2 recorded from channel %3")
            .arg(jobDesc)
            .arg(pginfo->toString(ProgramInfo::kTitleSubtitle))
            .arg(pginfo->toString(ProgramInfo::kRecordingKey));
    }
    else
        msg = QString("Started %1 for jobID %2").arg(jobDesc).arg(jobID);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString(msg.toLocal8Bit().constData()));

    switch (jobQueueCPU)
    {
        case  0: myth_nice(17);
                 myth_ioprio(8);
                 break;
        case  1: myth_nice(10);
                 myth_ioprio(7);
                 break;
        case  2:
        default: break;
    }

    LOG(VB_JOBQUEUE, LOG_INFO, LOC + QString("Running command: '%1'")
                                       .arg(command));
    GetMythDB()->GetDBManager()->CloseDatabases();
    uint result = myth_system(command);

    if ((result == GENERIC_EXIT_DAEMONIZING_ERROR) ||
        (result == GENERIC_EXIT_CMD_NOT_FOUND))
    {
        msg = QString("User Job '%1' failed, unable to find "
                      "executable, check your PATH and backend logs.")
                      .arg(command);
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);
        LOG(VB_GENERAL, LOG_NOTICE, LOC + QString("Current PATH: '%1'")
                                            .arg(getenv("PATH")));

        ChangeJobStatus(jobID, JOB_ERRORED,
            tr("ERROR: Unable to find executable, check backend logs."));
    }
    else if (result != 0)
    {
        msg = QString("User Job '%1' failed.").arg(command);
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);

        ChangeJobStatus(jobID, JOB_ERRORED,
            tr("ERROR: User Job returned non-zero, check logs."));
    }
    else
    {
        if (pginfo)
        {
            msg = QString("Finished %1 for %2 recorded from channel %3")
                .arg(jobDesc)
                .arg(pginfo->toString(ProgramInfo::kTitleSubtitle))
                .arg(pginfo->toString(ProgramInfo::kRecordingKey));
        }
        else
            msg = QString("Finished %1 for jobID %2").arg(jobDesc).arg(jobID);

        LOG(VB_GENERAL, LOG_INFO, LOC + QString(msg.toLocal8Bit().constData()));

        ChangeJobStatus(jobID, JOB_FINISHED, tr("Successfully Completed."));

        if (pginfo)
            pginfo->SendUpdateEvent();
    }

    RemoveRunningJob(jobID);
}

int JobQueue::UserJobTypeToIndex(int jobType)
{
    if (jobType & JOB_USERJOB)
    {
        int x = ((jobType & JOB_USERJOB)>> 8);
        int bits = 1;
        while ((x != 0) && ((x & 0x01) == 0))
        {
            bits++;
            x = x >> 1;
        }
        if ( bits > 4 )
            return JOB_NONE;

        return bits;
    }
    return JOB_NONE;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
