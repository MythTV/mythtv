#ifndef SCHEDULER_H_
#define SCHEDULER_H_

class ProgramInfo;
class QSqlDatabase;
 
#include <list>
using namespace std;

class Scheduler
{
  public:
      Scheduler(QSqlDatabase *ldb);
     ~Scheduler();

      bool CheckForChanges(void);
      bool FillRecordLists(bool doautoconflicts = true);

      void RemoveFirstRecording(void ); 
      ProgramInfo *GetNextRecording(void);

      list<ProgramInfo *> *getAllPending(void) { return &recordingList; }

      list<ProgramInfo *> *getConflicting(ProgramInfo *pginfo,
                                          bool removenonplaying = true);

  private:
      void MarkConflicts(void);
      void PruneList(void);

      void MarkConflictsToRemove(void);
      void MarkSingleConflict(ProgramInfo *info,
                              list<ProgramInfo *> *conflictList);
      void CheckOverride(ProgramInfo *info, list<ProgramInfo *> *conflictList);
      void RemoveConflicts(void);

      void GuessSingle(ProgramInfo *info, list<ProgramInfo *> *conflictList);
      void GuessConflicts(void);

      bool Conflict(ProgramInfo *a, ProgramInfo *b);

      bool FindInOldRecordings(ProgramInfo *pginfo);      

      QSqlDatabase *db;

      list<ProgramInfo *> recordingList;

      bool hasconflicts;
};

#endif
