#ifndef HOUSEKEEPER_H_
#define HOUSEKEEPER_H_

#include <QDateTime>
#include <QThread>
#include <QPointer>

class Scheduler;
class QString;
class HouseKeeper;

class HKThread : public QThread
{
    Q_OBJECT
  public:
    HKThread() : m_parent(NULL) {}
    void SetParent(HouseKeeper *parent) { m_parent = parent; }
    void run(void);
  private:
    HouseKeeper *m_parent;
};

class MFDThread : public QThread
{
    Q_OBJECT
  public:
    MFDThread() : m_parent(NULL) {}
    void SetParent(HouseKeeper *parent) { m_parent = parent; }
    void run(void);
  private:
    HouseKeeper *m_parent;
};

class HouseKeeper
{
    friend class HKThread;
    friend class MFDThread;
  public:
    HouseKeeper(bool runthread, bool master, Scheduler *lsched = NULL);
   ~HouseKeeper();

  protected:
    void RunHouseKeeping(void);
    void RunMFD(void);
    static void *runMFDThread(void *param);

  private:

    bool wantToRun(const QString &dbTag, int period, int minhour, int maxhour,
                   bool nowIfPossible = false);
    void updateLastrun(const QString &dbTag);
    QDateTime getLastRun(const QString &dbTag);
    void flushLogs();
    void runFillDatabase();
    void CleanupMyOldRecordings(void);
    void CleanupAllOldInUsePrograms(void);
    void CleanupOrphanedLivetvChains(void);
    void CleanupRecordedTables(void);
    void CleanupProgramListings(void);
    void RunStartupTasks(void);
    void UpdateThemeChooserInfoCache(void);

    bool threadrunning;
    bool filldbRunning;
    bool isMaster;

    Scheduler *sched;
    HKThread  HouseKeepingThread;
    QPointer<MFDThread> FillDBThread;
};

#endif
