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
#include "inputgroupmap.h"
#include "mythdeque.h"
#include "mythscheduler.h"
#include "mthread.h"
#include "scheduledrecording.h"

class EncoderLink;
class MainServer;
class AutoExpire;

class Scheduler;

class Scheduler : public MThread, public MythScheduler
{
  public:
    Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList,
              QString recordTbl = "record", Scheduler *master_sched = NULL);
    ~Scheduler();

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

    void AddRecording(const RecordingInfo&);
    void FillRecordListFromDB(uint recordid = 0);
    void FillRecordListFromMaster(void);

    void UpdateRecStatus(RecordingInfo *pginfo);
    void UpdateRecStatus(uint cardid, uint chanid,
                         const QDateTime &startts, RecStatusType recstatus,
                         const QDateTime &recendts);
    // Returns a list of all pending recordings and returns
    // true iff there are conflicts
    bool GetAllPending(RecList &retList) const;
    virtual void GetAllPending(QStringList &strList) const;
    virtual QMap<QString,ProgramInfo*> GetRecording(void) const;

    enum SchedSortColumn { kSortTitle, kSortLastRecorded, kSortPriority, kSortType };
    static void GetAllScheduled(QStringList &strList,
                                SchedSortColumn sortBy = kSortTitle,
                                bool ascending = true);
    static void GetAllScheduled(RecList &proglist,
                                SchedSortColumn sortBy = kSortTitle,
                                bool ascending = true);

    void getConflicting(RecordingInfo *pginfo, QStringList &strlist);
    void getConflicting(RecordingInfo *pginfo, RecList *retlist);

    void PrintList(bool onlyFutureRecordings = false)
        { PrintList(reclist, onlyFutureRecordings); };
    void PrintList(RecList &list, bool onlyFutureRecordings = false);
    void PrintRec(const RecordingInfo *p, const char *prefix = NULL);

    void SetMainServer(MainServer *ms);

    void SlaveConnected(RecordingList &slavelist);
    void SlaveDisconnected(uint cardid);

    void DisableScheduling(void) { schedulingEnabled = false; }
    void EnableScheduling(void) { schedulingEnabled = true; }
    void GetNextLiveTVDir(uint cardid);
    void ResetIdleTime(void);

    bool WasStartedAutomatically();

    RecStatusType GetRecStatus(const ProgramInfo &pginfo);

    int GetError(void) const { return error; }

  protected:
    virtual void run(void); // MThread

  private:
    QString recordTable;
    QString priorityTable;

    bool VerifyCards(void);

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
                          int openEnd = 0) const;
    const RecordingInfo *FindConflict(const RecordingInfo *p, int openEnd = 0)
        const;
    void MarkOtherShowings(RecordingInfo *p);
    void MarkShowingsList(RecList &showinglist, RecordingInfo *p);
    void BackupRecStatus(void);
    void RestoreRecStatus(void);
    bool TryAnotherShowing(RecordingInfo *p,  bool samePriority,
                           bool livetv = false);
    void SchedNewRecords(void);
    void MoveHigherRecords(bool livetv = false);
    void SchedLiveTV(void);
    void PruneRedundants(void);
    void UpdateNextRecord(void);

    bool ChangeRecordingEnd(RecordingInfo *oldp, RecordingInfo *newp);

    bool CheckShutdownServer(int prerollseconds, QDateTime &idleSince,
                             bool &blockShutdown);
    void ShutdownServer(int prerollseconds, QDateTime &idleSince);
    void PutInactiveSlavesToSleep(void);
    bool WakeUpSlave(QString slaveHostname, bool setWakingStatus = true);
    void WakeUpSlaves(void);

    int FillRecordingDir(const QString &title,
                         const QString &hostname,
                         const QString &storagegroup,
                         const QDateTime &recstartts,
                         const QDateTime &recendts,
                         uint cardid,
                         QString &recording_dir,
                         const RecList &reclist);
    void FillDirectoryInfoCache(bool force = false);

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
        RecordingInfo &ri, RecStatusTypes recStatus, const QString &details);
    void HandleIdleShutdown(
        bool &blockShutdown, QDateTime &idleSince, int prerollseconds,
        int idleTimeoutSecs, int idleWaitForRecordingTime,
        bool &statuschanged);

    void EnqueueMatch(uint recordid, uint sourceid, uint mplexid,
                      const QDateTime &maxstarttime, const QString &why)
    { reschedQueue.enqueue(ScheduledRecording::BuildMatchRequest(recordid,
                                     sourceid, mplexid, maxstarttime, why)); };
    void EnqueueCheck(const RecordingInfo &recinfo, const QString &why)
    { reschedQueue.enqueue(ScheduledRecording::BuildCheckRequest(recinfo,
                                                                 why)); };
    void EnqueuePlace(const QString &why)
    { reschedQueue.enqueue(ScheduledRecording::BuildPlaceRequest(why)); };

    bool HaveQueuedRequests(void)
    { return !reschedQueue.empty(); };
    void ClearRequestQueue(void)
    { reschedQueue.clear(); };

    void CreateConflictLists(void);

    MythDeque<QStringList> reschedQueue;
    mutable QMutex schedLock;
    QMutex recordmatchLock;
    QWaitCondition reschedWait;
    RecList reclist;
    RecList worklist;
    RecList retrylist;
    vector<RecList *> conflictlists;
    QMap<uint, RecList *> conflictlistmap;
    QMap<uint, RecList> recordidlistmap;
    QMap<QString, RecList> titlelistmap;
    InputGroupMap igrp;

    QDateTime schedTime;
    bool reclist_changed;

    bool specsched;
    bool schedulingEnabled;
    QMap<int, bool> schedAfterStartMap;

    QMap<int, EncoderLink *> *m_tvList;
    AutoExpire *m_expirer;

    QMap<QString, bool> recPendingList;

    bool doRun;

    MainServer *m_mainServer;

    QMutex resetIdleTime_lock;
    bool resetIdleTime;

    bool m_isShuttingDown;
    MSqlQueryInfo dbConn;

    QDateTime fsInfoCacheFillTime;
    QMap<QString, FileSystemInfo> fsInfoCache;

    int error;

    QSet<QString> sysEvents[4];

    // Try to avoid LiveTV sessions until this time
    QDateTime livetvTime;

    // cache IsSameProgram()
    typedef pair<const RecordingInfo*,const RecordingInfo*> IsSameKey;
    typedef QMap<IsSameKey,bool> IsSameCacheType;
    mutable IsSameCacheType cache_is_same_program;
};

#endif
