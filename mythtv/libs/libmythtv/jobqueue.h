#ifndef JOBQUEUE_H_
#define JOBQUEUE_H_

#include <qobject.h>
#include <qmap.h>
#include <qmutex.h>
#include <qobject.h>
#include <qsqldatabase.h>
#include <pthread.h>


#include "programinfo.h"

using namespace std;

// When Updating this enum, also update the JobQueue::StatusText() method.
enum JobStatus {
    JOB_QUEUED        = 0x0001,
    JOB_PENDING       = 0x0002,
    JOB_STARTING      = 0x0003,
    JOB_RUNNING       = 0x0004,
    JOB_STOPPING      = 0x0005,
    JOB_PAUSED        = 0x0006,
    JOB_RETRY         = 0x0007,
    JOB_ERRORING      = 0x0008,
    JOB_ABORTING      = 0x0009,

    // JOB_DONE is a mask to indicate the job is done no matter what the status
    JOB_DONE          = 0x0100,
    JOB_FINISHED      = 0x0110,
    JOB_ABORTED       = 0x0120,
    JOB_ERRORED       = 0x0130,
    JOB_CANCELLED     = 0x0140,

    JOB_UNKNOWN       = 0x0000
};

enum JobCmds {
    JOB_RUN          = 0x0000,
    JOB_PAUSE        = 0x0001,
    JOB_RESUME       = 0x0002,
    JOB_STOP         = 0x0004,
    JOB_RESTART      = 0x0008
};

enum JobFlags {
    JOB_REG_FLAGS    = 0x0000,
    JOB_USE_CUTLIST  = 0x0001,
    JOB_LIVE_REC     = 0x0002
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

class JobQueue : public QObject
{
  public:
    JobQueue(bool master);
    ~JobQueue(void);
    void customEvent(QCustomEvent *e);

    static bool QueueJob(int jobType, QString chanid,
                         QDateTime starttime, QString args = "",
                         QString comment = "", QString host = "",
                         int flags = 0);
    static bool QueueJobs(int jobTypes, QString chanid,
                         QDateTime starttime, QString args = "",
                         QString comment = "", QString host = "");

    static int GetJobID(int jobType, QString chanid,
                        QDateTime starttime);
    static bool GetJobInfoFromID(int jobID, int &jobType,
                                 QString &chanid, QDateTime &starttime);

    static bool ChangeJobCmds(int jobID, int newCmds);
    static bool ChangeJobCmds(int jobType, QString chanid,
                              QDateTime starttime, int newCmds);
    static bool ChangeJobFlags(int jobID, int newFlags);
    static bool ChangeJobStatus(int jobID, int newStatus,
                                QString comment = "");
    static bool ChangeJobComment(int jobID,
                                 QString comment = "");
    static bool IsJobQueuedOrRunning(int jobType, QString chanid,
                                     QDateTime starttime);
    static bool IsJobRunning(int jobType, QString chanid,
                             QDateTime starttime);
    static bool IsJobQueued(int jobType, QString chanid, QDateTime starttime);
    static bool PauseJob(int jobID);
    static bool ResumeJob(int jobID);
    static bool RestartJob(int jobID);
    static bool StopJob(int jobID);
    static bool DeleteJob(int jobID);

    static int GetJobCmd(int jobID);
    static int GetJobFlags(int jobID);
    static int GetJobStatus(int jobID);
    static int GetJobStatus(int jobType, QString chanid, QDateTime starttime);

    static bool DeleteAllJobs(QString chanid, QDateTime starttime);

    static QString JobText(int jobType);
    static QString StatusText(int status);

    static int GetJobsInQueue(QMap<int, JobQueueEntry> &jobs,
                              int findJobs = JOB_LIST_NOT_DONE);

    static void RecoverQueue(bool justOld = false);
    static void RecoverOldJobsInQueue()
                                      { RecoverQueue(true); }
    static void CleanupOldJobsInQueue();

  private:
    static void *QueueProcesserThread(void *param);
    void RunQueueProcesser(void);
    void ProcessQueue(void);

    void ProcessJob(int id, int jobType, QString chanid, QDateTime starttime);

    bool AllowedToRun(JobQueueEntry job);
    bool ClaimJob(int jobID);

    void StartChildJob(void *(*start_routine)(void *), ProgramInfo *tmpInfo);

    QString GetJobDescription(int jobType);
    QString GetJobCommand(int jobType, ProgramInfo *tmpInfo);

    static void *TranscodeThread(void *param);
    void DoTranscodeThread(void);

    static void *FlagCommercialsThread(void *param);
    void DoFlagCommercialsThread(void);

    static void *UserJobThread(void *param);
    void DoUserJobThread(void);

    QString m_hostname;

    int jobsRunning;
    int jobQueueCPU;

    ProgramInfo *m_pginfo;

    QMutex controlFlagsLock;
    QMap<QString, int *> jobControlFlags;

    QMap<QString, int> runningJobIDs;
    QMap<QString, int> runningJobTypes;
    QMap<QString, QString> runningJobDescs;
    QMap<QString, QString> runningJobCommands;

    bool childThreadStarted;
    bool isMaster;

    bool queuePoll;
    pthread_t queueThread;
};

#endif

