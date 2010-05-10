#ifndef JOBQUEUE_H_
#define JOBQUEUE_H_

#include <pthread.h>

#include <QWaitCondition>
#include <QObject>
#include <QEvent>
#include <QMutex>
#include <QMap>

#include "programinfo.h"

using namespace std;

// Strings are used by JobQueue::StatusText()
#define JOBSTATUS_MAP(F) \
    F(JOB_UNKNOWN,      0x0000, QObject::tr("Unknown")) \
    F(JOB_QUEUED,       0x0001, QObject::tr("Queued")) \
    F(JOB_PENDING,      0x0002, QObject::tr("Pending")) \
    F(JOB_STARTING,     0x0003, QObject::tr("Starting")) \
    F(JOB_RUNNING,      0x0004, QObject::tr("Running")) \
    F(JOB_STOPPING,     0x0005, QObject::tr("Stopping")) \
    F(JOB_PAUSED,       0x0006, QObject::tr("Paused")) \
    F(JOB_RETRY,        0x0007, QObject::tr("Retrying")) \
    F(JOB_ERRORING,     0x0008, QObject::tr("Erroring")) \
    F(JOB_ABORTING,     0x0009, QObject::tr("Aborting")) \
    /* \
     * JOB_DONE is a mask to indicate the job is done no matter what the \
     * status \
     */ \
    F(JOB_DONE,         0x0100, QObject::tr("Done (Invalid status!)")) \
    F(JOB_FINISHED,     0x0110, QObject::tr("Finished")) \
    F(JOB_ABORTED,      0x0120, QObject::tr("Aborted")) \
    F(JOB_ERRORED,      0x0130, QObject::tr("Errored")) \
    F(JOB_CANCELLED,    0x0140, QObject::tr("Cancelled")) \

enum JobStatus {
#define JOBSTATUS_ENUM(A,B,C)   A = B ,
    JOBSTATUS_MAP(JOBSTATUS_ENUM)
};

enum JobCmds {
    JOB_RUN          = 0x0000,
    JOB_PAUSE        = 0x0001,
    JOB_RESUME       = 0x0002,
    JOB_STOP         = 0x0004,
    JOB_RESTART      = 0x0008
};

enum JobFlags {
    JOB_NO_FLAGS     = 0x0000,
    JOB_USE_CUTLIST  = 0x0001,
    JOB_LIVE_REC     = 0x0002,
    JOB_EXTERNAL     = 0x0004
};

enum JobLists {
    JOB_LIST_ALL      = 0x0001,
    JOB_LIST_DONE     = 0x0002,
    JOB_LIST_NOT_DONE = 0x0004,
    JOB_LIST_ERROR    = 0x0008,
    JOB_LIST_RECENT   = 0x0010
};

enum JobTypes {
    JOB_NONE         = 0x0000,

    JOB_SYSTEMJOB    = 0x00ff,
    JOB_TRANSCODE    = 0x0001,
    JOB_COMMFLAG     = 0x0002,

    JOB_USERJOB      = 0xff00,
    JOB_USERJOB1     = 0x0100,
    JOB_USERJOB2     = 0x0200,
    JOB_USERJOB3     = 0x0400,
    JOB_USERJOB4     = 0x0800
};

typedef struct jobqueueentry {
    int id;
    QString chanid;
    QDateTime starttime;
    QDateTime schedruntime;
    QString startts;
    QDateTime inserttime;
    int type;
    int cmds;
    int flags;
    int status;
    QDateTime statustime;
    QString hostname;
    QString args;
    QString comment;
} JobQueueEntry;

typedef struct runningjobinfo {
    int          id;
    int          type;
    int          flag;
    QString      desc;
    QString      command;
    ProgramInfo *pginfo;
} RunningJobInfo;

class MPUBLIC JobQueue : public QObject
{
    Q_OBJECT
  public:
    JobQueue(bool master);
    ~JobQueue(void);
    void customEvent(QEvent *e);

    static bool QueueRecordingJobs(ProgramInfo *pinfo, int jobTypes = JOB_NONE);
    static bool QueueJob(int jobType, QString chanid,
                         QDateTime starttime, QString args = "",
                         QString comment = "", QString host = "",
                         int flags = 0, int status = JOB_QUEUED,
                         QDateTime schedruntime = QDateTime());

    static bool QueueJobs(int jobTypes, QString chanid,
                         QDateTime starttime, QString args = "",
                         QString comment = "", QString host = "");

    static int GetJobID(int jobType, QString chanid,
                        QDateTime starttime);
    static bool GetJobInfoFromID(int jobID, int &jobType,
                                 QString &chanid, QDateTime &starttime);
    static bool GetJobInfoFromID(int jobID, int &jobType,
                                 QString &chanid, QString &starttime);

    static bool ChangeJobCmds(int jobID, int newCmds);
    static bool ChangeJobCmds(int jobType, QString chanid,
                              QDateTime starttime, int newCmds);
    static bool ChangeJobFlags(int jobID, int newFlags);
    static bool ChangeJobStatus(int jobID, int newStatus,
                                QString comment = "");
    static bool ChangeJobHost(int jobID, QString newHostname);
    static bool ChangeJobComment(int jobID,
                                 QString comment = "");
    static bool ChangeJobArgs(int jobID,
                              QString args = "");
    static bool IsJobQueuedOrRunning(int jobType, QString chanid,
                                     QDateTime starttime);
    int GetRunningJobID(const QString &chanid, const QDateTime &starttime);
    static bool IsJobRunning(int jobType, QString chanid,
                             QDateTime starttime);
    static bool IsJobRunning(int jobType, const ProgramInfo *pginfo);
    static bool IsJobQueued(int jobType, QString chanid, QDateTime starttime);
    static bool PauseJob(int jobID);
    static bool ResumeJob(int jobID);
    static bool RestartJob(int jobID);
    static bool StopJob(int jobID);
    static bool DeleteJob(int jobID);

    static enum JobCmds GetJobCmd(int jobID);
    static enum JobFlags GetJobFlags(int jobID);
    static enum JobStatus GetJobStatus(int jobID);
    static enum JobStatus GetJobStatus(int jobType, QString chanid,
                        QDateTime starttime);
    static QString GetJobArgs(int jobID);
    static int UserJobTypeToIndex(int JobType);

    static bool DeleteAllJobs(QString chanid, QDateTime starttime);

    static void ClearJobMask(int &mask) { mask = JOB_NONE; }
    static bool JobIsInMask(int job, int mask) { return (bool)(job & mask); }
    static bool JobIsNotInMask(int job, int mask)
                               { return ! JobIsInMask(job, mask); }
    static void AddJobsToMask(int jobs, int &mask) { mask |= jobs; }
    static void RemoveJobsFromMask(int jobs, int &mask) { mask &= ~jobs; }

    static QString JobText(int jobType);
    static QString StatusText(int status);

    static bool HasRunningOrPendingJobs(int startingWithinMins = 0);

    static int GetJobsInQueue(QMap<int, JobQueueEntry> &jobs,
                              int findJobs = JOB_LIST_NOT_DONE);

    static void RecoverQueue(bool justOld = false);
    static void RecoverOldJobsInQueue()
                                      { RecoverQueue(true); }
    static void CleanupOldJobsInQueue();

  private:
    typedef struct jobthreadstruct
    {
        JobQueue *jq;
        int jobID;
    } JobThreadStruct;

    static void *QueueProcesserThread(void *param);
    void RunQueueProcesser(void);
    void ProcessQueue(void);

    void ProcessJob(JobQueueEntry job);

    bool AllowedToRun(JobQueueEntry job);

    static bool InJobRunWindow(int orStartingWithinMins = 0);

    void StartChildJob(void *(*start_routine)(void *), int jobID);

    QString GetJobDescription(int jobType);
    QString GetJobCommand(int id, int jobType, ProgramInfo *tmpInfo);
    void RemoveRunningJob(int id);

    static QString PrettyPrint(off_t bytes);

    static void *TranscodeThread(void *param);
    void DoTranscodeThread(int jobID);

    static void *FlagCommercialsThread(void *param);
    void DoFlagCommercialsThread(int jobID);

    static void *UserJobThread(void *param);
    void DoUserJobThread(int jobID);

    QString m_hostname;

    int jobsRunning;
    int jobQueueCPU;

    ProgramInfo *m_pginfo;

    QMutex controlFlagsLock;
    QMap<QString, int *> jobControlFlags;

    QMutex *runningJobsLock;
    QMap<int, RunningJobInfo> runningJobs;

    bool isMaster;

    pthread_t queueThread;
    QWaitCondition queueThreadCond;
    QMutex queueThreadCondLock;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
