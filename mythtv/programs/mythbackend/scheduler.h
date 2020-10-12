#ifndef SCHEDULER_H_
#define SCHEDULER_H_

// C++ headers
#include <deque>
#include <vector>
using namespace std;

// Qt headers
#include <QWaitCondition>
#include <QObject>
#include <QString>
#include <QMutex>
#include <QMap>
#include <QSet>

// MythTV headers
#include "filesysteminfo.h"
#include "recordinginfo.h"
#include "remoteutil.h"
#include "mythdeque.h"
#include "mythscheduler.h"
#include "mthread.h"
#include "scheduledrecording.h"

class EncoderLink;
class MainServer;
class AutoExpire;

class Scheduler;

class SchedInputInfo
{
  public:
    SchedInputInfo(void) = default;;
    ~SchedInputInfo(void) = default;

    uint          m_inputId      {0};
    uint          m_sgroupId     {0};
    bool          m_schedGroup   {false};
    vector<uint>  m_groupInputs;
    vector<uint>  m_conflictingInputs;
    RecList      *m_conflictList {nullptr};
};

class Scheduler : public MThread, public MythScheduler
{
  public:
    Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList,
              const QString& tmptable = "record", Scheduler *master_sched = nullptr);
    ~Scheduler() override;

    void Stop(void);
    void Wait(void) { MThread::wait(); }

    void SetExpirer(AutoExpire *autoExpirer) { m_expirer = autoExpirer; }

    void Reschedule(const QStringList &request);
    void RescheduleMatch(uint recordid, uint sourceid, uint mplexid,
                         const QDateTime &maxstarttime, const QString &why)
    { Reschedule(ScheduledRecording::BuildMatchRequest(recordid, sourceid,
                                               mplexid, maxstarttime, why)); };
    void RescheduleCheck(const RecordingInfo &recinfo, const QString &why)
    { Reschedule(ScheduledRecording::BuildCheckRequest(recinfo, why)); };
    void ReschedulePlace(const QString &why)
    { Reschedule(ScheduledRecording::BuildPlaceRequest(why)); };

    void AddRecording(const RecordingInfo &pi);
    void AddRecording(const ProgramInfo& prog)
    { AddRecording(RecordingInfo(prog)); };
    void FillRecordListFromDB(uint recordid = 0);
    void FillRecordListFromMaster(void);

    void UpdateRecStatus(RecordingInfo *pginfo);
    void UpdateRecStatus(uint cardid, uint chanid,
                         const QDateTime &startts, RecStatus::Type recstatus,
                         const QDateTime &recendts);
    // Returns a list of all pending recordings and returns
    // true iff there are conflicts
    bool GetAllPending(RecList &retList, int recRuleId = 0) const;
    bool GetAllPending(ProgramList &retList, int recRuleId = 0) const;
    void GetAllPending(QStringList &strList) const override; // MythScheduler
    QMap<QString,ProgramInfo*> GetRecording(void) const override; // MythScheduler

    enum SchedSortColumn { kSortTitle, kSortLastRecorded, kSortNextRecording,
                           kSortPriority, kSortType };
    static void GetAllScheduled(QStringList &strList,
                                SchedSortColumn sortBy = kSortTitle,
                                bool ascending = true);
    static void GetAllScheduled(RecList &proglist,
                                SchedSortColumn sortBy = kSortTitle,
                                bool ascending = true);

    void getConflicting(RecordingInfo *pginfo, QStringList &strlist);
    void getConflicting(RecordingInfo *pginfo, RecList *retlist);

    void PrintList(bool onlyFutureRecordings = false)
        { PrintList(m_recList, onlyFutureRecordings); };
    static void PrintList(RecList &list, bool onlyFutureRecordings = false);
    static void PrintRec(const RecordingInfo *p, const QString &prefix = "");

    void SetMainServer(MainServer *ms);

    void SlaveConnected(RecordingList &slavelist);
    void SlaveDisconnected(uint cardid);

    void DisableScheduling(void) { m_schedulingEnabled = false; }
    void EnableScheduling(void) { m_schedulingEnabled = true; }
    void GetNextLiveTVDir(uint cardid);
    void ResetIdleTime(void);

    static bool WasStartedAutomatically();

    RecStatus::Type GetRecStatus(const ProgramInfo &pginfo);

    int GetError(void) const { return m_error; }

    void AddChildInput(uint parentid, uint inputid);
    void DelayShutdown();

  protected:
    void run(void) override; // MThread

  private:
    enum OpenEndType {
        openEndNever = 0,
        openEndDiffChannel = 1,
        openEndAlways = 2
    };

    QString m_recordTable;
    QString m_priorityTable;

    static bool VerifyCards(void);

    bool InitInputInfoMap(void);
    void CreateTempTables(void);
    void DeleteTempTables(void);
    void UpdateDuplicates(void);
    bool FillRecordList(void);
    void UpdateMatches(uint recordid, uint sourceid, uint mplexid,
                       const QDateTime &maxstarttime);
    void UpdateManuals(uint recordid);
    void BuildWorkList(void);
    bool ClearWorkList(void);
    void AddNewRecords(void);
    void AddNotListed(void);
    void BuildNewRecordsQueries(uint recordid, QStringList &from,
                                QStringList &where, MSqlBindings &bindings);
    void PruneOverlaps(void);
    void BuildListMaps(void);
    void ClearListMaps(void);

    bool IsBusyRecording(const RecordingInfo *rcinfo);

    bool IsSameProgram(const RecordingInfo *a, const RecordingInfo *b) const;

    bool FindNextConflict(const RecList &cardlist,
                          const RecordingInfo *p, RecConstIter &iter,
                          OpenEndType openEnd = openEndNever,
                          uint *paffinity = nullptr,
                          bool ignoreinput = false) const;
    const RecordingInfo *FindConflict(const RecordingInfo *p,
                                      OpenEndType openEnd = openEndNever,
                                      uint *affinity = nullptr,
                                      bool checkAll = false)
        const;
    void MarkOtherShowings(RecordingInfo *p);
    void MarkShowingsList(RecList &showinglist, RecordingInfo *p);
    void BackupRecStatus(void);
    void RestoreRecStatus(void);
    bool TryAnotherShowing(RecordingInfo *p,  bool samePriority,
                           bool livetv = false);
    void SchedNewRecords(void);
    void SchedNewFirstPass(RecIter &start, const RecIter& end,
                           int recpriority, int recpriority2);
    void SchedNewRetryPass(const RecIter& start, const RecIter& end,
                           bool samePriority, bool livetv = false);
    void SchedLiveTV(void);
    void PruneRedundants(void);
    void UpdateNextRecord(void);

    bool ChangeRecordingEnd(RecordingInfo *oldp, RecordingInfo *newp);

    static bool CheckShutdownServer(int prerollseconds, QDateTime &idleSince,
                             bool &blockShutdown, uint logmask);
    void ShutdownServer(int prerollseconds, QDateTime &idleSince);
    void PutInactiveSlavesToSleep(void);
    bool WakeUpSlave(const QString& slaveHostname, bool setWakingStatus = true);
    void WakeUpSlaves(void);

    int FillRecordingDir(const QString &title,
                         const QString &hostname,
                         const QString &storagegroup,
                         const QDateTime &recstartts,
                         const QDateTime &recendts,
                         uint cardid,
                         QString &recording_dir,
                         const RecList &reclist);
    void FillDirectoryInfoCache(void);

    void OldRecordedFixups(void);
    void ResetDuplicates(uint recordid, uint findid, const QString &title,
                         const QString &subtitle, const QString &descrip,
                         const QString &programid);
    bool HandleReschedule(void);
    bool HandleRunSchedulerStartup(
        int prerollseconds, int idleWaitForRecordingTime);
    void HandleWakeSlave(RecordingInfo &ri, int prerollseconds);
    bool HandleRecording(RecordingInfo &ri, bool &statuschanged,
                         QDateTime &nextStartTime, QDateTime &nextWakeTime,
                         int prerollseconds);
    void HandleRecordingStatusChange(
        RecordingInfo &ri, RecStatus::Type recStatus, const QString &details);
    bool AssignGroupInput(RecordingInfo &ri, int prerollseconds);
    void HandleIdleShutdown(
        bool &blockShutdown, QDateTime &idleSince, int prerollseconds,
        int idleTimeoutSecs, int idleWaitForRecordingTime,
        bool &statuschanged);

    void EnqueueMatch(uint recordid, uint sourceid, uint mplexid,
                      const QDateTime &maxstarttime, const QString &why)
    { m_reschedQueue.enqueue(ScheduledRecording::BuildMatchRequest(recordid,
                                     sourceid, mplexid, maxstarttime, why)); };
    void EnqueueCheck(const RecordingInfo &recinfo, const QString &why)
    { m_reschedQueue.enqueue(ScheduledRecording::BuildCheckRequest(recinfo,
                                                                 why)); };
    void EnqueuePlace(const QString &why)
    { m_reschedQueue.enqueue(ScheduledRecording::BuildPlaceRequest(why)); };

    bool HaveQueuedRequests(void)
    { return !m_reschedQueue.empty(); };
    void ClearRequestQueue(void)
    { m_reschedQueue.clear(); };

    bool CreateConflictLists(void);

    MythDeque<QStringList> m_reschedQueue;
    mutable QMutex         m_schedLock;
    QMutex                 m_recordMatchLock;
    QWaitCondition         m_reschedWait;
    RecList                m_recList;
    RecList                m_workList;
    RecList                m_livetvList;
    QMap<uint, SchedInputInfo> m_sinputInfoMap;
    vector<RecList *>      m_conflictLists;
    QMap<uint, RecList>    m_recordIdListMap;
    QMap<QString, RecList> m_titleListMap;

    QDateTime m_schedTime;
    bool m_recListChanged              {false};

    bool m_specSched;
    bool m_schedulingEnabled           {true};
    QMap<int, bool> m_schedAfterStartMap;

    QMap<int, EncoderLink *> *m_tvList {nullptr};
    AutoExpire *m_expirer              {nullptr};

    QSet<uint> m_schedOrderWarned;

    bool m_doRun;

    MainServer *m_mainServer           {nullptr};

    QMutex m_resetIdleTimeLock;
    bool   m_resetIdleTime             {false};

    bool m_isShuttingDown              {false};
    MSqlQueryInfo m_dbConn;

    QMap<QString, FileSystemInfo> m_fsInfoCache;

    int m_error                        {0};

    QSet<QString> m_sysEvents[4];

    // Try to avoid LiveTV sessions until this time
    QDateTime m_livetvTime;

    QDateTime m_lastPrepareTime;
    // Delay shutdown util this time (ms since epoch);
    int64_t m_delayShutdownTime        {0};

    OpenEndType m_openEnd;

    // cache IsSameProgram()
    using IsSameKey = pair<const RecordingInfo*,const RecordingInfo*>;
    using IsSameCacheType = QMap<IsSameKey,bool>;
    mutable IsSameCacheType m_cacheIsSameProgram;
    int m_tmLastLog                    {0};
};

#endif
