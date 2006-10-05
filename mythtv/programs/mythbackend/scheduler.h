#ifndef SCHEDULER_H_
#define SCHEDULER_H_

class EncoderLink;
class MainServer;

#include <qmutex.h>
#include <qwaitcondition.h>
#include <qmap.h> 
#include <list>
#include <vector>
#include <qobject.h>

#include "scheduledrecording.h"
#include "programinfo.h"

using namespace std;

typedef list<ProgramInfo *> RecList;
typedef RecList::iterator RecIter;

class Scheduler : public QObject
{
  public:
    Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList,
              QString recordTbl = "record", Scheduler *master_sched = NULL);
    ~Scheduler();

    void Reschedule(int recordid);
    void AddRecording(const ProgramInfo&);
    void FillRecordListFromDB(int recordid = -1);
    void FillRecordListFromMaster(void);

    void FillEncoderFreeSpaceCache(void);

    void UpdateRecStatus(ProgramInfo *pginfo);
    void UpdateRecStatus(int cardid, const QString &chanid, 
                         const QDateTime &startts, RecStatusType recstatus, 
                         const QDateTime &recendts);

    void getAllPending(RecList *retList);
    void getAllPending(QStringList &strList);

    void getAllScheduled(QStringList &strList);

    void getConflicting(ProgramInfo *pginfo, QStringList &strlist);
    RecList *getConflicting(ProgramInfo *pginfo);

    void PrintList(bool onlyFutureRecordings = false);
    void PrintRec(ProgramInfo *p, const char *prefix = NULL);

    bool HasConflicts(void) { return hasconflicts; }

    void SetMainServer(MainServer *ms);

    void SlaveConnected(ProgramList &slavelist);
    void SlaveDisconnected(int cardid);

    void DisableScheduling(void) { schedulingEnabled = false; }
    void EnableScheduling(void) { schedulingEnabled = true; }

  protected:
    void RunScheduler(void);
    static void *SchedulerThread(void *param);

  private:
    QString recordTable;

    void verifyCards(void);

    bool FillRecordList(void);
    void UpdateMatches(int recordid);
    void UpdateManuals(int recordid);
    void PruneOldRecords(void);
    void AddNewRecords(void);
    void AddNotListed(void);
    void BuildNewRecordsQueries(int recordid, QStringList &from, QStringList &where,
                                MSqlBindings &bindings);
    void PruneOverlaps(void);
    void BuildListMaps(void);
    void ClearListMaps(void);
    bool FindNextConflict(RecList &cardlist, ProgramInfo *p, RecIter &iter);
    void MarkOtherShowings(ProgramInfo *p);
    void MarkShowingsList(RecList &showinglist, ProgramInfo *p);
    void BackupRecStatus(void);
    void RestoreRecStatus(void);
    bool TryAnotherShowing(ProgramInfo *p);
    void SchedNewRecords(void);
    void MoveHigherRecords(void);
    void PruneRedundants(void);
    void UpdateNextRecord(void);

    bool ChangeRecordingEnd(ProgramInfo *oldp, ProgramInfo *newp);

    void findAllScheduledPrograms(list<ProgramInfo *> &proglist);
    bool CheckShutdownServer(int prerollseconds, QDateTime &idleSince,
                             bool &blockShutdown);
    void ShutdownServer(int prerollseconds);


    QValueList<int> reschedQueue;
    QMutex reschedLock;
    QMutex recordmatchLock;
    QWaitCondition reschedWait;
    RecList reclist;
    RecList retrylist;
    QMap<int, RecList> cardlistmap;
    QMap<int, RecList> recordidlistmap;
    QMap<QString, RecList> titlelistmap;

    QMutex *reclist_lock;

    bool specsched;
    bool hasconflicts;
    bool schedMoveHigher;
    bool schedulingEnabled;

    QMap<int, EncoderLink *> *m_tvList;   

    QMap<QString, bool> recPendingList;

    pthread_t schedThread;
    bool threadrunning;

    MainServer *m_mainServer;

    bool m_isShuttingDown;
    MSqlQueryInfo dbConn;

};

#endif
