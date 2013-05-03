// -*- Mode: c++ -*-
#ifndef EITSCANNER_H
#define EITSCANNER_H

// Qt includes
#include <QWaitCondition>
#include <QStringList>
#include <QDateTime>
#include <QRunnable>
#include <QMutex>

class TVRec;
class MThread;
class ChannelBase;
class DVBSIParser;
class EITHelper;
class ProgramMapTable;

class EITSource
{
  protected:
    virtual ~EITSource() {}
  public:
    virtual void SetEITHelper(EITHelper*) = 0;
    virtual void SetEITRate(float rate) = 0;
};

class EITScanner;

class EITScanner : public QRunnable
{
  public:
    EITScanner(uint cardnum);
    ~EITScanner() { TeardownAll(); }

    void StartPassiveScan(ChannelBase*, EITSource*);
    void StopPassiveScan(void);

    void StartActiveScan(TVRec*, uint max_seconds_per_source);

    void StopActiveScan(void);

  protected:
    void run(void);

  private:
    void TeardownAll(void);
    static void *SpawnEventLoop(void*);
           void  RescheduleRecordings(void);

    QMutex           lock;
    ChannelBase     *channel;
    EITSource       *eitSource;

    EITHelper       *eitHelper;
    MThread         *eventThread;
    volatile bool    exitThread;
    QWaitCondition   exitThreadCond; // protected by lock

    TVRec           *rec;
    volatile bool    activeScan;
    volatile bool    activeScanStopped; // protected by lock
    QWaitCondition   activeScanCond; // protected by lock
    QDateTime        activeScanNextTrig;
    uint             activeScanTrigTime;
    QStringList      activeScanChannels;
    QStringList::iterator activeScanNextChan;

    uint             cardnum;

    static QMutex    resched_lock;
    static QDateTime resched_next_time;

    /// Minumum number of seconds between reschedules.
    static const uint kMinRescheduleInterval;
};

#endif // EITSCANNER_H
