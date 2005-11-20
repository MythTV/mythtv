// -*- Mode: c++ -*-
#ifndef EITSCANNER_H
#define EITSCANNER_H

// C includes
#include <pthread.h>

// Qt includes
#include <qobject.h>
#include <qdatetime.h>
#include <qstringlist.h>
#include <qwaitcondition.h>

class TVRec;
class DVBChannel;
class DVBSIParser;
class EITHelper;
class dvb_channel_t;
class PMTObject;

class EITScanner : public QObject
{
    Q_OBJECT
  public:
    EITScanner();
    ~EITScanner() { TeardownAll(); }

    void StartPassiveScan(DVBChannel*, DVBSIParser*);
    void StopPassiveScan(void);

    void StartActiveScan(TVRec*, uint max_seconds_per_source);
    void StopActiveScan(void);        

  public slots:
    void SetPMTObject(const PMTObject*);
    void deleteLater(void);

  private:
    void TeardownAll(void);
    void RunEventLoop(void);
    static void *SpawnEventLoop(void*);
    static void RescheduleRecordings(void);

    DVBChannel      *channel;
    DVBSIParser     *parser;
    EITHelper       *eitHelper;
    pthread_t        eventThread;
    bool             exitThread;
    QWaitCondition   exitThreadCond;

    TVRec           *rec;
    bool             activeScan;
    QDateTime        activeScanNextTrig;
    uint             activeScanTrigTime;
    QStringList      activeScanChannels;
    QStringList::iterator activeScanNextChan;

    static QMutex    resched_lock;
    static QDateTime resched_next_time;

    /// Minumum number of seconds between reschedules.
    static const uint kMinRescheduleInterval;
};

#endif // EITSCANNER_H
