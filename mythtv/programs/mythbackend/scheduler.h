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

class Scheduler : public QObject
{
  public:
    Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList, 
              QSqlDatabase *ldb);
   ~Scheduler();

    bool CheckForChanges(void);
    bool FillRecordLists(bool doautoconflicts = true);
    void FillRecordListFromMaster(void);

    void FillEncoderFreeSpaceCache();

    void UpdateRecStatus(ProgramInfo *pginfo);

    list<ProgramInfo *> *getAllPending(void) { return &recordingList; }
    void getAllPending(list<ProgramInfo *> *retList);
    void getAllPending(QStringList &strList);

    list<ProgramInfo *> *getAllScheduled(void);
    void getAllScheduled(QStringList &strList);

    void getConflicting(ProgramInfo *pginfo, bool removenonplaying,
                        QStringList &strlist);

    list<ProgramInfo *> *getConflicting(ProgramInfo *pginfo,
                                        bool removenonplaying = true,
                                        list<ProgramInfo *> *uselist = NULL);

    void PrintList(bool onlyFutureRecordings = false);

    bool HasConflicts(void) { return hasconflicts; }

    void SetMainServer(MainServer *ms);

  protected:
    void RunScheduler(void);
    static void *SchedulerThread(void *param);

  private:
    void setupCards(void);

    void AddFuturePrograms(const QDateTime &now);
    void findAllScheduledPrograms(list<ProgramInfo*>& proglist);
    void MarkKnownInputs(void);
    void MarkConflicts(list<ProgramInfo *> *uselist = NULL);
    void PruneOverlaps(void);
    void PruneList(const QDateTime &now);

    void MarkConflictsToRemove(void);
    void MarkSingleConflict(ProgramInfo *info,
                            list<ProgramInfo *> *conflictList);

    void CheckRecPriority(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void CheckOverride(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void GuessSingle(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void GuessConflicts(void);

    bool Conflict(ProgramInfo *a, ProgramInfo *b);

    bool FindInOldRecordings(ProgramInfo *pginfo);      

    ProgramInfo *GetBest(ProgramInfo *info, 
                         list<ProgramInfo *> *conflictList);
    void DoMultiCard();

    void CheckShutdownServer(int prerollseconds, QDateTime &idleSince,
                             bool &blockShutdown);
    void ShutdownServer(int prerollseconds);

    list<ProgramInfo *> *CopyList(list<ProgramInfo *> *sourcelist);

    QSqlDatabase *db;

    list<ProgramInfo *> recordingList;
    list<ProgramInfo *> scheduledList;

    QMutex *recordingList_lock;
    QMutex *scheduledList_lock;

    bool doRecPriority;
    bool doRecPriorityFirst;

    bool hasconflicts;

    int numcards;
    int numsources;
    int numinputs;

    QMap<int, int> numInputsPerSource;
    QMap<int, vector<int> > sourceToInput;
    QMap<int, int> inputToCard;

    QMap<int, EncoderLink *> *m_tvList;   

    QMap<QString, bool> recPendingList;

    bool threadrunning;

    void PruneRecordList(const QDateTime &now);

    MainServer *m_mainServer;
};

#endif
