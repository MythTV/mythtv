#ifndef HOUSEKEEPER_H_
#define HOUSEKEEPER_H_

#include <QWaitCondition>
#include <QDateTime>
#include <QThread>
#include <QMutex>

class Scheduler;
class QString;
class HouseKeeper;
class MythSystem;

class HouseKeepingThread : public QThread
{
    Q_OBJECT
  public:
    HouseKeepingThread(HouseKeeper *p) : m_parent(p) {}
    ~HouseKeepingThread() { wait(); }
    inline virtual void run(void);
  private:
    HouseKeeper *m_parent;
};

class MythFillDatabaseThread : public QThread
{
    Q_OBJECT
  public:
    MythFillDatabaseThread(HouseKeeper *p) : m_parent(p) {}
    ~MythFillDatabaseThread() { wait(); }
    virtual void setTerminationEnabled(bool v)
        { QThread::setTerminationEnabled(v); }
    inline virtual void run(void);
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
    void flushLogs();
    void StartMFD(void);
    void KillMFD(void);
    void CleanupMyOldRecordings(void);
    void CleanupAllOldInUsePrograms(void);
    void CleanupOrphanedLivetvChains(void);
    void CleanupRecordedTables(void);
    void CleanupProgramListings(void);
    void RunStartupTasks(void);
    void UpdateThemeChooserInfoCache(void);

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
    MythSystem             *fillDBMythSystem;  // protected by fillDBLock
};

inline void HouseKeepingThread::run(void)
{
    m_parent->RunHouseKeeping();
}

inline void MythFillDatabaseThread::run(void)
{
    m_parent->RunMFD();
}

#endif
