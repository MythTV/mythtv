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
    friend int preview_helper(const QString &chanid,
                              const QString &starttime,
                              long long      previewFrameNumber,
                              long long      previewSeconds,
                              const QSize   &previewSize,
                              const QString &infile,
                              const QString &outfile);

    Q_OBJECT
  public:
    PreviewGenerator(const ProgramInfo *pginfo, bool local_only = true);

    void SetPreviewTime(long long time, bool in_seconds)
        { captureTime = time; timeInSeconds = in_seconds; }
    void SetPreviewTimeAsSeconds(long long seconds_in)
        { SetPreviewTime(seconds_in, true); }
    void SetPreviewTimeAsFrameNumber(long long frame_number)
        { SetPreviewTime(frame_number, false); }
    void SetOutputFilename(const QString&);
    void SetOutputSize(const QSize &size) { outSize = size; }

    void Start(void);
    bool Run(void);

    void AttachSignals(QObject*);
    void disconnectSafe(void);

    static const char *kInUseID;
  signals:
    void previewThreadDone(const QString&, bool&);
    void previewReady(const ProgramInfo*);

  public slots:
    void deleteLater();

  protected:
    virtual ~PreviewGenerator();
    void TeardownAll(void);

    bool RemotePreviewSetup(void);
    bool RemotePreviewRun(void);
    void RemotePreviewTeardown(void);

    bool LocalPreviewRun(void);
    bool IsLocal(void) const;

    bool RunReal(void);

    static void *PreviewRun(void*);

    static char *GetScreenGrab(const ProgramInfo *pginfo,
                               const QString     &filename,
                               long long          seektime,
                               bool               time_in_secs,
                               int               &bufferlen,
                               int               &video_width,
                               int               &video_height,
                               float             &video_aspect);

    static bool SavePreview(QString filename,
                            const unsigned char *data,
                            uint width, uint height, float aspect,
                            int desired_width, int desired_height);


    static QString CreateAccessibleFilename(
        const QString &pathname, const QString &outFileName);

  protected:
    QMutex             previewLock;
    pthread_t          previewThread;
    ProgramInfo        programInfo;

    bool               localOnly;
    bool               isConnected;
    bool               createSockets;
    MythSocket        *serverSock;
    QString            pathname;

    /// tells us whether to use time as seconds or frame number
    bool               timeInSeconds;
    /// snapshot time in seconds or frame number, depending on timeInSeconds
    long long          captureTime;
    QString            outFileName;
    QSize              outSize;
};

#endif // PREVIEW_GENERATOR_H_
