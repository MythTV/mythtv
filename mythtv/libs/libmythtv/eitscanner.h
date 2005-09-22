// -*- Mode: c++ -*-
#ifndef EITSCANNER_H
#define EITSCANNER_H

// C includes
#include <pthread.h>

// Qt includes
#include <qobject.h>

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
    ~EITScanner();

    void StartPassiveScan(DVBChannel*, DVBSIParser*);
    void StopPassiveScan(void);

  public slots:
    void SetPMTObject(const PMTObject*);

  private:
    void RunEventLoop(void);
    static void *SpawnEventLoop(void*);

    DVBChannel      *channel;
    DVBSIParser     *parser;
    EITHelper       *eitHelper;
    pthread_t        eventThread;
    bool             exitThread;
};

#endif // EITSCANNER_H
