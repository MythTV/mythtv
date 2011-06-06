#ifndef JITTEROMETER_H
#define JITTEROMETER_H

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

class Jitterometer
{
  public:
    Jitterometer(QString nname, int num_cycles = 0);
   ~Jitterometer();

    float GetLastFPS(void) { return last_fps; }
    void SetNumCycles(int cycles);
    bool RecordCycleTime();
    void RecordStartTime();
    bool RecordEndTime();

 private:
    int count;
    int num_cycles;
    struct timeval starttime;
    int starttime_valid;
    unsigned *times; // array of cycle lengths, in uS
    float last_fps;
    QString name;
};

#endif // JITTEROMETER_H


