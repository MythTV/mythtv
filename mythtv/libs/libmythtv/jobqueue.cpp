#include <unistd.h>

#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qdatetime.h>
#include <qfileinfo.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <iostream>
using namespace std;

#include "jobqueue.h"
#include "programinfo.h"
#include "libmyth/mythcontext.h"
#include "NuppelVideoPlayer.h"
#include "mythdbcon.h"

JobQueue::JobQueue(bool master, QSqlDatabase *db)
{
    isMaster = master;
    m_db = db;
    m_hostname = gContext->GetHostName();

    jobQueueCPU = gContext->GetNumSetting("JobQueueCPU", 0);

    jobsRunning = 0;

    if ( !m_db)
        cerr << "m_db == NULL" << endl;

    queuePoll = false;

    pthread_create(&queueThread, NULL, QueueProcesserThread, this);

    while (!queuePoll)
        usleep(50);

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
            QString key;

            if (tokens[2] == "ID")
            {
                dblock.lock();
                jobID = tokens[3].toInt();
                GetJobInfoFromID(m_db, jobID, jobType, chanid, starttime);
                dblock.unlock();
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
                msg = QString("ERROR in Job Queue.  Unable "
                              "to determine job info for message: "
                              "%1.  Program will not be flagged.")
                              .arg(message);
                VERBOSE(VB_GENERAL, msg);
                return;
            }

            key = QString("%1_%2").arg(chanid)
                          .arg(starttime.toString("yyyyMMddhhmm00"));


            msg = QString("Job Queue: Received message '%1'")
                          .arg(message);
            VERBOSE(VB_JOBQUEUE, msg);

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

    dblock.lock();
    RecoverQueue(m_db);
    dblock.unlock();

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
    VERBOSE(VB_JOBQUEUE, "JobQueue::ProcessQueue() started");

    MSqlQuery delquery(QString::null, m_db);
    MSqlQuery query(QString::null, m_db);
    QString chanid;
    QDateTime starttime;
    QString startts;
    int type;
    int id;
    int cmds;
    int flags;
    int status;
    QString hostname;
    int sleepTime = gContext->GetNumSetting("JobQueueCheckFrequency", 30);

    QMap<QString, int> job;
    QMap<QString, int> jobType;
    QMap<QString, int> jobStatus;
    int maxJobs;
    QString message;
    QMap<int, JobQueueEntry> jobs;
    bool atMax = false;


    while (queuePoll)
    {
        maxJobs = gContext->GetNumSetting("JobQueueMaxSimultaneousJobs", 3);

        job.clear();
        jobType.clear();
        jobStatus.clear();

        dblock.lock();

        GetJobsInQueue(m_db, jobs);
        
        if (jobs.size())
        {
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

            message = QString("JobQueue: Currently Running %1 jobs")
                              .arg(jobsRunning);
            if (jobsRunning >= maxJobs)
            {
                message += " (At Maximum, no new jobs can be started until "
                           "a running job completes)";

                if (!atMax)
                    VERBOSE(VB_JOBQUEUE, message);

                atMax = true;
            }
            else
            {
                VERBOSE(VB_JOBQUEUE, message);
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

                QString key = QString("%1_%2").arg(chanid).arg(startts);

                // Check to see if there was a previous job that is not done
                if ((jobStatus.contains(key)) &&
                    (!(jobStatus[key] & JOB_DONE)))
                {
                    message = QString("JobQueue: Skipping '%1' job for chanid "
                                      "%2 @ %3, previous job status is '%4'")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts)
                                      .arg(StatusText(jobStatus[key]));
                    VERBOSE(VB_JOBQUEUE, message);
                    continue;
                }

                job[key] = id;
                jobType[key] = type;
                jobStatus[key] = status;
               
                if ((hostname != "") &&
                    (hostname != m_hostname))
                {
                    message = QString("JobQueue: Skipping '%1' job for chanid "
                                      "%2 @ %3, should run on '%4' instead")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts)
                                      .arg(hostname);
                    VERBOSE(VB_JOBQUEUE, message);
                    continue;
                }

                if (!AllowedToRun(jobs[x]))
                {
                    message = QString("JobQueue: Skipping '%1' job for chanid "
                                      "%2 @ %3, not allowed to run on this "
                                      "backend.")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, message);
                    continue;
                }

                if ((cmds & JOB_STOP) && (status != JOB_QUEUED))
                {
                    message = QString("JobQueue: Stopping '%1' job for chanid "
                                      "%2 @ %3")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, message);

                    controlFlagsLock.lock();
                    if (jobControlFlags.contains(key))
                        *(jobControlFlags[key]) = JOB_STOP;
                    controlFlagsLock.unlock();

                    // ChangeJobCmds(m_db, id, JOB_RUN);
                    continue;
                }

                if ((cmds & JOB_PAUSE) && (status != JOB_QUEUED))
                {
                    message = QString("JobQueue: Pausing '%1' job for chanid "
                                      "%2 @ %3")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, message);

                    controlFlagsLock.lock();
                    if (jobControlFlags.contains(key))
                        *(jobControlFlags[key]) = JOB_PAUSE;
                    controlFlagsLock.unlock();

                    ChangeJobCmds(m_db, id, JOB_RUN);
                    continue;
                }

                if ((cmds & JOB_RESTART) && (status != JOB_QUEUED))
                {
                    message = QString("JobQueue: Restart '%1' job for chanid "
                                      "%2 @ %3")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, message);

                    controlFlagsLock.lock();
                    if (jobControlFlags.contains(key))
                        *(jobControlFlags[key]) = JOB_RUN;
                    controlFlagsLock.unlock();

                    ChangeJobCmds(m_db, id, JOB_RUN);
                    continue;
                }

                if (status != JOB_QUEUED)
                {

                    if (hostname == "")
                    {
                        message = QString("JobQueue: Resetting '%1' job for "
                                          "chanid %2 @ %3 to %4 status, "
                                          "because no hostname is set.")
                                          .arg(JobText(type))
                                          .arg(chanid).arg(startts)
                                          .arg(StatusText(JOB_QUEUED));
                        VERBOSE(VB_JOBQUEUE, message);

                        ChangeJobStatus(m_db, id, JOB_QUEUED, "");
                        ChangeJobCmds(m_db, id, JOB_RUN);
                    }
                    else
                    {
                        message = QString("JobQueue: Skipping '%1' job for "
                                          "chanid %2 @ %3, current job status "
                                          "is '%4'")
                                          .arg(JobText(type))
                                          .arg(chanid).arg(startts)
                                          .arg(StatusText(status));
                        VERBOSE(VB_JOBQUEUE, message);
                    }
                    continue;
                }

                if ((hostname == "") &&
                    (!ClaimJob(m_db, id)))
                {
                    message = QString("JobQueue: ERROR claiming '%1' job "
                                      "for chanid %2 @ %3.")
                                      .arg(JobText(type))
                                      .arg(chanid).arg(startts);
                    VERBOSE(VB_JOBQUEUE, message);
                    continue;
                }

                message = QString("JobQueue: Processing '%1' job for chanid "
                                  "%2 @ %3, current status is '%4'")
                                  .arg(JobText(type))
                                  .arg(chanid).arg(startts)
                                  .arg(StatusText(status));
                VERBOSE(VB_JOBQUEUE, message);

                ProcessJob(id, type, chanid, starttime);

                // never start more than one job at a time
                break;
            }
        }
        else
        {
            //VERBOSE(VB_JOBQUEUE, "JobQueue: No Jobs found to process.");
        }
        dblock.unlock();

        sleep(sleepTime);
    }
}

bool JobQueue::QueueJob(QSqlDatabase* db, int jobType,
                        QString chanid, QDateTime starttime, QString args,
                        QString comment, QString host, int flags)
{
    QString startts = starttime.toString("yyyyMMddhhmm00");

    int tmpStatus = JOB_UNKNOWN;
    int tmpCmd = JOB_UNKNOWN;
    int jobID = -1;
    MSqlQuery query(QString::null, db);

    query.prepare("SELECT status, id, cmds FROM jobqueue "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND type = :JOBTYPE;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", startts);
    query.bindValue(":JOBTYPE", jobType);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::QueueJob()", query);
        return false;
    }
    else
    {
        if ((query.numRowsAffected() > 0) && query.next())
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
                 DeleteJob(db, jobID);
                 break;
    }
    if (! (tmpStatus & JOB_DONE) && (tmpCmd & JOB_STOP))
        return false;
    query.prepare("INSERT jobqueue (chanid, starttime, inserttime, type, "
                         "status, hostname, args, comment, flags) "
                     "VALUES (:CHANID, :STARTTIME, now(), :JOBTYPE, :STATUS, "
                         ":HOST, :ARGS, :COMMENT, :FLAGS);");

    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", startts);
    query.bindValue(":JOBTYPE", jobType);
    query.bindValue(":STATUS", JOB_QUEUED);
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

bool JobQueue::QueueJobs(QSqlDatabase* db, int jobTypes,
                         QString chanid, QDateTime starttime, QString args,
                         QString comment, QString host)
{
    if (jobTypes & JOB_COMMFLAG)
        QueueJob(db, JOB_COMMFLAG, chanid, starttime, args, comment, host);
    if (jobTypes & JOB_TRANSCODE)
        QueueJob(db, JOB_TRANSCODE, chanid, starttime, args, comment, host);
    if (jobTypes & JOB_USERJOB1)
        QueueJob(db, JOB_USERJOB1, chanid, starttime, args, comment, host);
    if (jobTypes & JOB_USERJOB2)
        QueueJob(db, JOB_USERJOB2, chanid, starttime, args, comment, host);
    if (jobTypes & JOB_USERJOB3)
        QueueJob(db, JOB_USERJOB3, chanid, starttime, args, comment, host);
    if (jobTypes & JOB_USERJOB4)
        QueueJob(db, JOB_USERJOB4, chanid, starttime, args, comment, host);

    return true;
}

int JobQueue::GetJobID(QSqlDatabase* db, int jobType, QString chanid,
                       QDateTime starttime)
{
    QString startts = starttime.toString("yyyyMMddhhmm00");
    MSqlQuery query(QString::null, db);

    query.prepare("SELECT id FROM jobqueue "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND type = :JOBTYPE;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", startts);
    query.bindValue(":JOBTYPE", jobType);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobID()", query);
        return -1;
    }
    else
    {
        if ((query.numRowsAffected() > 0) && query.next())
            return query.value(0).toInt();
    }

    return -1;
}

bool JobQueue::GetJobInfoFromID(QSqlDatabase* db, int jobID, int &jobType,
                                QString &chanid, QDateTime &starttime)
{
    MSqlQuery query(QString::null, db);

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
        if ((query.numRowsAffected() > 0) && query.next())
        {
            jobType = query.value(0).toInt();
            chanid = query.value(1).toString();
            starttime = query.value(2).toDateTime();
            return true;
        }
    }

    return false;
}

bool JobQueue::PauseJob(QSqlDatabase* db, int jobID)
{
    QString message = QString("GLOBAL_JOB PAUSE ID %1").arg(jobID);
    MythEvent me(message);
    gContext->dispatch(me);

    return ChangeJobCmds(db, jobID, JOB_PAUSE);
}

bool JobQueue::ResumeJob(QSqlDatabase* db, int jobID)
{
    QString message = QString("GLOBAL_JOB RESUME ID %1").arg(jobID);
    MythEvent me(message);
    gContext->dispatch(me);

    return ChangeJobCmds(db, jobID, JOB_RUN);
}

bool JobQueue::RestartJob(QSqlDatabase* db, int jobID)
{
    QString message = QString("GLOBAL_JOB RESTART ID %1").arg(jobID);
    MythEvent me(message);
    gContext->dispatch(me);

    return ChangeJobCmds(db, jobID, JOB_RESTART);
}

bool JobQueue::StopJob(QSqlDatabase* db, int jobID)
{
    QString message = QString("GLOBAL_JOB STOP ID %1").arg(jobID);
    MythEvent me(message);
    gContext->dispatch(me);

    return ChangeJobCmds(db, jobID, JOB_STOP);
}

bool JobQueue::DeleteAllJobs(QSqlDatabase* db, QString chanid,
                             QDateTime starttime)
{
    QString key = QString("%1_%2").arg(chanid)
                          .arg(starttime.toString("yyyyMMddhhmm00"));
    MSqlQuery query(QString::null, db);
    QString message;

    query.prepare("UPDATE jobqueue SET status = :ABORTED "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND status = :QUEUED;");

    query.bindValue(":ABORTED", JOB_ABORTED);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":QUEUED", JOB_QUEUED);

    query.exec();

    if (!query.isActive())
        MythContext::DBError("Abort Pending Jobs", query);

    query.prepare("SELECT chanid, starttime, type FROM jobqueue "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND status IN (:STARTING,:RUNNING,:PAUSED,:STOPPING);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);
    query.bindValue(":STARTING", JOB_STARTING);
    query.bindValue(":RUNNING", JOB_RUNNING);
    query.bindValue(":PAUSED", JOB_PAUSED);
    query.bindValue(":STOPPING", JOB_STOPPING);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Stop Unfinished Jobs", query);
        return false;
    }

    if (query.numRowsAffected() > 0)
    {
        int jobType;
        QString chanid;
        QDateTime starttime;
        while (query.next())
        {
            chanid = query.value(0).toString();
            starttime = query.value(1).toDateTime();
            jobType = query.value(2).toInt();

            message = QString("GLOBAL_JOB STOP %1 %2 %3")
                              .arg(jobType).arg(chanid)
                              .arg(starttime.toString(Qt::ISODate));
            MythEvent me(message);
            gContext->dispatch(me);
        }
    }

    // wait until running job(s) are done
    bool jobsAreRunning = true;
    int totalSlept = 0;
    while (jobsAreRunning && totalSlept < 20)
    {
        usleep(1000);
        query.prepare("SELECT id FROM jobqueue "
                      "WHERE chanid = :CHANID and starttime = :STARTTIME "
                      "AND status NOT IN (:FINISHED,:ABORTED,:ERRORED);");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", starttime);
        query.bindValue(":FINISHED", JOB_FINISHED);
        query.bindValue(":ABORTED", JOB_ABORTED);
        query.bindValue(":ERRORED", JOB_ERRORED);

        query.exec();

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("Stop Unfinished Jobs", query);
            return false;
        }

        if (query.numRowsAffected() > 0)
        {
            message = QString("JobQueue: Waiting on %1 jobs still running for "
                              "chanid %2 @ %3").arg(query.numRowsAffected())
                              .arg(chanid).arg(starttime.toString());
            VERBOSE(VB_JOBQUEUE, message);
        }
        else
            jobsAreRunning = false;

        sleep(1);
        totalSlept++;
    }

    if (totalSlept < 20)
    {
        query.prepare("DELETE FROM jobqueue "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", starttime);

        query.exec();

        if (!query.isActive())
            MythContext::DBError("Delete All Jobs", query);
    }

    return true;
}

bool JobQueue::DeleteJob(QSqlDatabase* db, int jobID)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(QString::null, db);

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

bool JobQueue::ChangeJobCmds(QSqlDatabase* db, int jobID, int newCmds)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(QString::null, db);

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

bool JobQueue::ChangeJobCmds(QSqlDatabase* db, int jobType, QString chanid,
                             QDateTime starttime,  int newCmds)
{
    MSqlQuery query(QString::null, db);

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

bool JobQueue::ChangeJobFlags(QSqlDatabase* db, int jobID, int newFlags)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(QString::null, db);

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

bool JobQueue::ChangeJobStatus(QSqlDatabase* db, int jobID, int newStatus,
                               QString comment)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(QString::null, db);

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

bool JobQueue::ChangeJobComment(QSqlDatabase* db, int jobID, QString comment)
{
    if (jobID < 0)
        return false;

    MSqlQuery query(QString::null, db);

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

bool JobQueue::IsJobRunning(QSqlDatabase* db, int jobType, QString chanid,
                             QDateTime starttime)
{
    MSqlQuery query(QString::null, db);

    query.prepare("SELECT status,cmds FROM jobqueue WHERE type = :TYPE "
                  "AND chanid = :CHANID AND starttime = :STARTTIME;");

    query.bindValue(":TYPE", jobType);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", starttime);

    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Error in JobQueue::ChangeJobComment()", query);
        return false;
    }
    if (query.numRowsAffected() > 0 && query.next())
    {
        int tmpStatus, tmpCmd;
        tmpStatus = query.value(0).toInt();
        tmpCmd = query.value(1).toInt();
        if (!(tmpStatus & JOB_DONE))
            return true;
    }
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
        QString settingName = QString("UserJobDesc%1").arg(jobType >> 8);
        return gContext->GetSetting(settingName, settingName);
    }

    return tr("Unknown Job");
}

QString JobQueue::StatusText(int status)
{
    switch (status)
    {
        case JOB_QUEUED:   return tr("Queued");
        case JOB_PENDING:  return tr("Pending");
        case JOB_STARTING: return tr("Starting");
        case JOB_RUNNING:  return tr("Running");
        case JOB_PAUSED:   return tr("Paused");
        case JOB_STOPPING: return tr("Stopping");
        case JOB_DONE:     return tr("Done (Invalid status!)");
        case JOB_FINISHED: return tr("Finished");
        case JOB_ABORTED:  return tr("Aborted");
        case JOB_ERRORED:  return tr("Errored");
    }
    return tr("Undefined");
}

int JobQueue::GetJobsInQueue(QSqlDatabase* db, QMap<int, JobQueueEntry> &jobs,
                             int findJobs)
{
    JobQueueEntry thisJob;
    MSqlQuery query(QString::null, db);
    QDateTime recentDate = QDateTime::currentDateTime().addSecs(-4 * 3600);
    int jobCount = 0;

    jobs.clear();

    query.prepare("SELECT j.id, j.chanid, j.starttime, j.inserttime, j.type, "
                      "j.cmds, j.flags, j.status, j.statustime, j.hostname, "
                      "j.args, j.comment "
                  "FROM jobqueue j, recorded r "
                  "WHERE j.chanid = r.chanid AND j.starttime = r.starttime "
                      "AND r.endtime < now() "
                  "ORDER BY j.starttime, j.chanid, j.id;");

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Error in JobQueue::GetJobs(), Unable to "
                             "query list of Jobs in Queue.", query);
        return 0;
    }

    VERBOSE(VB_JOBQUEUE,
            QString("JobQueue::GetJobsInQueue: findJobs search bitmask %1, "
                    "found %2 total jobs")
                    .arg(findJobs).arg(query.numRowsAffected()));
                         

    if (query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            bool wantThisJob = false;

            thisJob.status = query.value(7).toInt();
            thisJob.statustime = query.value(8).toDateTime();
            thisJob.type = query.value(4).toInt();
            thisJob.chanid = query.value(1).toString();
            thisJob.starttime = query.value(2).toDateTime();
            thisJob.startts = thisJob.starttime.toString("yyyyMMddhhmm00");

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
                VERBOSE(VB_JOBQUEUE,
                        QString("JobQueue::GetJobsInQueue: Ignore '%1' Job for "
                                "%2 @ %3 in %4 state.")
                                .arg(JobText(thisJob.type))
                                .arg(thisJob.chanid)
                                .arg(thisJob.startts)
                                .arg(StatusText(thisJob.status)));
                continue;
            }

            VERBOSE(VB_JOBQUEUE,
                    QString("JobQueue::GetJobsInQueue: Found '%1' Job for "
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

            if (thisJob.type & JOB_USERJOB)
            {
                int userJobNumber = 1;
                int jobType = thisJob.type;

                jobType = jobType >> 8;
                while (jobType != 0x01 && userJobNumber < 5)
                {
                    jobType = jobType >> 1;
                    userJobNumber++;
                }

                if (userJobNumber >= 5)
                    thisJob.type = JOB_NONE;
                else
                    thisJob.type = userJobNumber << 8;
            }

            jobs[jobCount++] = thisJob;
        }
    }

    return jobCount;
}

bool JobQueue::ClaimJob(QSqlDatabase* db, int jobID)
{
    MSqlQuery query(QString::null, db);

    query.prepare("UPDATE jobqueue SET hostname = :HOSTNAME "
                  "WHERE hostname = :EMPTY AND id = :ID;");
    query.bindValue(":HOSTNAME", m_hostname);
    query.bindValue(":EMPTY", "");
    query.bindValue(":ID", jobID);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Error in JobQueue::ClaimJob(), Unable to "
                             "set hostname.", query);
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

    if (job.type == JOB_TRANSCODE)
        allowSetting = "JobAllowTranscode";
    else if (job.type == JOB_COMMFLAG)
        allowSetting = "JobAllowCommFlag";
    else if (job.type & JOB_USERJOB)
        allowSetting = QString("JobAllowUserJob%1").arg(job.type >> 8);
    else
        return false;

    if (gContext->GetNumSetting(allowSetting, 1))
        return true;

    return false;
}

int JobQueue::GetJobCmd(QSqlDatabase* db, int jobID)
{
    MSqlQuery query(QString::null, db);

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
        if ((query.numRowsAffected() > 0) && query.next())
            return query.value(0).toInt();
    }

    return JOB_UNKNOWN;
}

int JobQueue::GetJobFlags(QSqlDatabase* db, int jobID)
{
    MSqlQuery query(QString::null, db);

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
        if ((query.numRowsAffected() > 0) && query.next())
            return query.value(0).toInt();
    }

    return JOB_UNKNOWN;
}

int JobQueue::GetJobStatus(QSqlDatabase* db, int jobID)
{
    MSqlQuery query(QString::null, db);

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
        if ((query.numRowsAffected() > 0) && query.next())
            return query.value(0).toInt();
    }

    return JOB_UNKNOWN;
}

void JobQueue::RecoverQueue(QSqlDatabase *db, bool justOld)
{
    QMap<int, JobQueueEntry> jobs;
    QString msg;

    msg = QString("JobQueue::RecoverQueue: Checking for unfinished jobs to "
                  "recover.");
    VERBOSE(VB_JOBQUEUE, msg);

    GetJobsInQueue(db, jobs);
        
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
                msg = QString("JobQueue::RecoverQueue: Recovering '%1' %2 @ %3 "
                              "from '%4' state.")
                              .arg(JobText(it.data().type))
                              .arg(it.data().chanid)
                              .arg(it.data().startts)
                              .arg(StatusText(it.data().status));
                VERBOSE(VB_JOBQUEUE, msg);
                        
                ChangeJobStatus(db, it.data().id, JOB_QUEUED, "");
                ChangeJobCmds(db, it.data().id, JOB_RUN);
            }
            else
            {
                msg = QString("JobQueue::RecoverQueue: Ignoring '%1' %2 @ %3 "
                              "in '%4' state.")
                              .arg(JobText(it.data().type))
                              .arg(it.data().chanid)
                              .arg(it.data().startts)
                              .arg(StatusText(it.data().status));
                        
                // VERBOSE(VB_JOBQUEUE, msg);
            }
        }
    }
}

void JobQueue::CleanupOldJobsInQueue(QSqlDatabase* db)
{
    MSqlQuery delquery(QString::null, db);
    QDateTime donePurgeDate = QDateTime::currentDateTime().addDays(-3);
    QDateTime errorsPurgeDate = QDateTime::currentDateTime().addDays(-7);

    delquery.prepare("DELETE FROM jobqueue "
                     "WHERE (status in (:FINISHED, :ABORTED) "
                     "AND statustime < :DONEPURGEDATE) "
                     "OR (status in (:ERRORED) "
                     "AND statustime < :ERRORSPURGEDATE) ");
    delquery.bindValue(":FINISHED", JOB_FINISHED);
    delquery.bindValue(":ABORTED", JOB_ABORTED);
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
    QString startts = starttime.toString("yyyyMMddhhmm00");
    QString name = QString("jobqueue%1%2").arg(id).arg(rand());

    QString key = QString("%1_%2").arg(chanid)
                          .arg(starttime.toString("yyyyMMddhhmm00"));

    MythSqlDatabase *job_mdb = new MythSqlDatabase(name);
    QSqlDatabase *job_db;
    
    if (!job_mdb)
    {
        VERBOSE(VB_JOBQUEUE, "JobQueue: ERROR: Unable to open database "
                             "connection to process job");
        return;
    }

    job_db = job_mdb->db();

    ChangeJobStatus(job_db, id, JOB_PENDING);
    ProgramInfo *pginfo = ProgramInfo::GetProgramFromRecorded(job_db, chanid,
                                                              starttime);

    if (!pginfo)
    {
        QString message = QString("JobQueue: ERROR, unable to retrieve "
                                  "program info for chanid %1, starttime %2")
                                  .arg(chanid).arg(startts);
        VERBOSE(VB_JOBQUEUE, message);

        ChangeJobStatus(job_db, id, JOB_ERRORED,
                        "Unable to retrieve Program Info from database");

        delete job_mdb;
        return;
    }

    controlFlagsLock.lock();
    
    ChangeJobStatus(job_db, id, JOB_STARTING);
    runningJobTypes[key] = jobType;
    runningJobIDs[key] = id;
    runningJobDescs[key] = GetJobDescription(jobType);
    runningJobCommands[key] = GetJobCommand(job_db, jobType, pginfo);

    if ((jobType == JOB_TRANSCODE) ||
        (runningJobCommands[key] == "transcode"))
    {
        StartChildJob(TranscodeThread, pginfo);
    }
    else if ((jobType == JOB_COMMFLAG) ||
             (runningJobCommands[key] == "commflag"))
    {
        StartChildJob(FlagCommercialsThread, pginfo);
    }
    else if (jobType & JOB_USERJOB)
    {
        StartChildJob(UserJobThread, pginfo);
    }
    else
    {
        ChangeJobStatus(job_db, id, JOB_ERRORED,
                        "UNKNOWN JobType, unable to process!");
        runningJobTypes.erase(key);
        runningJobIDs.erase(key);
        runningJobDescs.erase(key);
        runningJobCommands.erase(key);
    }

    controlFlagsLock.unlock();
    delete job_mdb;
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

    QString descSetting = QString("UserJobDesc%1").arg(jobType >> 8);

    return gContext->GetSetting(descSetting, "Unknown Job");
}

QString JobQueue::GetJobCommand(QSqlDatabase* db, int jobType,
                                    ProgramInfo *tmpInfo)
{
    QString command = "";
    MSqlQuery query(QString::null, db);

    if (jobType == JOB_TRANSCODE)
        return "transcode";
    else if (jobType == JOB_COMMFLAG)
        return "commflag";
    else if (!(jobType & JOB_USERJOB))
        return "";

    QString commandSetting = QString("UserJob%1").arg(jobType >> 8);

    command = gContext->GetSetting(commandSetting, "");

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
        command.replace(QRegExp("%CHANID%"), tmpInfo->chanid);
        command.replace(QRegExp("%STARTTIME%"),
                        tmpInfo->startts.toString("yyyyMMddhhmm00"));
        command.replace(QRegExp("%ENDTIME%"),
                        tmpInfo->endts.toString("yyyyMMddhhmm00"));
        command.replace(QRegExp("%STARTTIMEISO%"),
                        tmpInfo->startts.toString(Qt::ISODate));
        command.replace(QRegExp("%ENDTIMEISO%"),
                        tmpInfo->endts.toString(Qt::ISODate));
    }

    return command;
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
    QString key;
    QString startts = program_info->startts.toString("yyyyMMddhhmm00");
    
    key = QString("%1_%2")
                  .arg(program_info->chanid)
                  .arg(startts);
    int jobID = runningJobIDs[key];

    childThreadStarted = true;

    dblock.lock();
    ChangeJobStatus(m_db, jobID, JOB_RUNNING);
    bool useCutlist = !!(GetJobFlags(m_db, jobID) & JOB_USE_CUTLIST);
    dblock.unlock();

    controlFlagsLock.lock();
    jobControlFlags[key] = &controlTranscoding;
    controlFlagsLock.unlock();

    /////////////////////////////
    // DO TRANSCODE STUFF HERE //
    /////////////////////////////
    QString path = "mythtranscode";
    QString command = QString("%1 -c %2 -s %3 -p %4 -d %5").arg(path.ascii())
                      .arg(program_info->chanid)
                      .arg(program_info->recstartts.toString(Qt::ISODate))
                      .arg("autodetect")
                      .arg(useCutlist ? "-l" : "");

    if (jobQueueCPU < 2)
        nice(17);

    bool retry = true;
    int retrylimit = 3;
    while (retry)
    {
        retry = false;
        dblock.lock();
        ChangeJobStatus(m_db, jobID, JOB_STARTING);
        dblock.unlock();

        myth_system(command.ascii());

        dblock.lock();
        int status = GetJobStatus(m_db, jobID);
        dblock.unlock();

        QString fileprefix = gContext->GetFilePrefix();
        QString filename = program_info->GetRecordFilename(fileprefix);
        if (status == JOB_STOPPING)
        {
            QString tmpfile = filename;
            QString query;
            tmpfile += ".tmp";
            // Get new filesize
            struct stat st;
            long long filesize = 0;
            if (stat(tmpfile.ascii(), &st) == 0)
            filesize = st.st_size;
            // To save the original file...
            QString oldfile = filename;
            oldfile += ".old";
            rename (filename, oldfile);
            rename (tmpfile, filename);
            if (!gContext->GetNumSetting("SaveTranscoding", 0))
                unlink(oldfile);
            dblock.lock();
            if (useCutlist)
            {
                query = QString("DELETE FROM recordedmarkup WHERE "
                                "chanid = '%1' AND starttime = '%2';")
                                .arg(program_info->chanid).arg(startts);
                m_db->exec(query);
                query = QString("UPDATE recorded SET cutlist = NULL, "
                                "bookmark = NULL, "
                                "starttime = starttime WHERE "
                                "chanid = '%1' AND starttime = '%2';")
                                .arg(program_info->chanid).arg(startts);
                m_db->exec(query);
            } else {
                query = QString("DELETE FROM recordedmarkup WHERE "
                                "chanid = '%1' AND starttime = '%2' "
                                "AND type = '%3';")
                                .arg(program_info->chanid).arg(startts)
                                .arg(MARK_KEYFRAME);
                m_db->exec(query);
            }
            if (filesize > 0)
                program_info->SetFilesize(filesize, m_db);
            ChangeJobStatus(m_db, jobID, JOB_FINISHED);
            dblock.unlock();
        } else {
            // transcode didn't finish delete partial transcode
            filename += ".tmp";
            VERBOSE(VB_GENERAL, QString("Deleting %1").arg(filename));
            unlink(filename);
            filename += ".map";
            unlink(filename);
            dblock.lock();
            if (status == JOB_ABORTING) // Stop command was sent
                ChangeJobStatus(m_db, jobID, JOB_ABORTED);
            else if (status != JOB_ERRORING && retrylimit) // Recoverable error
            {
                retry = true;
                retrylimit--;
            } else // Unrecoerable error
                ChangeJobStatus(m_db, jobID, JOB_ERRORED);
            dblock.unlock();
        }
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
    QString key;
    QString logDesc = QString("\"%1\" recorded from channel %2 at %3")
                          .arg(program_info->title.local8Bit())
                          .arg(program_info->chanid)
                          .arg(program_info->startts.toString());
    
    key = QString("%1_%2")
                  .arg(program_info->chanid)
                  .arg(program_info->startts.toString("yyyyMMddhhmm00"));
    int jobID = runningJobIDs[key];

    childThreadStarted = true;

    QString name = QString("commercial%1%2").arg(getpid()).arg(rand());

    MythSqlDatabase *commthread_db = new MythSqlDatabase(name);

    if (!commthread_db || !commthread_db->isOpen())
    {
        QString msg = QString("ERROR in Commercial Flagging.  Could not open "
                              "new database connection for %1. "
                              "Program can not be flagged.")
                              .arg(logDesc);
        VERBOSE(VB_GENERAL, msg);

        dblock.lock();
        ChangeJobStatus(m_db, jobID, JOB_ERRORED,
                        "Could not open new database connection for "
                        "commercial flagger.");
        dblock.unlock();

        delete program_info;
        return;
    }

    controlFlagsLock.lock();
    jobControlFlags[key] = &controlFlagging;
    controlFlagsLock.unlock();

    QString msg = QString("Starting Commercial Flagging for %1.")
                          .arg(logDesc);
    VERBOSE(VB_GENERAL, msg);
    gContext->LogEntry("commflag", LP_NOTICE, "Commercial Flagging Starting",
                       msg);

    int breaksFound = 0;
    QString cmd = QString("mythcommflag -j --chanid %1 --starttime %2 --force")
                         .arg(program_info->chanid)
                         .arg(program_info->startts.toString("yyyyMMddhhmm00"));

    if (print_verbose_messages & (VB_COMMFLAG|VB_JOBQUEUE))
    {
        cmd += " -v ";
        if (print_verbose_messages & VB_COMMFLAG)
        {
            cmd += "commflag";
            if (print_verbose_messages & VB_JOBQUEUE)
                cmd += ",jobqueue";
        }
        else
        {
            cmd += "jobqueue";
        }
    }

    breaksFound = myth_system(cmd.ascii());

    controlFlagsLock.lock();
    if (breaksFound == 127)
    {
        msg = QString("Commercial Flagging ERRORED for %1, unable to find mythcommflag, check your PATH.")
                      .arg(logDesc);

        gContext->LogEntry("commflag", LP_WARNING,
                           "Commercial Flagging Errored", msg);

        ChangeJobStatus(commthread_db->db(), jobID, JOB_ERRORED,
                        "ERROR: Unable to find mythcommflag.");
    }
    else if ((*(jobControlFlags[key]) == JOB_STOP) ||
             (breaksFound == 255))
    {
        msg = QString("ABORTED Commercial Flagging for %1.")
                      .arg(logDesc);

        gContext->LogEntry("commflag", LP_WARNING,
                           "Commercial Flagging Aborted", msg);

        ChangeJobStatus(commthread_db->db(), jobID, JOB_ABORTED,
                        "Job aborted by user.");
    }
    else if (breaksFound == 254)
    {
        msg = QString("ERROR in Commercial Flagging for %1, problem opening "
                      "file or initting decoder, check backend log.")
                      .arg(logDesc);

        gContext->LogEntry("commflag", LP_WARNING,
                           "Commercial Flagging ERRORED", msg);

        ChangeJobStatus(commthread_db->db(), jobID, JOB_ERRORED,
                        "Job ERRORED, unable to open file or init decoder.");
    }
    else if (breaksFound >= 200)
    {
        msg = QString("Commercial Flagging ERRORED for %1 with result %2.")
                      .arg(logDesc).arg(breaksFound);

        gContext->LogEntry("commflag", LP_WARNING,
                           "Commercial Flagging Errored", msg);

        ChangeJobStatus(commthread_db->db(), jobID, JOB_ERRORED,
                        QString("Job aborted with Error %1.").arg(breaksFound));
    }
    else
    {
        msg = QString("Finished Commercial Flagging for %1. "
                      "Found %2 commercial break(s).")
                      .arg(logDesc)
                      .arg(breaksFound);

        gContext->LogEntry("commflag", LP_NOTICE,
                           "Commercial Flagging Finished", msg);

        msg = QString("Finished, %1 break(s) found.").arg(breaksFound);
        ChangeJobStatus(commthread_db->db(), jobID, JOB_FINISHED, msg);
    }
    VERBOSE(VB_GENERAL, msg);

    jobControlFlags.erase(key);
    runningJobIDs.erase(key);
    runningJobTypes.erase(key);
    runningJobDescs.erase(key);
    runningJobCommands.erase(key);
    controlFlagsLock.unlock();

    delete program_info;
    delete commthread_db;
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
    QString key;
    
    key = QString("%1_%2")
                  .arg(program_info->chanid)
                  .arg(program_info->startts.toString("yyyyMMddhhmm00"));
    int jobID = runningJobIDs[key];
    QString jobDesc = runningJobDescs[key];

    childThreadStarted = true;

    dblock.lock();
    ChangeJobStatus(m_db, jobID, JOB_RUNNING);
    dblock.unlock();

    QString msg = QString("Started \"%1\" for \"%2\" recorded "
                          "from channel %3 at %4")
                          .arg(jobDesc)
                          .arg(program_info->title.local8Bit())
                          .arg(program_info->chanid)
                          .arg(program_info->startts.toString());
    VERBOSE(VB_JOBQUEUE, msg);
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

    myth_system(runningJobCommands[key].ascii());

    msg = QString("Finished \"%1\" for \"%2\" recorded from "
                  "channel %3 at %4.")
                  .arg(jobDesc)
                  .arg(program_info->title.local8Bit())
                  .arg(program_info->chanid)
                  .arg(program_info->startts.toString());
    VERBOSE(VB_JOBQUEUE, msg);

    gContext->LogEntry("jobqueue", LP_NOTICE,
                       QString("Job \"%1\" Finished").arg(jobDesc), msg);

    dblock.lock();
    ChangeJobStatus(m_db, jobID, JOB_FINISHED, "Successfully Completed.");
    dblock.unlock();

    controlFlagsLock.lock();
    runningJobIDs.erase(key);
    runningJobTypes.erase(key);
    runningJobDescs.erase(key);
    runningJobCommands.erase(key);
    controlFlagsLock.unlock();
}

