#ifndef SCHEDULER_H_
#define SCHEDULER_H_

class TV;
class ProgramInfo;

#include <vector>
using namespace std;

class Scheduler
{
  public:
      Scheduler(TV *ltv);
     ~Scheduler();

      bool CheckForChanges(void);
      bool FillRecordLists(void);

      void RemoveFirstRecording(); 
      ProgramInfo *GetNextRecording(void);
      
  private:
      TV *tv;

      vector<ProgramInfo *> recordingList;
};

#endif
