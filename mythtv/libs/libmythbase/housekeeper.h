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
#include "mythdate.h"
#include "mythevent.h"
#include "mythbaseexp.h"
#include "referencecounter.h"

class Scheduler;
class HouseKeeper;

enum HouseKeeperScope {
    kHKGlobal = 0,              ///< task should only run once per cluster
                                ///<      e.g. mythfilldatabase
    kHKLocal,                   ///< task should only run once per machine
                                ///<      e.g. hardware profile update
    kHKInst                     ///< task should run on every process
                                ///<      e.g. ServerPool update
};

enum HouseKeeperStartup {
    kHKNormal = 0,              ///< task is checked normally
    kHKRunOnStartup,            ///< task is queued when HouseKeeper is started
    kHKRunImmediateOnStartup    ///< task is run during HouseKeeper startup
};

class MBASE_PUBLIC HouseKeeperTask : public ReferenceCounter
{
  public:
    HouseKeeperTask(const QString &dbTag, HouseKeeperScope scope=kHKGlobal,
                    HouseKeeperStartup startup=kHKNormal);
   ~HouseKeeperTask() {}

    bool            CheckRun(QDateTime now);
    bool            Run(void);
    bool            ConfirmRun(void)                { return m_confirm;     }
    bool            IsRunning(void)                 { return m_running;     }

    bool            CheckImmediate(void);
    bool            CheckStartup(void);

    QString         GetTag(void)                    { return m_dbTag;       }
    QDateTime       GetLastRun(void)                { return m_lastRun;     }
    HouseKeeperScope    GetScope(void)              { return m_scope;       }
    QDateTime       QueryLastRun(void);
    QDateTime       UpdateLastRun(void)
                           { return UpdateLastRun(MythDate::current());     }
    virtual QDateTime UpdateLastRun(QDateTime last);
    virtual void    SetLastRun(QDateTime last)      { m_lastRun = last;
                                                      m_confirm = false;    }

    virtual bool    DoCheckRun(QDateTime now)       { return false;         }
    virtual bool    DoRun(void)                     { return false;         }

    virtual void    Terminate(void)                 {}

  private:

    QString             m_dbTag;
    bool                m_confirm;
    HouseKeeperScope    m_scope;
    HouseKeeperStartup  m_startup;
    bool                m_running;

    QDateTime   m_lastRun;
};

class MBASE_PUBLIC PeriodicHouseKeeperTask : public HouseKeeperTask
{
  public:
    PeriodicHouseKeeperTask(const QString &dbTag, int period, float min=0.5,
                            float max=1.1, HouseKeeperScope scope=kHKGlobal,
                            HouseKeeperStartup startup=kHKNormal);
    virtual bool DoCheckRun(QDateTime now);
    virtual bool InWindow(QDateTime now);
    virtual bool PastWindow(QDateTime now);
    virtual QDateTime UpdateLastRun(QDateTime last);
    virtual void SetLastRun(QDateTime last);
    virtual void SetWindow(float min, float max);

  protected:
    virtual void CalculateWindow(void);

    int                         m_period;
    QPair<float,float>          m_windowPercent;
    QPair<int,int>              m_windowElapsed;
    float                       m_currentProb;
};

class MBASE_PUBLIC DailyHouseKeeperTask : public PeriodicHouseKeeperTask
{
  public:
    DailyHouseKeeperTask(const QString &dbTag,
                         HouseKeeperScope scope=kHKGlobal,
                         HouseKeeperStartup startup=kHKNormal);
    DailyHouseKeeperTask(const QString &dbTag, int minhour, int maxhour,
                         HouseKeeperScope scope=kHKGlobal,
                         HouseKeeperStartup startup=kHKNormal);
    virtual void SetHourWindow(int min, int max);
    virtual bool InWindow(QDateTime now);

  protected:
    virtual void CalculateWindow(void);

  private:
    QPair<int, int> m_windowHour;
};

class HouseKeepingThread : public MThread
{
  public:
    HouseKeepingThread(HouseKeeper *p) :
        MThread("HouseKeeping"), m_idle(true), m_keepRunning(true),
        m_parent(p) {}
   ~HouseKeepingThread() {}
    virtual void run(void);
    void Discard(void)                  { m_keepRunning = false;        }
    bool isIdle(void)                   { return m_idle;                }
    void Wake(void)                     { m_waitCondition.wakeAll();    }

    void Terminate(void);

  private:
    bool                m_idle;
    bool                m_keepRunning;
    HouseKeeper        *m_parent;
    QMutex              m_waitMutex;
    QWaitCondition      m_waitCondition;
};

class MBASE_PUBLIC HouseKeeper : public QObject
{
    Q_OBJECT

  public:
    HouseKeeper(void);
   ~HouseKeeper();

    void RegisterTask(HouseKeeperTask *task);
    void Start(void);
    void StartThread(void);
    HouseKeeperTask* GetQueuedTask(void);

    void customEvent(QEvent *e);

  public slots:
    void Run(void);

  private:
    QTimer                         *m_timer;

    QQueue<HouseKeeperTask*>        m_taskQueue;
    QMutex                          m_queueLock;

    QMap<QString, HouseKeeperTask*> m_taskMap;
    QMutex                          m_mapLock;

    QList<HouseKeepingThread*>      m_threadList;
    QMutex                          m_threadLock;
};

#endif
