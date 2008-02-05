#ifndef SCHEDULER_H_
#define SCHEDULER_H_

// C++ headers
#include <deque>
#include <vector>
using namespace std;

// Qt headers
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qmap.h> 
#include <qobject.h>

// MythTV headers
#include "scheduledrecording.h"
#include "programinfo.h"
#include "remoteutil.h"
#include "inputgroupmap.h"

class EncoderLink;
class MainServer;
class AutoExpire;

#define USE_DEQUE_RECLIST 1
typedef deque<ProgramInfo*> RecList;
#define SORT_RECLIST(LIST, ORDER) \
  do { stable_sort((LIST).begin(), (LIST).end(), ORDER); } while (0)

typedef RecList::const_iterator RecConstIter;
typedef RecList::iterator RecIter;
typedef RecList::const_iterator RecConstIter;

class Scheduler : public QObject
{
  public:
    Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList,
              QString recordTbl = "record", Scheduler *master_sched = NULL);
    ~Scheduler();

    void SetExpirer(AutoExpire *autoExpirer) { expirer = autoExpirer; }

    void Reschedule(int recordid);
    void AddRecording(const ProgramInfo&);
    void FillRecordListFromDB(int recordid = -1);
    void FillRecordListFromMaster(void);

    void UpdateRecStatus(ProgramInfo *pginfo);
    void UpdateRecStatus(int cardid, const QString &chanid, 
                         const QDateTime &startts, RecStatusType recstatus, 
                         const QDateTime &recendts);

    bool getAllPending(RecList *retList);
    void getAllPending(QStringList &strList);

    void getAllScheduled(QStringList &strList);

    void getConflicting(ProgramInfo *pginfo, QStringList &strlist);
    void getConflicting(ProgramInfo *pginfo, RecList *retlist);

    void PrintList(bool onlyFutureRecordings = false) 
        { PrintList(reclist, onlyFutureRecordings); };
    void PrintList(RecList &list, bool onlyFutureRecordings = false);
    void PrintRec(const ProgramInfo *p, const char *prefix = NULL);

    void SetMainServer(MainServer *ms);

    void SlaveConnected(ProgramList &slavelist);
    void SlaveDisconnected(int cardid);

    void DisableScheduling(void) { schedulingEnabled = false; }
    void EnableScheduling(void) { schedulingEnabled = true; }
    void GetNextLiveTVDir(int cardid);
    void ResetIdleTime(void);

  protected:
    void RunScheduler(void);
    static void *SchedulerThread(void *param);

  private:
    QString recordTable;
    QString priorityTable;

    void verifyCards(void);

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

    bool IsBusyRecording(const ProgramInfo *rcinfo);

    bool IsSameProgram(const ProgramInfo *a, const ProgramInfo *b) const;

    bool FindNextConflict(const RecList &cardlist,
                          const ProgramInfo *p, RecConstIter &iter,
                          bool openEnd = false) const;
    const ProgramInfo *FindConflict(const QMap<int, RecList> &reclists,
                                    const ProgramInfo *p, bool openEnd = false) const;
    void MarkOtherShowings(ProgramInfo *p);
    void MarkShowingsList(RecList &showinglist, ProgramInfo *p);
    void BackupRecStatus(void);
    void RestoreRecStatus(void);
    bool TryAnotherShowing(ProgramInfo *p,  bool samePriority,
                           bool preserveLive = false);
    void SchedNewRecords(void);
    void MoveHigherRecords(bool move_this = true);
    void SchedPreserveLiveTV(void);
    void PruneRedundants(void);
    void UpdateNextRecord(void);

    bool ChangeRecordingEnd(ProgramInfo *oldp, ProgramInfo *newp);

    void findAllScheduledPrograms(RecList &proglist);
    bool CheckShutdownServer(int prerollseconds, QDateTime &idleSince,
                             bool &blockShutdown);
    void ShutdownServer(int prerollseconds, QDateTime &idleSince);

    bool WasStartedAutomatically();

    int FillRecordingDir(ProgramInfo *pginfo, RecList& reclist);
    void FillDirectoryInfoCache(bool force = false);

    QValueList<int> reschedQueue;
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

    // Try to avoid LiveTV sessions until this time
    QDateTime livetvTime;
    int livetvpriority;
    int prefinputpri;
    QMap<QString, bool> hasLaterList;

    // cache IsSameProgram()
    typedef pair<const ProgramInfo*,const ProgramInfo*> IsSameKey;
    typedef QMap<IsSameKey,bool> IsSameCacheType;
    mutable IsSameCacheType cache_is_same_program;
};

#endif
