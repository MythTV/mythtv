#ifndef SCHEDULER_H_
#define SCHEDULER_H_

class ProgramInfo;
class QSqlDatabase;
class EncoderLink;
class MainServer;

#include <qmutex.h>
#include <qmap.h> 
#include <list>
#include <vector>
#include <qobject.h>

#include "scheduledrecording.h"

using namespace std;

typedef list<ProgramInfo *> RecList;
typedef RecList::iterator RecIter;

class Scheduler : public QObject
{
  public:
    Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList, 
              QSqlDatabase *ldb);
   ~Scheduler();

    bool CheckForChanges(void);
    bool FillRecordLists(void);
    void FillRecordListFromMaster(void);

    void FillEncoderFreeSpaceCache(void);

    void UpdateRecStatus(ProgramInfo *pginfo);

    RecList *getAllPending(void) { return &reclist; }
    void getAllPending(RecList *retList);
    void getAllPending(QStringList &strList);

    RecList *getAllScheduled(void);
    void getAllScheduled(QStringList &strList);

    void getConflicting(ProgramInfo *pginfo, QStringList &strlist);
    RecList *getConflicting(ProgramInfo *pginfo);

    void PrintList(bool onlyFutureRecordings = false);
    void PrintRec(ProgramInfo *p, const char *prefix = NULL);

    bool HasConflicts(void) { return hasconflicts; }

    void SetMainServer(MainServer *ms);

  protected:
    void RunScheduler(void);
    static void *SchedulerThread(void *param);

  private:
    void verifyCards(void);

    void PruneOldRecords(void);
    void AddNewRecords(void);
    bool FindNextConflict(ProgramInfo *p, RecIter &iter);
    void MarkOtherShowings(ProgramInfo *p);
    void BackupRecStatus(void);
    void RestoreRecStatus(void);
    bool TryAnotherShowing(RecIter iter);
    void SchedNewRecords(void);
    void MoveHigherRecords(void);
    void PruneRedundants(void);

    void findAllScheduledPrograms(list<ProgramInfo *> &proglist);
    void CheckShutdownServer(int prerollseconds, QDateTime &idleSince,
                             bool &blockShutdown);
    void ShutdownServer(int prerollseconds);

    QSqlDatabase *db;

    RecList reclist;
    RecList retrylist;
    RecList schedlist;

    QMutex *reclist_lock;
    QMutex *schedlist_lock;

    bool hasconflicts;
    bool schedMoveHigher;

    QMap<int, EncoderLink *> *m_tvList;   

    QMap<QString, bool> recPendingList;

    bool threadrunning;

    MainServer *m_mainServer;
};

#endif
