#include <unistd.h>

#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qdatetime.h>
#include <qfileinfo.h>
#include <qregexp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <iostream>
using namespace std;

#include "exitcodes.h"
#include "jobqueue.h"
#include "programinfo.h"
#include "libmyth/mythcontext.h"
#include "libmyth/util.h"
#include "NuppelVideoPlayer.h"
#include "mythdbcon.h"
#include "previewgenerator.h"

#define LOC     QString("JobQueue: ")
#define LOC_ERR QString("JobQueue Error: ")

JobQueue::JobQueue(bool master)
{
    isMaster = master;
    m_hostname = gContext->GetHostName();

    jobQueueCPU = gContext->GetNumSetting("JobQueueCPU", 0);

    jobsRunning = 0;

    queuePoll = false;

#ifndef USING_VALGRIND
    pthread_create(&queueThread, NULL, QueueProcesserThread, this);

    while (!queuePoll)
        usleep(50);
#endif // USING_VALGRIND

    gContext->addListener(this);
}

JobQueue::~JobQueue(void)
{
    queuePoll = false;
    pthread_join(queueThread, NULL);

    gContext->removeListener(this);
}

void JobQueue::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message.left(9) == "LOCAL_JOB")
        {
            // LOCAL_JOB action type chanid starttime hostname
            QString msg;
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            QString action = tokens[1];
            int jobType = JOB_UNKNOWN;
            int jobID = -1;
            QString chanid;
            
            QDateTime starttime;
            QString detectionHost = "";

            if (tokens[2] == "ID")
            {
                jobID = tokens[3].toInt();
                GetJobInfoFromID(jobID, jobType, chanid, starttime);
            }
            else
            {
                jobType = tokens[2].toInt();
                chanid = tokens[3];
                starttime = QDateTime::fromString(tokens[4], Qt::ISODate);
                detectionHost = tokens[5];
            }

            if (jobType == JOB_UNKNOWN)
            {
                msg = QString("Unable to determine job info for message: "
                              "%1.  Program will not be flagged.")
                              .arg(message);
                VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
                return;
            }

            QString key = GetJobQueueKey(chanid, starttime);

            msg = QString("Received message '%1'").arg(message);
            VERBOSE(VB_JOBQUEUE, LOC + msg);

            if (((action == "STOP") ||
                 (action == "PAUSE") ||
                 (action == "RESTART") ||
                 (action == "RESUME" )) &&
                (jobControlFlags.contains(key)) &&
                (runningJobTypes.contains(key)) &&
                (runningJobTypes[key] == jobType))
            {
                controlFlagsLock.lock();

                if (action == "STOP")
                    *(jobControlFlags[key]) = JOB_STOP;
                else if (action == "PAUSE")
                    *(jobControlFlags[key]) = JOB_PAUSE;
                else if (action == "RESUME")
                    *(jobControlFlags[key]) = JOB_RUN;
                else if (action == "RESTART")
                    *(jobControlFlags[key]) = JOB_RESTART;

                controlFlagsLock.unlock();
            }
        }
    }
}

void JobQueue::RunQueueProcesser()
{
    queuePoll = true;

    RecoverQueue();

    sleep(10);

    ProcessQueue();
}

void *JobQueue::QueueProcesserThread(void *param)
{
    JobQueue *jobqueue = (JobQueue *)param;
    jobqueue->RunQueueProcesser();

    return NULL;
}

void JobQueue::ProcessQueue(void)
{
    VERBOSE(VB_JOBQUEUE, LOC + "ProcessQueue() started");

    QString chanid;
    QDateTime starttime;
    QString startts;
    int type;
    int id;
    int cmds;
    int flags;
    int status;
    QString hostname;
    int sleepTime;

    QMap<QString, int> jobStatus;
    int maxJobs;
    QString queueStartTimeStr;
    QString queueEndTimeStr;
    int queueStartTime;
    int queueEndTime;
    QTime curQTime;
    int curTime;
    QString message;
    QString tmpStr;
    QMap<int, JobQueueEntry> jobs;
    bool atMax = false;
    bool inTimeWindow = true;
    bool startedJobAlready = false;

    while (queuePoll)
    {
        startedJobAlready = false;
        sleepTime = gContext->GetNumSetting("JobQueueCheckFrequency", 30);
        queueStartTimeStr =
            gContext->GetSetting("JobQueueWindowStart", "00:00");
        queueEndTimeStr =
            gContext->GetSetting("JobQueueWindowEnd", "23:59");

        maxJobs = gContext->GetNumSetting("JobQueueMaxSimultaneousJobs", 3);
        VERBOSE(VB_JOBQUEUE, LOC +
                QString("Currently set at %1 job(s) max and to run new jobs "
                        "from %2 to %3").arg(maxJobs).arg(queueStartTimeStr)
                                        .arg(queueEndTimeStr));

        jobStatus.clear();

        GetJobsInQueue(jobs);
        
        if (jobs.size())
        {
            tmpStr = queueStartTimeStr;
            queueStartTime = tmpStr.replace(QRegExp(":"), "").toInt();
            tmpStr = queueEndTimeStr;
            queueEndTime = tmpStr.replace(QRegExp(":"), "").toInt();
            curQTime = QTime::currentTime();
            curTime = curQTime.hour() * 100 + curQTime.minute();
            inTimeWindow = false;

            if ((queueStartTime <= curTime) && (curTime < queueEndTime))
            {
                inTimeWindow = true;
            }
            else if ((queueStartTime > queueEndTime) &&
                     ((curTime < queueEndTime) || (queueStartTime <= curTime)))
            {
                inTimeWindow = true;
            }

            jobsRunning = 0;
            for (unsigned int x = 0; x < jobs.size(); x++)
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
                                   "started, next window starts at %1.")
                                   .arg(queueStartTimeStr);
                VERBOSE(VB_JOBQUEUE, LOC + message);
            }
            else if (jobsRunning >= maxJobs)
            {
                message += " (At Maximum, no new jobs can be started until "
                           "a running job completes)";

                if (!atMax)
                    VERBOSE(VB_JOBQUEUE, LOC + message);

                atMax = true;
            }
            else
            {
                VERBOSE(VB_JOBQUEUE, LOC + message);
                atMax = false;
            }


            for (unsigned int x = 0;
                 (x < jobs.size()) && (jobsRunning < maxJobs); x++)
            {
                id = jobs[x].id;
                chanid = jobs[x].chanid;
                startts = jobs[x].startts;
                starttime = jobs[x].starttime;
                type = jobs[x].type;
                cmds = jobs[x].cmds;
                flags = jobs[x].flags;
                status = jobs[x].status;
                hostname = jobs[x].hostname;

                QString key = GetJobQueueKey(chanid, startts);

                // Should we even be looking at this job?
                if ((inTimeWindow) &&
                    (hostname != "") &&
                    (hostname != m_hostname))
                {
                    // Setting the status here will prevent us from processing
                    // any other jobs for this recording until this one is
                    // completed on the remote host.
                    jobStatus[key] = status;

                    message = QString("Skipping '%1' job for chanid "
                                      "%2 @ %3, should run on '%4' instead")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts)
                                      .arg(hostname);
                    VERBOSE(VB_JOBQUEUE, LOC + message);
                    continue;
                }

                // Check to see if there was a previous job that is not done
                if ((inTimeWindow) &&
                    (jobStatus.contains(key)) &&
                    (!(jobStatus[key] & JOB_DONE)))
                {
                    message = QString("Skipping '%1' job for chanid "
                                      "%2 @ %3, there is another job for "
                                      "this recording with a status of '%4'")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts)
                                      .arg(StatusText(jobStatus[key]));
                    VERBOSE(VB_JOBQUEUE, LOC + message);
                    continue;
                }

                jobStatus[key] = status;

                // Are we allowed to run this job?
                if ((inTimeWindow) && (!AllowedToRun(jobs[x])))
                {
                    message = QString("Skipping '%1' job for chanid "
                                      "%2 @ %3, not allowed to run on this "
                                      "backend.")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, LOC + message);
                    continue;
                }

                if (cmds & JOB_STOP)
                {
                    // if we're trying to stop a job and it's not queued
                    //  then lets send a STOP command
                    if (status != JOB_QUEUED) {
                        message = QString("Stopping '%1' job for "
                                              "chanid %2 @ %3")
                                          .arg(JobText(type))
                                          .arg(chanid).arg(startts);
                        VERBOSE(VB_JOBQUEUE, LOC + message);
    
                        controlFlagsLock.lock();
                        if (jobControlFlags.contains(key))
                            *(jobControlFlags[key]) = JOB_STOP;
                        controlFlagsLock.unlock();
    
                        // ChangeJobCmds(m_db, id, JOB_RUN);
                        continue;
                    
                    // if we're trying to stop a job and it's still queued
                    //  then let's just change the status to cancelled so
                    //  we don't try to run it from the queue
                    } else {
                        message = QString("Cancelling '%1' job for "
                                          "chanid %2 @ %3")
                                          .arg(JobText(type))
                                          .arg(chanid).arg(startts);
                        VERBOSE(VB_JOBQUEUE, LOC + message);

                        // at the bottom of this loop we requeue any jobs that
                        //  are not currently queued and also not associated 
                        //  with a hostname so we must claim this job before we
                        //  can cancel it
                        if (!ChangeJobHost(id, m_hostname))
                        {
                            message = QString("Unable to clain '%1' job "
                                              "for chanid %2 @ %3.")
                                              .arg(JobText(type))
                                              .arg(chanid).arg(startts);
                            VERBOSE(VB_JOBQUEUE, LOC_ERR + message);
                            continue;
                        }

                        ChangeJobStatus(id, JOB_CANCELLED, "");
                        ChangeJobCmds(id, JOB_RUN);
                        continue;
                    }
                }

                if ((cmds & JOB_PAUSE) && (status != JOB_QUEUED))
                {
                    message = QString("Pausing '%1' job for chanid %2 @ %3")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, LOC + message);

                    controlFlagsLock.lock();
                    if (jobControlFlags.contains(key))
                        *(jobControlFlags[key]) = JOB_PAUSE;
                    controlFlagsLock.unlock();

                    ChangeJobCmds(id, JOB_RUN);
                    continue;
                }

                if ((cmds & JOB_RESTART) && (status != JOB_QUEUED))
                {
                    message = QString("Restart '%1' job for chanid %2 @ %3")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, LOC + message);

                    controlFlagsLock.lock();
                    if (jobControlFlags.contains(key))
                        *(jobControlFlags[key]) = JOB_RUN;
                    controlFlagsLock.unlock();

                    ChangeJobCmds(id, JOB_RUN);
                    continue;
                }

                if (status != JOB_QUEUED)
                {

                    if (hostname == "")
                    {
                        message = QString("Resetting '%1' job for "
                                          "chanid %2 @ %3 to %4 status, "
                                          "because no hostname is set.")
                                          .arg(JobText(type))
                                          .arg(chanid).arg(startts)
                                          .arg(StatusText(JOB_QUEUED));
                        VERBOSE(VB_JOBQUEUE, LOC + message);

                        ChangeJobStatus(id, JOB_QUEUED, "");
                        ChangeJobCmds(id, JOB_RUN);
                    }
                    else if (inTimeWindow)
                    {
                        message = QString("Skipping '%1' job for chanid %2 @ "
                                          "%3, current job status is '%4'")
                                          .arg(JobText(type))
                                          .arg(chanid).arg(startts)
                                          .arg(StatusText(status));
                        VERBOSE(VB_JOBQUEUE, LOC + message);
                    }
                    continue;
                }

                // never start or claim more than one job in a single run
                if (startedJobAlready)
                    continue;

                if ((inTimeWindow) &&
                    (hostname == "") &&
                    (!ChangeJobHost(id, m_hostname)))
                {
                    message = QString("Unable to claim '%1' job "
                                      "for chanid %2 @ %3.")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, LOC_ERR + message);
                    continue;
                }

                if (!inTimeWindow)
                {
                    message = QString("Skipping '%1' job for chanid "
                                      "%2 @ %3, Current time is outside of the "
                                      "Job Queue processing window.")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, LOC + message);
                    continue;
                }

                message = QString("Processing '%1' job for chanid "
                                  "%2 @ %3, current status is '%4'")
                                  .arg(JobText(type))
                                  .arg(chanid).arg(startts)
                                  .arg(StatusText(status));
                VERBOSE(VB_JOBQUEUE, LOC + message);

                ProcessJob(id, type, chanid, starttime);

                startedJobAlready = true;
            }
        }

        if (startedJobAlready)
            sleep(5);
        else
            sleep(sleepTime);
    }
}

bool JobQueue::QueueRecordingJobs(ProgramInfo *pinfo, int jobTypes)
{
    if (!pinfo)
        return false;

    if (jobTypes == JOB_NONE)
        jobTypes = pinfo->GetAutoRunJobs();

    if (pinfo->chancommfree)
        jobTypes &= (~JOB_COMMFLAG);

    if (jobTypes != JOB_NONE)
    {
        QString jobHost = "";

        if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
            jobHost = pinfo->hostname;

        return JobQueue::QueueJobs(jobTypes, pinfo->chanid, pinfo->recstartts,
                                   "", "", jobHost);
    }
    else
        return false;

    return true;
}

bool JobQueue::QueueJob(int jobType, QString chanid, QDateTime starttime, 
                        QString args, QString comment, QString host,
                        int flags, int status)
{
    int tmpStatus = JOB_UNKNOWN;
    int tmpCmd = JOB_UNKNOWN;
    int jobID = -1;
    
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT status, id, cmds FROM jobqueue "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND type = :JOBTYPE;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":JOBTYPE", jobType);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::QueueJob()", query);
        return false;
    }
    else
    {
        if ((query.size() > 0) && query.next())
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

    query.prepare("INSERT INTO jobqueue (chanid, starttime, inserttime, type, "
                  "status, statustime, hostname, args, comment, flags) "
                  "VALUES (:CHANID, :STARTTIME, now(), :JOBTYPE, :STATUS, "
                  "now(), :HOST, :ARGS, :COMMENT, :FLAGS);");

    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":JOBTYPE", jobType);
    query.bindValue(":STATUS", status);
    query.bindValue(":HOST", host);
    query.bindValue(":ARGS", args);
    query.bindValue(":COMMENT", comment);
    query.bindValue(":FLAGS", flags);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::StartJob()", query);
        return false;
    }

    return true;
}

bool JobQueue::QueueJobs(int jobTypes, QString chanid, QDateTime starttime, 
                         QString args, QString comment, QString host)
{
    if (gContext->GetNumSetting("AutoTranscodeBeforeAutoCommflag", 0))
    {
        if (jobTypes & JOB_TRANSCODE)
            QueueJob(JOB_TRANSCODE, chanid, starttime, args, comment, host);
        if (jobTypes & JOB_COMMFLAG)
            QueueJob(JOB_COMMFLAG, chanid, starttime, args, comment, host);
    }
    else
    {
        if (jobTypes & JOB_COMMFLAG)
            QueueJob(JOB_COMMFLAG, chanid, starttime, args, comment, host);
        if (jobTypes & JOB_TRANSCODE)
            QueueJob(JOB_TRANSCODE, chanid, starttime, args, comment, host);
    }

    if (jobTypes & JOB_USERJOB1)
        QueueJob(JOB_USERJOB1, chanid, starttime, args, comment, host);
    if (jobTypes & JOB_USERJOB2)
        QueueJob(JOB_USERJOB2, chanid, starttime, args, comment, host);
    if (jobTypes & JOB_USERJOB3)
        QueueJob(JOB_USERJOB3, chanid, starttime, args, comment, host);
    if (jobTypes & JOB_USERJOB4)
        QueueJob(JOB_USERJOB4, chanid, starttime, args, comment, host);

    return true;
}

int JobQueue::GetJobID(int jobType, QString chanid, QDateTime starttime)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT id FROM jobqueue "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND type = :JOBTYPE;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":JOBTYPE", jobType);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobID()", query);
        return -1;
    }
    else
    {
        if ((query.size() > 0) && query.next())
            return query.value(0).toInt();
    }

    return -1;
}

bool JobQueue::GetJobInfoFromID(int jobID, int &jobType, QString &chanid, 
                                QDateTime &starttime)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT type, chanid, starttime FROM jobqueue "
                  "WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    query.exec();
    
    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobID()", query);
        return false;
    }
    else
    {
        if ((query.size() > 0) && query.next())
        {
            jobType = query.value(0).toInt();
            chanid = query.value(1).toString();
            starttime = query.value(2).toDateTime();
            return true;
        }
    }

    return false;
}

bool JobQueue::GetJobInfoFromID(int jobID, int &jobType, QString &chanid, 
                                QString &starttime)
{
    QDateTime tmpStarttime;
    bool result = JobQueue::GetJobInfoFromID(jobID, jobType, chanid,
                                             tmpStarttime);
    if (result)
        starttime = tmpStarttime.toString("yyyyMMddhhmmss");

    return result;
}

bool JobQueue::PauseJob(int jobID)
{
    QString message = QString("GLOBAL_JOB PAUSE ID %1").arg(jobID);
    MythEvent me(message);
    gContext->dispatch(me);

    return ChangeJobCmds(jobID, JOB_PAUSE);
}

bool JobQueue::ResumeJob(int jobID)
{
    QString message = QString("GLOBAL_JOB RESUME ID %1").arg(jobID);
    MythEvent me(message);
    gContext->dispatch(me);

    return ChangeJobCmds(jobID, JOB_RUN);
}

bool JobQueue::RestartJob(int jobID)
{
    QString message = QString("GLOBAL_JOB RESTART ID %1").arg(jobID);
    MythEvent me(message);
    gContext->dispatch(me);

    return ChangeJobCmds(jobID, JOB_RESTART);
}

bool JobQueue::StopJob(int jobID)
{
    QString message = QString("GLOBAL_JOB STOP ID %1").arg(jobID);
    MythEvent me(message);
    gContext->dispatch(me);

    return ChangeJobCmds(jobID, JOB_STOP);
}

bool JobQueue::DeleteAllJobs(QString chanid, QDateTime starttime)
{
    QString key = GetJobQueueKey(chanid, starttime);
    MSqlQuery query(MSqlQuery::InitCon());
    QString message;

    query.prepare("UPDATE jobqueue SET status = :CANCELLED "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND status = :QUEUED;");

    query.bindValue(":CANCELLED", JOB_CANCELLED);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":QUEUED", JOB_QUEUED);

    query.exec();

    if (!query.isActive())
        MythContext::DBError("Cancel Pending Jobs", query);

    query.prepare("UPDATE jobqueue SET cmds = :CMD "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND status <> :CANCELLED;");
    query.bindValue(":CMD", JOB_STOP);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":CANCELLED", JOB_CANCELLED);

    if (!query.exec())
    {
        MythContext::DBError("Stop Unfinished Jobs", query);
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
        query.bindValue(":STARTTIME", starttime);
        query.bindValue(":FINISHED", JOB_FINISHED);
        query.bindValue(":ABORTED", JOB_ABORTED);
        query.bindValue(":ERRORED", JOB_ERRORED);
        query.bindValue(":CANCELLED", JOB_CANCELLED);

        query.exec();

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("Stop Unfinished Jobs", query);
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
                              .arg(chanid).arg(starttime.toString());
            VERBOSE(VB_JOBQUEUE, LOC + message);
        }

        sleep(1);
        totalSlept++;
    }

    if (totalSlept <= maxSleep)
    {
        query.prepare("DELETE FROM jobqueue "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", starttime);

        query.exec();

        if (!query.isActive())
            MythContext::DBError("Delete All Jobs", query);
    }
    else
    {
        query.prepare("SELECT id, type, status, comment FROM jobqueue "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                      "AND status <> :CANCELLED ORDER BY id;");

        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", starttime);
        query.bindValue(":CANCELLED", JOB_CANCELLED);

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("Error in JobQueue::DeleteAllJobs(), Unable "
                                 "to query list of Jobs left in Queue.", query);
            return 0;
        }

        VERBOSE(VB_IMPORTANT, LOC_ERR + 
                QString( "In DeleteAllJobs: There are Jobs "
                         "left in the JobQueue that are still running for "
                         "chanid %1 @ %2.").arg(chanid)
                         .arg(starttime.toString()));

        if (query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        QString("Job ID %1: '%2' with status '%3' and "
                                "comment '%4'")
                                .arg(query.value(0).toInt())
                                .arg(JobText(query.value(1).toInt()))
                                .arg(StatusText(query.value(2).toInt()))
                                .arg(query.value(3).toString()));
            }
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

    query.exec();
    
    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::DeleteJob()", query);
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

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::ChangeJobCmds()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobCmds(int jobType, QString chanid,
                             QDateTime starttime,  int newCmds)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET cmds = :CMDS WHERE type = :TYPE "
                  "AND chanid = :CHANID AND starttime = :STARTTIME;");

    query.bindValue(":CMDS", newCmds);
    query.bindValue(":TYPE", jobType);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::ChangeJobCmds()", query);
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

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::ChangeJobFlags()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobStatus(int jobID, int newStatus, QString comment)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET status = :STATUS, comment = :COMMENT "
                  "WHERE id = :ID;");

    query.bindValue(":STATUS", newStatus);
    query.bindValue(":COMMENT", comment);
    query.bindValue(":ID", jobID);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::ChangeJobStatus()", query);
        return false;
    }

    return true;
}

bool JobQueue::ChangeJobComment(int jobID, QString comment)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE jobqueue SET comment = :COMMENT "
                  "WHERE id = :ID;");

    query.bindValue(":COMMENT", comment);
    query.bindValue(":ID", jobID);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::ChangeJobComment()", query);
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

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::ChangeJobArgs()", query);
        return false;
    }

    return true;
}

bool JobQueue::IsJobRunning(int jobType, QString chanid, QDateTime starttime)
{
    int tmpStatus = GetJobStatus(jobType, chanid, starttime);

    if ((tmpStatus != JOB_UNKNOWN) && (tmpStatus != JOB_QUEUED) &&
        (!(tmpStatus & JOB_DONE)))
        return true;

    return false;
}

bool JobQueue::IsJobRunning(int jobType, ProgramInfo *pginfo)
{
    return JobQueue::IsJobRunning(jobType, pginfo->chanid, pginfo->recstartts);
}

bool JobQueue::IsJobQueuedOrRunning(int jobType, QString chanid,
                                    QDateTime starttime)
{
    int tmpStatus = GetJobStatus(jobType, chanid, starttime);

    if ((tmpStatus != JOB_UNKNOWN) && (!(tmpStatus & JOB_DONE)))
        return true;

    return false;
}

bool JobQueue::IsJobQueued(int jobType, QString chanid,
                            QDateTime starttime)
{
    int tmpStatus = GetJobStatus(jobType, chanid, starttime);

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
    }

    if (jobType & JOB_USERJOB)
    {
        QString settingName =
            QString("UserJobDesc%1").arg(UserJobTypeToIndex(jobType));
        return gContext->GetSetting(settingName, settingName);
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

QString JobQueue::GetJobQueueKey(QString chanid, QString startts)
{
    return QString("%1_%2").arg(chanid).arg(startts);
}

QString JobQueue::GetJobQueueKey(QString chanid, QDateTime starttime)
{
    return JobQueue::GetJobQueueKey(chanid,
                                    starttime.toString("yyyyMMddhhmmss"));
}

QString JobQueue::GetJobQueueKey(ProgramInfo *pginfo)
{
    return JobQueue::GetJobQueueKey(pginfo->chanid, pginfo->recstartts);
}

int JobQueue::GetJobsInQueue(QMap<int, JobQueueEntry> &jobs, int findJobs)
{
    JobQueueEntry thisJob;
    MSqlQuery query(MSqlQuery::InitCon());
    QDateTime recentDate = QDateTime::currentDateTime().addSecs(-4 * 3600);
    int jobCount = 0;
    bool commflagWhileRecording =
             gContext->GetNumSetting("AutoCommflagWhileRecording", 0);

    jobs.clear();

    query.prepare("SELECT j.id, j.chanid, j.starttime, j.inserttime, j.type, "
                      "j.cmds, j.flags, j.status, j.statustime, j.hostname, "
                      "j.args, j.comment, r.endtime "
                  "FROM jobqueue j, recorded r "
                  "WHERE j.chanid = r.chanid AND j.starttime = r.starttime "
                  "ORDER BY j.inserttime, j.chanid, j.id;");

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobs(), Unable to "
                             "query list of Jobs in Queue.", query);
        return 0;
    }

    VERBOSE(VB_JOBQUEUE, LOC +
            QString("GetJobsInQueue: findJobs search bitmask %1, "
                    "found %2 total jobs")
                    .arg(findJobs).arg(query.numRowsAffected()));
                         

    if (query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            bool wantThisJob = false;

            thisJob.chanid = query.value(1).toString();
            thisJob.starttime = query.value(2).toDateTime();
            thisJob.type = query.value(4).toInt();
            thisJob.status = query.value(7).toInt();
            thisJob.statustime = query.value(8).toDateTime();
            thisJob.startts = thisJob.starttime.toString("yyyyMMddhhmmss");

            if ((query.value(12).toDateTime() > QDateTime::currentDateTime()) &&
                ((!commflagWhileRecording) ||
                 (thisJob.type != JOB_COMMFLAG)))
            {
                VERBOSE(VB_JOBQUEUE, LOC +
                        QString("GetJobsInQueue: Ignoring '%1' Job "
                                "for %2 @ %3 in %4 state.  Endtime in future.")
                                .arg(JobText(thisJob.type))
                                .arg(thisJob.chanid)
                                .arg(thisJob.startts)
                                .arg(StatusText(thisJob.status)));
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
                VERBOSE(VB_JOBQUEUE, LOC +
                        QString("GetJobsInQueue: Ignore '%1' Job for "
                                "%2 @ %3 in %4 state.")
                                .arg(JobText(thisJob.type))
                                .arg(thisJob.chanid)
                                .arg(thisJob.startts)
                                .arg(StatusText(thisJob.status)));
                continue;
            }

            VERBOSE(VB_JOBQUEUE, LOC +
                    QString("GetJobsInQueue: Found '%1' Job for "
                            "%2 @ %3 in %4 state.")
                            .arg(JobText(thisJob.type))
                            .arg(thisJob.chanid)
                            .arg(thisJob.startts)
                            .arg(StatusText(thisJob.status)));

            thisJob.id = query.value(0).toInt();
            thisJob.inserttime = query.value(3).toDateTime();
            thisJob.cmds = query.value(5).toInt();
            thisJob.flags = query.value(6).toInt();
            thisJob.hostname = query.value(9).toString();
            thisJob.args = query.value(10).toString();
            thisJob.comment = query.value(11).toString();

            if ((thisJob.type & JOB_USERJOB) &&
                (UserJobTypeToIndex(thisJob.type) == 0))
            {
                thisJob.type = JOB_NONE;
                VERBOSE(VB_JOBQUEUE, LOC +
                    QString("GetJobsInQueue: Unknown Job Type: %1")
                        .arg(thisJob.type)); 
            }

            if (thisJob.type != JOB_NONE)
                jobs[jobCount++] = thisJob;
        }
    }

    return jobCount;
}

bool JobQueue::ChangeJobHost(int jobID, QString newHostname)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (newHostname != "")
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

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError(QString("Error in JobQueue::ChangeJobHost(), "
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

    if ((job.hostname != "") &&
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
            default:             return false;
        }
    }

    if (gContext->GetNumSetting(allowSetting, 1))
        return true;

    return false;
}

int JobQueue::GetJobCmd(int jobID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cmds FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobCmd()", query);
        return false;
    }
    else
    {
        if ((query.size() > 0) && query.next())
            return query.value(0).toInt();
    }

    return JOB_UNKNOWN;
}

QString JobQueue::GetJobArgs(int jobID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT args FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobArgs()", query);
        return false;
    }
    else
    {
        if ((query.numRowsAffected() > 0) && query.next())
            return query.value(0).toString();
    }

    return QString("");
}

int JobQueue::GetJobFlags(int jobID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT flags FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobFlags()", query);
        return false;
    }
    else
    {
        if ((query.size() > 0) && query.next())
            return query.value(0).toInt();
    }

    return JOB_UNKNOWN;
}

int JobQueue::GetJobStatus(int jobID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT status FROM jobqueue WHERE id = :ID;");

    query.bindValue(":ID", jobID);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobStatus()", query);
        return false;
    }
    else
    {
        if ((query.size() > 0) && query.next())
            return query.value(0).toInt();
    }

    return JOB_UNKNOWN;
}

int JobQueue::GetJobStatus(int jobType, QString chanid, QDateTime startts)
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("SELECT status FROM jobqueue WHERE type = :TYPE "
                  "AND chanid = :CHANID AND starttime = :STARTTIME;");

    query.bindValue(":TYPE", jobType);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", startts);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobStatus()", query);
        return false;
    }
    if (query.size() > 0 && query.next())
    {
        int tmpStatus;
        tmpStatus = query.value(0).toInt();
        return tmpStatus;
    }
    return JOB_UNKNOWN;
}

void JobQueue::RecoverQueue(bool justOld)
{
    QMap<int, JobQueueEntry> jobs;
    QString msg;

    msg = QString("RecoverQueue: Checking for unfinished jobs to "
                  "recover.");
    VERBOSE(VB_JOBQUEUE, LOC + msg);

    GetJobsInQueue(jobs);
        
    if (jobs.size())
    {
        QMap<int, JobQueueEntry>::Iterator it;
        QDateTime oldDate = QDateTime::currentDateTime().addDays(-1);
        QString hostname = gContext->GetHostName();
        int tmpStatus;
        int tmpCmds;

        for (it = jobs.begin(); it != jobs.end(); ++it)
        {
            tmpCmds = it.data().cmds;
            tmpStatus = it.data().status;

            if (((tmpStatus == JOB_STARTING) ||
                 (tmpStatus == JOB_RUNNING) ||
                 (tmpStatus == JOB_PAUSED) ||
                 (tmpCmds & JOB_STOP) ||
                 (tmpStatus == JOB_STOPPING)) &&
                (((!justOld) &&
                  (it.data().hostname == hostname)) ||
                 (it.data().statustime < oldDate)))
            {
                msg = QString("RecoverQueue: Recovering '%1' %2 @ %3 "
                              "from '%4' state.")
                              .arg(JobText(it.data().type))
                              .arg(it.data().chanid)
                              .arg(it.data().startts)
                              .arg(StatusText(it.data().status));
                VERBOSE(VB_JOBQUEUE, LOC + msg);
                        
                ChangeJobStatus(it.data().id, JOB_QUEUED, "");
                ChangeJobCmds(it.data().id, JOB_RUN);
                if (!gContext->GetNumSetting("JobsRunOnRecordHost", 0))
                    ChangeJobHost(it.data().id, "");
            }
            else
            {
                msg = QString("RecoverQueue: Ignoring '%1' %2 @ %3 "
                              "in '%4' state.")
                              .arg(JobText(it.data().type))
                              .arg(it.data().chanid)
                              .arg(it.data().startts)
                              .arg(StatusText(it.data().status));
                        
                // VERBOSE(VB_JOBQUEUE, LOC + msg);
            }
        }
    }
}

void JobQueue::CleanupOldJobsInQueue()
{
    MSqlQuery delquery(MSqlQuery::InitCon());
    QDateTime donePurgeDate = QDateTime::currentDateTime().addDays(-2);
    QDateTime errorsPurgeDate = QDateTime::currentDateTime().addDays(-4);

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

    if (!delquery.exec() || !delquery.isActive())
    {
        MythContext::DBError("JobQueue::CleanupOldJobsInQueue: Error deleting "
                             "old finished jobs.", delquery);
    }
}

void JobQueue::ProcessJob(int id, int jobType, QString chanid,
                          QDateTime starttime)
{
    QString name = QString("jobqueue%1%2").arg(id).arg(rand());

    QString key = GetJobQueueKey(chanid, starttime);

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_JOBQUEUE, LOC_ERR +
                "ProcessJob(): Unable to open database connection");
        return;
    }

    ChangeJobStatus(id, JOB_PENDING);
    ProgramInfo *pginfo = ProgramInfo::GetProgramFromRecorded(chanid,
                                                              starttime);

    if (!pginfo)
    {
        QString message = QString("Unable to retrieve "
                                  "program info for chanid %1, starttime %2")
                                  .arg(chanid).arg(starttime.toString());
        VERBOSE(VB_JOBQUEUE, LOC_ERR + message);

        ChangeJobStatus(id, JOB_ERRORED,
                        "Unable to retrieve Program Info from database");

        return;
    }

    controlFlagsLock.lock();
    
    ChangeJobStatus(id, JOB_STARTING);
    runningJobTypes[key] = jobType;
    runningJobIDs[key] = id;
    runningJobDescs[key] = GetJobDescription(jobType);
    runningJobCommands[key] = GetJobCommand(id, jobType, pginfo);

    if ((jobType == JOB_TRANSCODE) ||
        (runningJobCommands[key] == "mythtranscode"))
    {
        StartChildJob(TranscodeThread, pginfo);
    }
    else if ((jobType == JOB_COMMFLAG) ||
             (runningJobCommands[key] == "mythcommflag"))
    {
        StartChildJob(FlagCommercialsThread, pginfo);
    }
    else if (jobType & JOB_USERJOB)
    {
        StartChildJob(UserJobThread, pginfo);
    }
    else
    {
        ChangeJobStatus(id, JOB_ERRORED,
                        "UNKNOWN JobType, unable to process!");
        runningJobTypes.erase(key);
        runningJobIDs.erase(key);
        runningJobDescs.erase(key);
        runningJobCommands.erase(key);
    }

    controlFlagsLock.unlock();
}

void JobQueue::StartChildJob(void *(*ChildThreadRoutine)(void *),
                             ProgramInfo *tmpInfo)
{
    m_pginfo = tmpInfo;
    childThreadStarted = false;

    pthread_t childThread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&childThread, &attr, ChildThreadRoutine, this);

    while (!childThreadStarted)
        usleep(50);

    delete m_pginfo;
    m_pginfo = NULL;
}

QString JobQueue::GetJobDescription(int jobType)
{
    if (jobType == JOB_TRANSCODE)
        return "Transcode";
    else if (jobType == JOB_COMMFLAG)
        return "Commercial Flagging";
    else if (!(jobType & JOB_USERJOB))
        return "Unknown Job";

    QString descSetting =
        QString("UserJobDesc%1").arg(UserJobTypeToIndex(jobType));

    return gContext->GetSetting(descSetting, "Unknown Job");
}

QString JobQueue::GetJobCommand(int id, int jobType, ProgramInfo *tmpInfo)
{
    QString command = "";
    MSqlQuery query(MSqlQuery::InitCon());

    if (jobType == JOB_TRANSCODE)
    {
        command = gContext->GetSetting("JobQueueTranscodeCommand");
        if (command.stripWhiteSpace().isEmpty())
            command = "mythtranscode";
            
        if (command == "mythtranscode")
            return command;
    }
    else if (jobType == JOB_COMMFLAG)
    {
        command = gContext->GetSetting("JobQueueCommFlagCommand");
        if (command.stripWhiteSpace().isEmpty())
            command = "mythcommflag";
            
        if (command == "mythcommflag")
            return command;
    }
    else if (jobType & JOB_USERJOB)
    {
        command = gContext->GetSetting(
                    QString("UserJob%1").arg(UserJobTypeToIndex(jobType)), "");
    }

    if (command != "")
    {
        command.replace(QRegExp("%DIR%"), gContext->GetFilePrefix());
        command.replace(QRegExp("%FILE%"), tmpInfo->GetRecordBasename());
        command.replace(QRegExp("%TITLE%"), tmpInfo->title);
        command.replace(QRegExp("%SUBTITLE%"), tmpInfo->subtitle);
        command.replace(QRegExp("%DESCRIPTION%"), tmpInfo->description);
        command.replace(QRegExp("%HOSTNAME%"), tmpInfo->hostname);
        command.replace(QRegExp("%CATEGORY%"), tmpInfo->category);
        command.replace(QRegExp("%RECGROUP%"), tmpInfo->recgroup);
        command.replace(QRegExp("%PLAYGROUP%"), tmpInfo->playgroup);
        command.replace(QRegExp("%CHANID%"), tmpInfo->chanid);
        command.replace(QRegExp("%STARTTIME%"),
                        tmpInfo->recstartts.toString("yyyyMMddhhmmss"));
        command.replace(QRegExp("%ENDTIME%"),
                        tmpInfo->recendts.toString("yyyyMMddhhmmss"));
        command.replace(QRegExp("%STARTTIMEISO%"),
                        tmpInfo->recstartts.toString(Qt::ISODate));
        command.replace(QRegExp("%ENDTIMEISO%"),
                        tmpInfo->recendts.toString(Qt::ISODate));
        command.replace(QRegExp("%PROGSTART%"),
                        tmpInfo->startts.toString("yyyyMMddhhmmss"));
        command.replace(QRegExp("%PROGEND%"),
                        tmpInfo->endts.toString("yyyyMMddhhmmss"));
        command.replace(QRegExp("%PROGSTARTISO%"),
                        tmpInfo->startts.toString(Qt::ISODate));
        command.replace(QRegExp("%PROGENDISO%"),
                        tmpInfo->endts.toString(Qt::ISODate));
        command.replace(QRegExp("%VERBOSELEVEL%"),
                        QString("%1").arg(print_verbose_messages));
        command.replace(QRegExp("%JOBID%"), QString("%1").arg(id));

        QString transProf;
        if (tmpInfo->transcoder == RecordingProfile::TranscoderAutodetect)
            transProf = "autodetect";
        else
            transProf = QString::number(tmpInfo->transcoder);
        command.replace(QRegExp("%TRANSPROFILE%"), transProf);
    }

    return command;
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
    JobQueue *theTranscoder = (JobQueue *)param;
    theTranscoder->DoTranscodeThread();

    return NULL;
}

void JobQueue::DoTranscodeThread(void)
{
    if (!m_pginfo)
        return;

    ProgramInfo *program_info = new ProgramInfo(*m_pginfo);
    int controlTranscoding = JOB_RUN;
    QString subtitle = program_info->subtitle.isEmpty() ? "" :
                           QString(" \"%1\"").arg(program_info->subtitle);
    
    QString key = GetJobQueueKey(program_info);
    int jobID = runningJobIDs[key];

    childThreadStarted = true;

    ChangeJobStatus(jobID, JOB_RUNNING);
    bool hasCutlist = !!(program_info->getProgramFlags() & FL_CUTLIST);
    bool useCutlist = !!(GetJobFlags(jobID) & JOB_USE_CUTLIST) && hasCutlist;

    controlFlagsLock.lock();
    jobControlFlags[key] = &controlTranscoding;
    controlFlagsLock.unlock();

    int transcoder = program_info->transcoder;
    QString profilearg = transcoder == RecordingProfile::TranscoderAutodetect ?
                            "autodetect" :
                            QString::number(transcoder);

    QString path;
    QString command;

    if (runningJobCommands[key] == "mythtranscode")
    {
        path = gContext->GetInstallPrefix() + "/bin/mythtranscode";
        command = QString("%1 -j %2 -V %3 -p %4 %5")
                          .arg(path).arg(jobID).arg(print_verbose_messages)
                          .arg(profilearg.ascii()).arg(useCutlist ? "-l" : "");
    }
    else
    {
        command = runningJobCommands[key];

        QStringList tokens = QStringList::split(" ", command);
        path = tokens[0];
    }

    if (jobQueueCPU < 2)
        nice(17);

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
        query.exec();
        if (query.isActive() && query.size() > 0 && query.next())
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
        off_t origfilesize, filesize;
        struct stat st;

        retry = false;

        ChangeJobStatus(jobID, JOB_STARTING);
        program_info->SetTranscoded(TRANSCODING_RUNNING);

        QString fileprefix = gContext->GetFilePrefix();
        QString filename = program_info->GetRecordFilename(fileprefix);

        origfilesize = 0;
        filesize = 0;

        if (stat(filename.ascii(), &st) == 0)
            origfilesize = st.st_size;

        QString msg = QString("Transcode %1")
                              .arg(StatusText(GetJobStatus(jobID)));

        QString details = QString("%1%2: %3 (%4)")
                            .arg(program_info->title.local8Bit())
                            .arg(subtitle.local8Bit())
                            .arg(PrettyPrint(origfilesize))
                            .arg(transcoderName);

        VERBOSE(VB_GENERAL, LOC + QString("%1 for %2").arg(msg).arg(details));
        gContext->LogEntry("transcode", LP_NOTICE, msg, details);

        VERBOSE(VB_JOBQUEUE, LOC + QString("Running command: '%1'")
                                           .arg(command));

        int result = myth_system(command.ascii());
        int status = GetJobStatus(jobID);
 
        details = "";

        if ((result == MYTHSYSTEM__EXIT__EXECL_ERROR) ||
            (result == MYTHSYSTEM__EXIT__CMD_NOT_FOUND))
        {
            msg = QString("Transcoding failed for %1, %2 does not exist or "
                          "is not executable")
                          .arg(details).arg(path);
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg);

            gContext->LogEntry("transcode", LP_WARNING,
                               "Transcoding Errored", msg);

            ChangeJobStatus(jobID, JOB_ERRORED,
                "ERROR: Unable to find mythtranscode, check backend logs.");
            program_info->SetTranscoded(TRANSCODING_NOT_TRANSCODED);

            retry = false;
            break;
        }
        else if (result == TRANSCODE_EXIT_RESTART && retrylimit > 0)
        {
            VERBOSE(VB_JOBQUEUE, LOC + "Transcode command restarting");
            retry = true;
            retrylimit--;

            program_info->SetTranscoded(TRANSCODING_NOT_TRANSCODED);
        }
        else
        {
            msg = QString("Transcode %1")
                          .arg(StatusText(GetJobStatus(jobID)));
            gContext->LogEntry("transcode", LP_NOTICE, msg, details);

            if (status == JOB_FINISHED)
            {
                retry = false;

                // Clear the pathname to force rechecking the DB in case 
                // mythtranscode renamed the file.
                program_info->pathname = "";
                filename = program_info->GetRecordFilename(fileprefix);

                if (stat(filename.ascii(), &st) == 0)
                    filesize = st.st_size;

                if (filesize != origfilesize)
                {
                    QString comment = QString("%1: %2 => %3")
                                            .arg(transcoderName)
                                            .arg(PrettyPrint(origfilesize))
                                            .arg(PrettyPrint(filesize));
                    ChangeJobStatus(jobID, JOB_FINISHED, comment);

                    details = QString("%1%2")
                                      .arg(program_info->title).arg(subtitle);

                    if (filesize > 0)
                    {
                        program_info->SetFilesize(filesize);

                        details += QString(": %1 (%2)")
                                          .arg(PrettyPrint(filesize))
                                          .arg(transcoderName);
                    }

                    VERBOSE(VB_GENERAL, LOC +
                            QString("%1 for %2%3: %4 => %5 (%6)")
                            .arg(msg).arg(program_info->title).arg(subtitle)
                            .arg(PrettyPrint(origfilesize))
                            .arg(PrettyPrint(filesize)).arg(transcoderName));
                }

                MythEvent me("RECORDING_LIST_CHANGE");
                gContext->dispatch(me);

                program_info->SetTranscoded(TRANSCODING_COMPLETE);
            }
        }
    }

    if (retrylimit == 0)
    {
        VERBOSE(VB_JOBQUEUE, LOC_ERR + "Retry limit exceeded for transcoder, "
                                       "setting job status to errored.");
        ChangeJobStatus(jobID, JOB_ERRORED, "Retry limit exceeded");
    }

    controlFlagsLock.lock();
    runningJobIDs.erase(key);
    runningJobTypes.erase(key);
    runningJobDescs.erase(key);
    runningJobCommands.erase(key);
    controlFlagsLock.unlock();
}

void *JobQueue::FlagCommercialsThread(void *param)
{
    JobQueue *theFlagger = (JobQueue *)param;
    theFlagger->DoFlagCommercialsThread();

    return NULL;
}

void JobQueue::DoFlagCommercialsThread(void)
{
    if (!m_pginfo)
        return;

    ProgramInfo *program_info = new ProgramInfo(*m_pginfo);
    int controlFlagging = JOB_RUN;
    QString subtitle = program_info->subtitle.isEmpty() ? "" :
                           QString(" \"%1\"").arg(program_info->subtitle);
    QString logDesc = QString("%1%2 recorded from channel %3 at %4")
                          .arg(program_info->title.local8Bit())
                          .arg(subtitle.local8Bit())
                          .arg(program_info->chanid)
                          .arg(program_info->recstartts.toString());
    
    QString key = GetJobQueueKey(program_info);
    int jobID = runningJobIDs[key];

    childThreadStarted = true;


    if (!MSqlQuery::testDBConnection())
    {
        QString msg = QString("Commercial Flagging failed.  Could not open "
                              "new database connection for %1. "
                              "Program can not be flagged.")
                              .arg(logDesc);
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg);

        ChangeJobStatus(jobID, JOB_ERRORED,
                        "Could not open new database connection for "
                        "commercial flagger.");

        delete program_info;
        return;
    }

    controlFlagsLock.lock();
    jobControlFlags[key] = &controlFlagging;
    controlFlagsLock.unlock();

    QString msg = "Commercial Flagging Starting";
    VERBOSE(VB_GENERAL, LOC + QString("%1 for %2").arg(msg).arg(logDesc));
    gContext->LogEntry("commflag", LP_NOTICE, msg, logDesc);

    int breaksFound = 0;
    QString path;
    QString command;

    if (runningJobCommands[key] == "mythcommflag")
    {
        path = gContext->GetInstallPrefix() + "/bin/mythcommflag";
        command = QString("%1 -j %2 -V %3")
                          .arg(path).arg(jobID).arg(print_verbose_messages);
    }
    else
    {
        command = runningJobCommands[key];

        QStringList tokens = QStringList::split(" ", command);
        path = tokens[0];
    }

    VERBOSE(VB_JOBQUEUE, LOC + QString("Running command: '%1'").arg(command));

    breaksFound = myth_system(command.ascii());

    controlFlagsLock.lock();
    if ((breaksFound == MYTHSYSTEM__EXIT__EXECL_ERROR) ||
        (breaksFound == MYTHSYSTEM__EXIT__CMD_NOT_FOUND))
    {
        msg = QString("Commercial Flagging failed for %1, %2 does not exist "
                      "or is not executable")
                      .arg(logDesc).arg(path);
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg);

        gContext->LogEntry("commflag", LP_WARNING,
                           "Commercial Flagging Errored", msg);

        ChangeJobStatus(jobID, JOB_ERRORED,
            "ERROR: Unable to find mythcommflag, check backend logs.");
        msg = ""; // reset msg so we don't reprint it later
    }
    else if ((*(jobControlFlags[key]) == JOB_STOP) ||
             (breaksFound >= COMMFLAG_EXIT_START))
    {
        msg = QString("ABORTED Commercial Flagging for %1.")
                      .arg(logDesc);

        gContext->LogEntry("commflag", LP_WARNING,
                           "Commercial Flagging Aborted", msg);

        ChangeJobStatus(jobID, JOB_ABORTED,
                        "Job aborted by user.");
    }
    else if (breaksFound ==  COMMFLAG_EXIT_NO_PROGRAM_DATA)
    {
        msg = QString("ERROR in Commercial Flagging for %1, problem opening "
                      "file or initting decoder, check backend log.")
                      .arg(logDesc);

        gContext->LogEntry("commflag", LP_WARNING,
                           "Commercial Flagging ERRORED", msg);

        ChangeJobStatus(jobID, JOB_ERRORED,
                        "Job ERRORED, unable to open file or init decoder.");
    }
    else if (breaksFound >= COMMFLAG_EXIT_START)
    {
        msg = QString("Commercial Flagging ERRORED for %1 with result %2.")
                      .arg(logDesc).arg(breaksFound);

        gContext->LogEntry("commflag", LP_WARNING,
                           "Commercial Flagging Errored", msg);

        ChangeJobStatus(jobID, JOB_ERRORED,
                        QString("Job aborted with Error %1.").arg(breaksFound));
    }
    else
    {
        msg = "Commercial Flagging Finished";

        QString details = QString("%1: %2 commercial break(s)")
                                .arg(logDesc)
                                .arg(breaksFound);

        gContext->LogEntry("commflag", LP_NOTICE, msg, details);

        msg = QString("Finished, %1 break(s) found.").arg(breaksFound);
        ChangeJobStatus(jobID, JOB_FINISHED, msg);

        VERBOSE(VB_GENERAL, LOC + QString("Commercial Flagging %1").arg(msg));
        msg = "";

        MythEvent me("RECORDING_LIST_CHANGE");
        gContext->dispatch(me);

        const QString prefix   = gContext->GetSetting("RecordFilePrefix");
        program_info->pathname = prefix + "/" + program_info->pathname;
        (new PreviewGenerator(program_info, true))->Run();
    }

    if (msg != "")
        VERBOSE(VB_IMPORTANT, LOC + msg);

    jobControlFlags.erase(key);
    runningJobIDs.erase(key);
    runningJobTypes.erase(key);
    runningJobDescs.erase(key);
    runningJobCommands.erase(key);
    controlFlagsLock.unlock();

    delete program_info;
}

void *JobQueue::UserJobThread(void *param)
{
    JobQueue *theUserJob = (JobQueue *)param;
    theUserJob->DoUserJobThread();

    return NULL;
}

void JobQueue::DoUserJobThread(void)
{
    if (!m_pginfo)
        return;

    ProgramInfo *program_info = new ProgramInfo(*m_pginfo);
    QString key = GetJobQueueKey(program_info);
    int jobID = runningJobIDs[key];
    QString jobDesc = runningJobDescs[key];

    childThreadStarted = true;

    ChangeJobStatus(jobID, JOB_RUNNING);

    QString msg = QString("Started \"%1\" for \"%2\" recorded "
                          "from channel %3 at %4")
                          .arg(jobDesc)
                          .arg(program_info->title.local8Bit())
                          .arg(program_info->chanid)
                          .arg(program_info->recstartts.toString());
    VERBOSE(VB_GENERAL, LOC + msg);
    gContext->LogEntry("jobqueue", LP_NOTICE,
                       QString("Job \"%1\" Started").arg(jobDesc), msg);

    switch (jobQueueCPU)
    {
        case  0: nice(17);
                 break;
        case  1: nice(10);
                 break;
        case  2:
        default: break;
    }

    VERBOSE(VB_JOBQUEUE, LOC + QString("Running command: '%1'")
                                       .arg(runningJobCommands[key]));

    int result = myth_system(runningJobCommands[key].ascii());

    if ((result == MYTHSYSTEM__EXIT__EXECL_ERROR) ||
        (result == MYTHSYSTEM__EXIT__CMD_NOT_FOUND))
    {
        msg = QString("User Job '%1' failed, unable to find "
                      "executable, check your PATH and backend logs.")
                      .arg(runningJobCommands[key]);
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
        VERBOSE(VB_IMPORTANT, LOC + QString("Current PATH: '%1'")
                                            .arg(getenv("PATH")));

        gContext->LogEntry("jobqueue", LP_WARNING,
                           "User Job Errored", msg);

        ChangeJobStatus(jobID, JOB_ERRORED,
            "ERROR: Unable to find executable, check backend logs.");
    }
    else
    {
        msg = QString("Finished \"%1\" for \"%2\" recorded from "
                      "channel %3 at %4.")
                      .arg(jobDesc)
                      .arg(program_info->title.local8Bit())
                      .arg(program_info->chanid)
                      .arg(program_info->recstartts.toString());
        VERBOSE(VB_GENERAL, LOC + msg);

        gContext->LogEntry("jobqueue", LP_NOTICE,
                           QString("Job \"%1\" Finished").arg(jobDesc), msg);

        ChangeJobStatus(jobID, JOB_FINISHED, "Successfully Completed.");

        MythEvent me("RECORDING_LIST_CHANGE");
        gContext->dispatch(me);
    }

    controlFlagsLock.lock();
    runningJobIDs.erase(key);
    runningJobTypes.erase(key);
    runningJobDescs.erase(key);
    runningJobCommands.erase(key);
    controlFlagsLock.unlock();
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
