#ifndef SCHEDULER_H_
#define SCHEDULER_H_

class ProgramInfo;
class QSqlDatabase;

#include <vector>
using namespace std;

class Scheduler
{
  public:
      Scheduler(QSqlDatabase *ldb);
     ~Scheduler();

      bool CheckForChanges(void);
      bool FillRecordLists(void);

      void RemoveFirstRecording(); 
      ProgramInfo *GetNextRecording(void);
      
  private:
      QSqlDatabase *db;

      vector<ProgramInfo *> recordingList;
};

#endif
