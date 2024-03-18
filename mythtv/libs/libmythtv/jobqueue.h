#ifndef JOBQUEUE_H_
#define JOBQUEUE_H_

#include <sys/types.h>

#include <QWaitCondition>
#include <QDateTime>
#include <QRunnable>
#include <QObject>
#include <QEvent>
#include <QMutex>
#include <QRecursiveMutex>
#include <QMap>

#include "mythtvexp.h"
#include "libmythbase/mythchrono.h"

class MThread;
class ProgramInfo;
class RecordingInfo;

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// Strings are used by JobQueue::StatusText()
#define JOBSTATUS_MAP(F) \
    F(JOB_UNKNOWN,      0x0000, JobQueue::tr("Unknown")) \
    F(JOB_QUEUED,       0x0001, JobQueue::tr("Queued")) \
    F(JOB_PENDING,      0x0002, JobQueue::tr("Pending")) \
    F(JOB_STARTING,     0x0003, JobQueue::tr("Starting")) \
    F(JOB_RUNNING,      0x0004, JobQueue::tr("Running")) \
    F(JOB_STOPPING,     0x0005, JobQueue::tr("Stopping")) \
    F(JOB_PAUSED,       0x0006, JobQueue::tr("Paused")) \
    F(JOB_RETRY,        0x0007, JobQueue::tr("Retrying")) \
    F(JOB_ERRORING,     0x0008, JobQueue::tr("Erroring")) \
    F(JOB_ABORTING,     0x0009, JobQueue::tr("Aborting")) \
    /* \
     * JOB_DONE is a mask to indicate the job is done no matter what the \
     * status \
     */ \
    F(JOB_DONE,         0x0100, JobQueue::tr("Done (Invalid status!)")) \
    F(JOB_FINISHED,     0x0110, JobQueue::tr("Finished")) \
    F(JOB_ABORTED,      0x0120, JobQueue::tr("Aborted")) \
    F(JOB_ERRORED,      0x0130, JobQueue::tr("Errored")) \
    F(JOB_CANCELLED,    0x0140, JobQueue::tr("Cancelled")) \

#define JOBSTATUS_ENUM(A,B,C)   A = (B) ,
enum JobStatus : std::uint16_t {
    JOBSTATUS_MAP(JOBSTATUS_ENUM)
};

// NOLINTEND(cppcoreguidelines-macro-usage)

enum JobCmds : std::uint8_t {
    JOB_RUN          = 0x0000,
    JOB_PAUSE        = 0x0001,
    JOB_RESUME       = 0x0002,
    JOB_STOP         = 0x0004,
    JOB_RESTART      = 0x0008
};

enum JobFlags : std::uint8_t {
    JOB_NO_FLAGS     = 0x0000,
    JOB_USE_CUTLIST  = 0x0001,
    JOB_LIVE_REC     = 0x0002,
    JOB_EXTERNAL     = 0x0004,
    JOB_REBUILD      = 0x0008
};

enum JobLists : std::uint8_t {
    JOB_LIST_ALL      = 0x0001,
    JOB_LIST_DONE     = 0x0002,
    JOB_LIST_NOT_DONE = 0x0004,
    JOB_LIST_ERROR    = 0x0008,
    JOB_LIST_RECENT   = 0x0010
};

enum JobTypes : std::uint16_t {
    JOB_NONE         = 0x0000,

    JOB_SYSTEMJOB    = 0x00ff,
    JOB_TRANSCODE    = 0x0001,
    JOB_COMMFLAG     = 0x0002,
    JOB_METADATA     = 0x0004,
    JOB_PREVIEW      = 0x0008,

    JOB_USERJOB      = 0xff00,
    JOB_USERJOB1     = 0x0100,
    JOB_USERJOB2     = 0x0200,
    JOB_USERJOB3     = 0x0400,
    JOB_USERJOB4     = 0x0800
};

static QMap< QString, int > JobNameToType {
    { "Transcode", JOB_TRANSCODE },
    { "Commflag",  JOB_COMMFLAG },
    { "Metadata",  JOB_METADATA },
    { "UserJob1",  JOB_USERJOB1 },
    { "UserJob2",  JOB_USERJOB2 },
    { "UserJob3",  JOB_USERJOB3 },
    { "UserJob4",  JOB_USERJOB4 }
};

struct JobQueueEntry {
    int       id           {0};
    uint      chanid       {0};
    QDateTime recstartts;
    QDateTime schedruntime;
    QString startts;
    QDateTime inserttime;
    int       type         {0};
    int       cmds         {0};
    int       flags        {0};
    int       status       {0};
    QDateTime statustime;
    QString hostname;
    QString args;
    QString comment;
};

struct RunningJobInfo {
    int          id      {0};
    int          type    {0};
    int          flag    {0};
    QString      desc;
    QString      command;
    ProgramInfo *pginfo  {nullptr};
};

class JobQueue;

class MTV_PUBLIC JobQueue : public QObject, public QRunnable
{
    Q_OBJECT

    friend class QueueProcessorThread;
  public:
    explicit JobQueue(bool master);
    ~JobQueue(void) override;
    void customEvent(QEvent *e)  override; // QObject

    static bool QueueRecordingJobs(
        const RecordingInfo &recinfo, int jobTypes = JOB_NONE);
    static bool QueueJob(int jobType, uint chanid,
                         const QDateTime &recstartts, const QString& args = "",
                         const QString& comment = "", QString host = "",
                         int flags = 0, int status = JOB_QUEUED,
                         QDateTime schedruntime = QDateTime());

    static bool QueueJobs(int jobTypes, uint chanid,
                         const QDateTime &recstartts, const QString& args = "",
                         const QString& comment = "", const QString& host = "");

    static int GetJobID(int jobType, uint chanid,
                        const QDateTime &recstartts);
    static bool GetJobInfoFromID(int jobID, int &jobType,
                                 uint &chanid, QDateTime &recstartts);
    static bool GetJobInfoFromID(int jobID, int &jobType,
                                 uint &chanid, QString &recstartts);
    static int GetJobTypeFromName(const QString &name);

    static bool ChangeJobCmds(int jobID, int newCmds);
    static bool ChangeJobCmds(int jobType, uint chanid,
                              const QDateTime &recstartts, int newCmds);
    static bool ChangeJobFlags(int jobID, int newFlags);
    static bool ChangeJobStatus(int jobID, int newStatus,
                                const QString& comment = "");
    static bool ChangeJobHost(int jobID, const QString& newHostname);
    static bool ChangeJobComment(int jobID,
                                 const QString& comment = "");
    static bool ChangeJobArgs(int jobID,
                              const QString& args = "");
    static bool IsJobQueuedOrRunning(int jobType, uint chanid,
                                     const QDateTime &recstartts);
    int GetRunningJobID(uint chanid, const QDateTime &recstartts);
    static bool IsJobRunning(int jobType, uint chanid,
                             const QDateTime &recstartts);
    static bool IsJobRunning(int jobType, const ProgramInfo &pginfo);
    static bool IsJobQueued(int jobType,
                            uint chanid, const QDateTime &recstartts);
    static bool IsJobStatusQueued(int status);
    static bool IsJobStatusRunning(int status);
    static bool PauseJob(int jobID);
    static bool ResumeJob(int jobID);
    static bool RestartJob(int jobID);
    static bool StopJob(int jobID);
    static bool DeleteJob(int jobID);
    static bool SafeDeleteJob(int jobID, int jobType, int chanid,
                              const QDateTime& recstartts);

    static enum JobCmds GetJobCmd(int jobID);
    static enum JobFlags GetJobFlags(int jobID);
    static enum JobStatus GetJobStatus(int jobID);
    static enum JobStatus GetJobStatus(int jobType, uint chanid,
                                       const QDateTime &recstartts);
    static QString GetJobArgs(int jobID);
    static int UserJobTypeToIndex(int JobType);

    static bool DeleteAllJobs(uint chanid, const QDateTime &recstartts);

    static void ClearJobMask(int &mask) { mask = JOB_NONE; }
    static bool JobIsInMask(int job, int mask) { return (bool)(job & mask); }
    static bool JobIsNotInMask(int job, int mask)
                               { return ! JobIsInMask(job, mask); }
    static void AddJobsToMask(int jobs, int &mask) { mask |= jobs; }
    static void RemoveJobsFromMask(int jobs, int &mask) { mask &= ~jobs; }

    static QString JobText(int jobType);
    static QString StatusText(int status);

    static bool HasRunningOrPendingJobs(std::chrono::minutes startingWithinMins = 0min);

    static int GetJobsInQueue(QMap<int, JobQueueEntry> &jobs,
                              int findJobs = JOB_LIST_NOT_DONE);

    static void RecoverQueue(bool justOld = false);
    static void RecoverOldJobsInQueue()
                                      { RecoverQueue(true); }
    static void CleanupOldJobsInQueue();

    static bool InJobRunWindow(QDateTime jobstarttsRaw);

  private:
    struct JobThreadStruct
    {
        JobQueue *jq;
        int jobID;
    };

    void run(void) override; // QRunnable
    void ProcessQueue(void);

    void ProcessJob(const JobQueueEntry& job);

    bool AllowedToRun(const JobQueueEntry& job);

    static bool InJobRunWindow(std::chrono::minutes orStartsWithinMins = 0min);

    void StartChildJob(void *(*ChildThreadRoutine)(void *), int jobID);

    static QString GetJobDescription(int jobType);
    static QString GetJobCommand(int id, int jobType, ProgramInfo *tmpInfo);
    void RemoveRunningJob(int id);

    static QString PrettyPrint(off_t bytes);

    static void *TranscodeThread(void *param);
    void DoTranscodeThread(int jobID);

    static void *MetadataLookupThread(void *param);
    void DoMetadataLookupThread(int jobID);

    static void *FlagCommercialsThread(void *param);
    void DoFlagCommercialsThread(int jobID);

    static void *UserJobThread(void *param);
    void DoUserJobThread(int jobID);

    QString                    m_hostname;

    int                        m_jobsRunning         {0};
    int                        m_jobQueueCPU         {0};

    ProgramInfo               *m_pginfo              {nullptr};

//  QMutex                     m_controlFlagsLock;
//  QMap<QString, int *>       m_jobControlFlags;

    QRecursiveMutex           *m_runningJobsLock     {nullptr};
    QMap<int, RunningJobInfo>  m_runningJobs;

    bool                       m_isMaster;

    MThread                   *m_queueThread         {nullptr};
    QWaitCondition             m_queueThreadCond;
    QMutex                     m_queueThreadCondLock;
    bool                       m_processQueue        {false};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
