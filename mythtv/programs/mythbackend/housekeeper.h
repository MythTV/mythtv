#ifndef HOUSEKEEPER_H_
#define HOUSEKEEPER_H_

#include <QWaitCondition>
#include <QDateTime>
#include <QMutex>

#include "mthread.h"

class Scheduler;
class QString;
class HouseKeeper;
class MythSystemLegacy;

class HouseKeepingThread : public MThread
{
  public:
    HouseKeepingThread(HouseKeeper *p) :
        MThread("HouseKeeping"), m_parent(p) {}
    ~HouseKeepingThread() { wait(); }
    virtual void run(void);
  private:
    HouseKeeper *m_parent;
};

class MythFillDatabaseThread : public MThread
{
  public:
    MythFillDatabaseThread(HouseKeeper *p) :
        MThread("MythFillDB"), m_parent(p) {}
    ~MythFillDatabaseThread() { wait(); }
    static void setTerminationEnabled(bool v)
        { MThread::setTerminationEnabled(v); }
    virtual void run(void);
  private:
    HouseKeeper *m_parent;
};

class HouseKeeper
{
    friend class HouseKeepingThread;
    friend class MythFillDatabaseThread;
  public:
    HouseKeeper(bool runthread, bool master, Scheduler *lsched = NULL);
   ~HouseKeeper();

  protected:
    void RunHouseKeeping(void);
    void RunMFD(void);

  private:

    bool wantToRun(const QString &dbTag, int period, int minhour, int maxhour,
                   bool nowIfPossible = false);
    void updateLastrun(const QString &dbTag);
    QDateTime getLastRun(const QString &dbTag);
    void flushDBLogs();
    void StartMFD(void);
    void KillMFD(void);
    void CleanupMyOldRecordings(void);
    void CleanupAllOldInUsePrograms(void);
    void CleanupOrphanedLivetvChains(void);
    void CleanupRecordedTables(void);
    void CleanupProgramListings(void);
    void RunStartupTasks(void);
    void UpdateThemeChooserInfoCache(void);
    void UpdateRecordedArtwork(void);

  private:
    bool                    isMaster;
    Scheduler              *sched;

    QMutex                  houseKeepingLock;
    QWaitCondition          houseKeepingWait;  // protected by houseKeepingLock
    bool                    houseKeepingRun;   // protected by houseKeepingLock
    HouseKeepingThread     *houseKeepingThread;// protected by houseKeepingLock

    QMutex                  fillDBLock;
    QWaitCondition          fillDBWait;        // protected by fillDBLock
    MythFillDatabaseThread *fillDBThread;      // Only mod in HouseKeepingThread
    bool                    fillDBStarted;     // protected by fillDBLock
    MythSystemLegacy             *fillDBMythSystemLegacy;  // protected by fillDBLock
};

#endif
