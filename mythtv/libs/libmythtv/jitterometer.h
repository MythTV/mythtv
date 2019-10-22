#ifndef JITTEROMETER_H
#define JITTEROMETER_H

#include <QVector>
#include <QFile>
#include "mythtvexp.h"

/* Jitterometer usage. There are 2 ways to use this:
------------------------------------------------------------------

 1.  Every 100 iterations of the for loop, RecordCycleTime() will
     print a report about the mean time to execute the loop, and
     the jitter in the recorded times.

       my_jmeter = new Jitterometer("forloop", 100);
       for ( ) {
         ... some stuff ...
         my_jmeter->RecordCycleTime();
       }

-------------------------------------------------------------------

2.  Every 42 times Weird_Operation() is run, RecordEndTime() will
    print a report about the mean time to do a Weird_Operation(), and
    the jitter in the recorded times.

      beer = new Jitterometer("weird operation", 42);
      for( ) {
         ...
         beer->RecordStartTime();
         Weird_Operation();
         beer->RecordEndTime();
         ...
      }
*/

class MTV_PUBLIC Jitterometer
{
  public:
    Jitterometer(const QString &nname, int ncycles = 0);
   ~Jitterometer();

    float GetLastFPS(void) const { return m_last_fps; }
    float GetLastSD(void) const { return m_last_sd;  }
    QString GetLastCPUStats(void) const { return m_lastcpustats; }
    void SetNumCycles(int cycles);
    bool RecordCycleTime();
    void RecordStartTime();
    bool RecordEndTime();
    QString GetCPUStat(void);

 private:
    Jitterometer(const Jitterometer &) = delete;            // not copyable
    Jitterometer &operator=(const Jitterometer &) = delete; // not copyable

    int                 m_count           {0};
    int                 m_num_cycles;
    struct timeval      m_starttime       {0,0};
    bool                m_starttime_valid {false};
    QVector<uint>       m_times; // array of cycle lengths, in uS
    float               m_last_fps        {0};
    float               m_last_sd         {0};
    QString             m_name;
    QFile              *m_cpustat         {nullptr};
    unsigned long long *m_laststats       {nullptr};
    QString             m_lastcpustats;
};

#endif // JITTEROMETER_H


