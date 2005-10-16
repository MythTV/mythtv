// -*- Mode: c++ -*-
#ifndef PREVIEW_GENERATOR_H_
#define PREVIEW_GENERATOR_H_

#include <pthread.h>

#include <qstring.h>
#include <qmutex.h>
#include <qsocket.h>

#include "programinfo.h"
#include "util.h"

class PreviewGenerator : public QObject
{
    Q_OBJECT
  public:
    PreviewGenerator(const ProgramInfo *pginfo)
        : programInfo(*pginfo),
          eventSock(new QSocket(0, "preview event socket")),
          serverSock(NULL)
    {
        QObject::connect(eventSock, SIGNAL(connected()), 
                         this,      SLOT(  EventSocketConnected()));
        QObject::connect(eventSock, SIGNAL(readyRead()), 
                         this,      SLOT(  EventSocketRead()));
        QObject::connect(eventSock, SIGNAL(connectionClosed()), 
                         this,      SLOT(  EventSocketClosed()));
    }

    void Start(void)
    {
        pthread_create(&previewThread, NULL, PreviewRun, this);
        pthread_detach(previewThread);
    }

    virtual ~PreviewGenerator()
    {
        QMutexLocker locker(&previewLock);
        const QString filename = programInfo.pathname + ".png";
        emit previewThreadDone(filename);
    }

    static void *PreviewGenerator::PreviewRun(void *param)
    {
        PreviewGenerator *gen = (PreviewGenerator*) param;
        gen->Run();
        gen->deleteLater();
        return NULL;
    }

    void Run(void);

  signals:
    void previewThreadDone(const QString&);
    void previewReady(const ProgramInfo*);

  protected slots:
    void EventSocketConnected();
    void EventSocketClosed();
    void EventSocketRead();

  private:
    QMutex             previewLock;
    pthread_t          previewThread;
    ProgramInfo        programInfo;
    QSocket           *eventSock;
    QSocketDevice     *serverSock;
};

#endif // PREVIEW_GENERATOR_H_
