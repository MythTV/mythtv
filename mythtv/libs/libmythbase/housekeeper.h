#ifndef HOUSEKEEPER_H_
#define HOUSEKEEPER_H_

#include <QWaitCondition>
#include <QDateTime>
#include <QString>
#include <QEvent>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QList>
#include <QMap>
#include <QPair>

#include "mthread.h"
#include "mythchrono.h"
#include "mythdate.h"
#include "mythevent.h"
#include "mythbaseexp.h"
#include "referencecounter.h"

class Scheduler;
class HouseKeeper;

enum HouseKeeperScope : std::uint8_t {
    kHKGlobal = 0,              ///< task should only run once per cluster
                                ///<      e.g. mythfilldatabase
    kHKLocal,                   ///< task should only run once per machine
                                ///<      e.g. hardware profile update
    kHKInst                     ///< task should run on every process
                                ///<      e.g. ServerPool update
};

enum HouseKeeperStartup : std::uint8_t {
    kHKNormal = 0,              ///< task is checked normally
    kHKRunOnStartup,            ///< task is queued when HouseKeeper is started
    kHKRunImmediateOnStartup    ///< task is run during HouseKeeper startup
};

class MBASE_PUBLIC HouseKeeperTask : public ReferenceCounter
{
  public:
    explicit HouseKeeperTask(const QString &dbTag, HouseKeeperScope scope=kHKGlobal,
                    HouseKeeperStartup startup=kHKNormal);
   ~HouseKeeperTask() override = default;

    bool            CheckRun(const QDateTime& now);
    bool            Run(void);
    bool            ConfirmRun(void) const          { return m_confirm;     }
    bool            IsRunning(void) const           { return m_running;     }

    bool            CheckImmediate(void);
    bool            CheckStartup(void);

    QString         GetTag(void)                    { return m_dbTag;       }
    QDateTime       GetLastRun(void)                { return m_lastRun;     }
    QDateTime       GetLastSuccess(void)            { return m_lastSuccess; }
    HouseKeeperScope    GetScope(void)              { return m_scope;       }
    QDateTime       QueryLastRun(void);
    QDateTime       QueryLastSuccess(void);
    QDateTime       UpdateLastRun(bool successful=true)
                   { return UpdateLastRun(MythDate::current(), successful); }
    virtual QDateTime UpdateLastRun(const QDateTime& last, bool successful=true);
    virtual void    SetLastRun(const QDateTime& last, bool successful=true);

    virtual bool    DoCheckRun(const QDateTime& /*now*/) { return false;         }
    virtual bool    DoRun(void)                     { return false;         }

    virtual void    Terminate(void)                 {}

  private:
    void            QueryLast(void);

    QString             m_dbTag;
    bool                m_confirm {false};
    HouseKeeperScope    m_scope;
    HouseKeeperStartup  m_startup;
    bool                m_running {false};

    QDateTime   m_lastRun;
    QDateTime   m_lastSuccess;
    QDateTime   m_lastUpdate;
};

class MBASE_PUBLIC PeriodicHouseKeeperTask : public HouseKeeperTask
{
  public:
    PeriodicHouseKeeperTask(const QString &dbTag, std::chrono::seconds period, float min=0.5,
                            float max=1.1, std::chrono::seconds retry=0s, HouseKeeperScope scope=kHKGlobal,
                            HouseKeeperStartup startup=kHKNormal);
    bool DoCheckRun(const QDateTime& now) override; // HouseKeeperTask
    virtual bool InWindow(const QDateTime& now);
    virtual bool PastWindow(const QDateTime& now);
    QDateTime UpdateLastRun(const QDateTime& last, bool successful=true) override; // HouseKeeperTask
    void SetLastRun(const QDateTime& last, bool successful=true) override; // HouseKeeperTask
    virtual void SetWindow(float min, float max);

  protected:
    virtual void CalculateWindow(void);

    std::chrono::seconds        m_period;
    std::chrono::seconds        m_retry;
    QPair<float,float>          m_windowPercent;
    QPair<std::chrono::seconds,std::chrono::seconds> m_windowElapsed;
    double                      m_currentProb { 1.0 };
};

class MBASE_PUBLIC DailyHouseKeeperTask : public PeriodicHouseKeeperTask
{
  public:
    explicit DailyHouseKeeperTask(const QString &dbTag,
                         HouseKeeperScope scope=kHKGlobal,
                         HouseKeeperStartup startup=kHKNormal);
    DailyHouseKeeperTask(const QString &dbTag,
                         std::chrono::hours minhour, std::chrono::hours maxhour,
                         HouseKeeperScope scope=kHKGlobal,
                         HouseKeeperStartup startup=kHKNormal);
    virtual void SetHourWindow(std::chrono::hours min, std::chrono::hours max);
    bool InWindow(const QDateTime& now) override; // PeriodicHouseKeeperTask

  protected:
    void CalculateWindow(void) override; // PeriodicHouseKeeperTask

  private:
    QPair<std::chrono::hours, std::chrono::hours> m_windowHour;
};

class HouseKeepingThread : public MThread
{
  public:
    explicit HouseKeepingThread(HouseKeeper *p) :
        MThread("HouseKeeping"), m_parent(p) {}
   ~HouseKeepingThread() override = default;
    void run(void) override; // MThread
    void Discard(void)                  { m_keepRunning = false;        }
    bool isIdle(void) const             { return m_idle;                }
    void Wake(void)                     { m_waitCondition.wakeAll();    }

    void Terminate(void);

  private:
    bool                m_idle        { true };
    bool                m_keepRunning { true };
    HouseKeeper        *m_parent      { nullptr };
    QMutex              m_waitMutex;
    QWaitCondition      m_waitCondition;
};

class MBASE_PUBLIC HouseKeeper : public QObject
{
    Q_OBJECT

  public:
    HouseKeeper(void);
   ~HouseKeeper() override;

    void RegisterTask(HouseKeeperTask *task);
    void Start(void);
    void StartThread(void);
    HouseKeeperTask* GetQueuedTask(void);

    void customEvent(QEvent *e) override; // QObject

  public slots:
    void Run(void);

  private:
    QTimer                         *m_timer { nullptr };

    QQueue<HouseKeeperTask*>        m_taskQueue;
    QMutex                          m_queueLock;

    QMap<QString, HouseKeeperTask*> m_taskMap;
    QMutex                          m_mapLock;

    QList<HouseKeepingThread*>      m_threadList;
    QMutex                          m_threadLock;
};

#endif
