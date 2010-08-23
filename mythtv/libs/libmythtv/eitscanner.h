// -*- Mode: c++ -*-
#ifndef EITSCANNER_H
#define EITSCANNER_H

// Qt includes
#include <QWaitCondition>
#include <QStringList>
#include <QDateTime>
#include <QThread>
#include <QMutex>

class TVRec;
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

class EITThread : public QThread
{
  public:
    virtual void run();
    EITScanner   *scanner;
};

class EITScanner
{
  friend class EITThread;

  public:
    EITScanner(uint cardnum);
    ~EITScanner() { TeardownAll(); }

    void StartPassiveScan(ChannelBase*, EITSource*);
    void StopPassiveScan(void);

    void StartActiveScan(TVRec*, uint max_seconds_per_source);

    void StopActiveScan(void);

  protected:
    void RunEventLoop(void);

  private:
    void TeardownAll(void);
    static void *SpawnEventLoop(void*);
    static void RescheduleRecordings(void);

    QMutex           lock;
    ChannelBase     *channel;
    EITSource       *eitSource;

    EITHelper       *eitHelper;
    EITThread        eventThread;
    bool             exitThread;
    QWaitCondition   exitThreadCond;

    TVRec           *rec;
    bool             activeScan;
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
