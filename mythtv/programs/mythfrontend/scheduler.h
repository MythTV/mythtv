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
      bool FillRecordLists(void);

      void RemoveFirstRecording(void ); 
      ProgramInfo *GetNextRecording(void);

      list<ProgramInfo *> *getAllPending(void) { return &recordingList; }

  private:
      void MarkConflicts(void);
      void PruneList(void);

      bool Conflict(ProgramInfo *a, ProgramInfo *b);

      QSqlDatabase *db;

      list<ProgramInfo *> recordingList;

      bool hasconflicts;
};

#endif
