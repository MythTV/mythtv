#ifndef SCHEDULER_H_
#define SCHEDULER_H_

class ProgramInfo;
class QSqlDatabase;
class EncoderLink;

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

    void RemoveRecording(ProgramInfo *pginfo);

    list<ProgramInfo *> *getAllPending(void) { return &recordingList; }
    void getAllPending(list<ProgramInfo *> *retList);

    list<ProgramInfo *> *getAllScheduled(void);
    void getAllScheduled(list<ProgramInfo *> *retList);

    list<ProgramInfo *> *getConflicting(ProgramInfo *pginfo,
                                        bool removenonplaying = true,
                                        list<ProgramInfo *> *uselist = NULL);

    void PrintList(void);

    bool HasConflicts(void) { return hasconflicts; }

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

    int totalRank(ProgramInfo *info);
    void CheckRank(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void CheckOverride(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void RemoveConflicts(void);
    void GuessSingle(ProgramInfo *info, list<ProgramInfo *> *conflictList);
    void GuessConflicts(void);

    bool Conflict(ProgramInfo *a, ProgramInfo *b);

    bool FindInOldRecordings(ProgramInfo *pginfo);      

    ProgramInfo *GetBest(ProgramInfo *info, 
                         list<ProgramInfo *> *conflictList);
    void DoMultiCard();

    list<ProgramInfo *> *CopyList(list<ProgramInfo *> *sourcelist);

    QSqlDatabase *db;

    list<ProgramInfo *> recordingList;
    list<ProgramInfo *> scheduledList;

    QMutex recordingList_lock;
    QMutex scheduledList_lock;

    bool doRank;
    bool doRankFirst;

    QMap<QString, int> rankMap;
    QMap<QString, int> channelRankMap;
    QMap<RecordingType, int> recTypeRankMap;

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
};

#endif
