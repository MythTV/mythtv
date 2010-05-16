#ifndef SCHEDULER_H_
#define SCHEDULER_H_

// POSIX headers
#include <pthread.h>

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

// MythTV headers
#include "recordinginfo.h"
#include "remoteutil.h"
#include "inputgroupmap.h"
#include "mythdeque.h"

class EncoderLink;
class MainServer;
class AutoExpire;

typedef deque<RecordingInfo*> RecList;
#define SORT_RECLIST(LIST, ORDER) \
  do { stable_sort((LIST).begin(), (LIST).end(), ORDER); } while (0)

typedef RecList::const_iterator RecConstIter;
typedef RecList::iterator RecIter;

class Scheduler : public QObject
{
    Q_OBJECT

  public:
    Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList,
              QString recordTbl = "record", Scheduler *master_sched = NULL);
    ~Scheduler();

    void SetExpirer(AutoExpire *autoExpirer) { expirer = autoExpirer; }

    void Reschedule(int recordid);
    void AddRecording(const RecordingInfo&);
    void FillRecordListFromDB(int recordid = -1);
    void FillRecordListFromMaster(void);

    void UpdateRecStatus(RecordingInfo *pginfo);
    void UpdateRecStatus(uint cardid, uint chanid,
                         const QDateTime &startts, RecStatusType recstatus,
                         const QDateTime &recendts);

    bool getAllPending(RecList *retList) const;
    void getAllPending(QStringList &strList);
    QMap<QString,ProgramInfo*> GetRecording(void) const;

    void getAllScheduled(QStringList &strList);

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

    RecStatusType GetRecStatus(const ProgramInfo &pginfo) const;

    int GetError(void) const { return error; }

  protected:
    void RunScheduler(void);
    static void *SchedulerThread(void *param);

  private:
    QString recordTable;
    QString priorityTable;

    bool VerifyCards(void);

    bool FillRecordList(void);
    void UpdateMatches(int recordid);
    void UpdateManuals(int recordid);
    void BuildWorkList(void);
    bool ClearWorkList(void);
    void AddNewRecords(void);
    void AddNotListed(void);
    void BuildNewRecordsQueries(int recordid, QStringList &from, QStringList &where,
                                MSqlBindings &bindings);
    void PruneOverlaps(void);
    void BuildListMaps(void);
    void ClearListMaps(void);

    bool IsBusyRecording(const RecordingInfo *rcinfo);

    bool IsSameProgram(const RecordingInfo *a, const RecordingInfo *b) const;

    bool FindNextConflict(const RecList &cardlist,
                          const RecordingInfo *p, RecConstIter &iter,
                          int openEnd = 0) const;
    const RecordingInfo *FindConflict(const QMap<int, RecList> &reclists,
                                    const RecordingInfo *p, int openEnd = 0) const;
    void MarkOtherShowings(RecordingInfo *p);
    void MarkShowingsList(RecList &showinglist, RecordingInfo *p);
    void BackupRecStatus(void);
    void RestoreRecStatus(void);
    bool TryAnotherShowing(RecordingInfo *p,  bool samePriority,
                           bool preserveLive = false);
    void SchedNewRecords(void);
    void MoveHigherRecords(bool move_this = true);
    void SchedPreserveLiveTV(void);
    void PruneRedundants(void);
    void UpdateNextRecord(void);

    bool ChangeRecordingEnd(RecordingInfo *oldp, RecordingInfo *newp);

    void findAllScheduledPrograms(RecList &proglist);
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

    MythDeque<int> reschedQueue;
    QMutex reschedLock;
    QMutex recordmatchLock;
    QWaitCondition reschedWait;
    RecList reclist;
    RecList worklist;
    RecList retrylist;
    QMap<int, RecList> cardlistmap;
    QMap<int, RecList> recordidlistmap;
    QMap<QString, RecList> titlelistmap;
    InputGroupMap igrp;

    QMutex *reclist_lock;
    bool reclist_changed;

    bool specsched;
    bool schedMoveHigher;
    bool schedulingEnabled;
    QMap<int, bool> schedAfterStartMap;

    QMap<int, EncoderLink *> *m_tvList;
    AutoExpire *expirer;

    QMap<QString, bool> recPendingList;

    pthread_t schedThread;
    bool threadrunning;

    MainServer *m_mainServer;

    QMutex resetIdleTime_lock;
    bool resetIdleTime;

    bool m_isShuttingDown;
    MSqlQueryInfo dbConn;

    QDateTime fsInfoCacheFillTime;
    QMap<QString, FileSystemInfo> fsInfoCache;

    int error;

    // Try to avoid LiveTV sessions until this time
    QDateTime livetvTime;
    int livetvpriority;
    int prefinputpri;
    QMap<QString, bool> hasLaterList;

    // cache IsSameProgram()
    typedef pair<const RecordingInfo*,const RecordingInfo*> IsSameKey;
    typedef QMap<IsSameKey,bool> IsSameCacheType;
    mutable IsSameCacheType cache_is_same_program;
};

#endif
