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
              QSqlDatabase *ldb, bool noAutoShutdown = false);
   ~Scheduler();

    bool CheckForChanges(void);
    bool FillRecordLists(bool doautoconflicts = true);
    void FillRecordListFromMaster(void);

    void FillEncoderFreeSpaceCache();

    void RemoveRecording(ProgramInfo *pginfo);

    list<ProgramInfo *> *getAllPending(void) { return &recordingList; }
    void getAllPending(list<ProgramInfo *> *retList);
    void getAllPending(QStringList *strList);

    list<ProgramInfo *> *getAllScheduled(void);
    void getAllScheduled(QStringList *strList);

    void getConflicting(ProgramInfo *pginfo,
                        bool removenonplaying,
                        QStringList *strlist);
    list<ProgramInfo *> *getConflicting(ProgramInfo *pginfo,
                                        bool removenonplaying = true,
                                        list<ProgramInfo *> *uselist = NULL);

    void PrintList(void);

    bool HasConflicts(void) { return hasconflicts; }

    void SetMainServer(MainServer *ms);

  protected:
    void RunScheduler(void);
    static void *SchedulerThread(void *param);

  private:
    void setupCards(void);

    void MarkKnownInputs(void);
    void MarkConflicts(list<ProgramInfo *> *uselist = NULL);
    void PruneOverlaps(void);
    void PruneList(void);

    void MarkConflictsToRemove(void);
    void MarkSingleConflict(ProgramInfo *info,
                            list<ProgramInfo *> *conflictList);

    int totalRecPriority(ProgramInfo *info);
    void CheckRecPriority(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void CheckOverride(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void RemoveConflicts(void);
    void GuessSingle(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void GuessConflicts(void);

    bool Conflict(ProgramInfo *a, ProgramInfo *b);

    bool FindInOldRecordings(ProgramInfo *pginfo);      

    ProgramInfo *GetBest(ProgramInfo *info, 
                         list<ProgramInfo *> *conflictList);
    void DoMultiCard();

    void ShutdownServer(int prerollseconds);

    list<ProgramInfo *> *CopyList(list<ProgramInfo *> *sourcelist);

    QSqlDatabase *db;

    list<ProgramInfo *> recordingList;
    list<ProgramInfo *> scheduledList;

    QMutex *recordingList_lock;
    QMutex *scheduledList_lock;

    bool doRecPriority;
    bool doRecPriorityFirst;

    QMap<QString, int> recpriorityMap;
    QMap<QString, int> channelRecPriorityMap;
    QMap<RecordingType, int> recTypeRecPriorityMap;

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

    void AddToDontRecord(ProgramInfo *pginfo);
    void PruneDontRecords(void);

    QValueList<ProgramInfo> dontRecordList;

    MainServer *m_mainServer;
    bool m_blockShutdown;
    bool m_noAutoShutdown;
};

#endif
