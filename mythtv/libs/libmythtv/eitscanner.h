// -*- Mode: c++ -*-
#ifndef EITSCANNER_H
#define EITSCANNER_H

// Qt includes
#include <QWaitCondition>
#include <QStringList>
#include <QDateTime>
#include <QRunnable>
#include <QMutex>

// MythTV
#include "mythrandom.h"

class TVRec;
class MThread;
class ChannelBase;
class DVBSIParser;
class EITHelper;
class ProgramMapTable;

class EITSource
{
  protected:
    virtual ~EITSource() = default;
  public:
    virtual void SetEITHelper(EITHelper*) = 0;
    virtual void SetEITRate(float rate) = 0;
};

class EITScanner;

class EITScanner : public QRunnable
{
  public:
    explicit EITScanner(uint cardnum);
    ~EITScanner() override { TeardownAll(); }

    void StartPassiveScan(ChannelBase *channel, EITSource *eitSource);
    void StopPassiveScan(void);

    void StartActiveScan(TVRec *rec, std::chrono::seconds max_seconds_per_source);
    void StopActiveScan(void);

  protected:
    void run(void) override; // QRunnable

  private:
    void TeardownAll(void);
    static void *SpawnEventLoop(void*);
           void  RescheduleRecordings(void);

    QMutex                m_lock;
    ChannelBase          *m_channel                 {nullptr};
    EITSource            *m_eitSource               {nullptr};

    EITHelper            *m_eitHelper               {nullptr};
    MThread              *m_eventThread             {nullptr};
    volatile bool         m_exitThread              {false};
    QWaitCondition        m_exitThreadCond;                     // protected by lock

    TVRec                *m_rec                     {nullptr};
    volatile bool         m_activeScan              {false};
    volatile bool         m_activeScanStopped       {true};     // protected by lock
    QWaitCondition        m_activeScanCond;                     // protected by lock
    QDateTime             m_activeScanNextTrig;
    std::chrono::seconds  m_activeScanTrigTime      {0s};
    QStringList           m_activeScanChannels;
    QStringList::iterator m_activeScanNextChan;
    uint                  m_activeScanNextChanIndex {MythRandom()};

    uint                  m_cardnum;

    static QMutex         s_resched_lock;
    static QDateTime      s_resched_next_time;

};

#endif // EITSCANNER_H
