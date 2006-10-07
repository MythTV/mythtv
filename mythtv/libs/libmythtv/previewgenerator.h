// -*- Mode: c++ -*-
#ifndef PREVIEW_GENERATOR_H_
#define PREVIEW_GENERATOR_H_

#include <pthread.h>

#include <qstring.h>
#include <qmutex.h>

#include "programinfo.h"
#include "util.h"

class MythSocket;

class MPUBLIC PreviewGenerator : public QObject
{
    Q_OBJECT
  public:
    PreviewGenerator(const ProgramInfo *pginfo, bool local_only = true);
    virtual ~PreviewGenerator();

    void Start(void);
    void Run(void);

    static bool SavePreview(QString filename,
                            const unsigned char *data,
                            uint width, uint height, float aspect);

    static char *GetScreenGrab(const ProgramInfo *pginfo,
                               const QString &filename, int secondsin,
                               int &bufferlen,
                               int &video_width, int &video_height,
                               float &video_aspect);

    void AttachSignals(QObject*);
    void disconnectSafe(void);

  signals:
    void previewThreadDone(const QString&, bool&);
    void previewReady(const ProgramInfo*);

  public slots:
    void deleteLater();

  private:
    void TeardownAll(void);
    bool RemotePreviewSetup(void);
    void RemotePreviewRun(void);
    void RemotePreviewTeardown(void);
    void LocalPreviewRun(void);
    bool IsLocal(void) const;
    static void *PreviewRun(void*);

    QMutex             previewLock;
    pthread_t          previewThread;
    ProgramInfo        programInfo;

    bool               localOnly;
    bool               isConnected;
    bool               createSockets;
    MythSocket        *serverSock;
};

#endif // PREVIEW_GENERATOR_H_
