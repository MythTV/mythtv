// -*- Mode: c++ -*-
#ifndef PREVIEW_GENERATOR_H_
#define PREVIEW_GENERATOR_H_

#include <QWaitCondition>
#include <QDateTime>
#include <QString>
#include <QMutex>
#include <QSize>
#include <QMap>
#include <QSet>

#include "programinfo.h"
#include "mythtvexp.h"
#include "mthread.h"
#include "mythdate.h"

class PreviewGenerator;
class QByteArray;
class MythSocket;
class QObject;
class QEvent;

typedef QMap<QString,QDateTime> FileTimeStampMap;

class MTV_PUBLIC PreviewGenerator : public QObject, public MThread
{
    friend int preview_helper(uint           chanid,
                              QDateTime      starttime,
                              long long      previewFrameNumber,
                              long long      previewSeconds,
                              const QSize   &previewSize,
                              const QString &infile,
                              const QString &outfile);

    Q_OBJECT

  public:
    typedef enum Mode
    {
        kNone           = 0x0,
        kLocal          = 0x1,
        kRemote         = 0x2,
        kLocalAndRemote = 0x3,
        kForceLocal     = 0x5,
        kModeMask       = 0x7,
    } Mode;

  public:
    PreviewGenerator(const ProgramInfo *pginfo,
                     const QString     &token,
                     Mode               mode = kLocal);

    void SetPreviewTime(long long time, bool in_seconds)
        { m_captureTime = time; m_timeInSeconds = in_seconds; }
    void SetPreviewTimeAsSeconds(long long seconds_in)
        { SetPreviewTime(seconds_in, true); }
    void SetPreviewTimeAsFrameNumber(long long frame_number)
        { SetPreviewTime(frame_number, false); }
    void SetOutputFilename(const QString&);
    void SetOutputSize(const QSize &size) { m_outSize = size; }

    QString GetToken(void) const { return m_token; }

    void run(void); // MThread
    bool Run(void);

    void AttachSignals(QObject*);

  public slots:
    void deleteLater();

  protected:
    virtual ~PreviewGenerator();
    void TeardownAll(void);

    bool RemotePreviewRun(void);
    bool LocalPreviewRun(void);
    bool IsLocal(void) const;

    bool RunReal(void);

    static char *GetScreenGrab(const ProgramInfo &pginfo,
                               const QString     &filename,
                               long long          seektime,
                               bool               time_in_secs,
                               int               &bufferlen,
                               int               &video_width,
                               int               &video_height,
                               float             &video_aspect);

    static bool SavePreview(const QString &filename,
                            const unsigned char *data,
                            uint width, uint height, float aspect,
                            int desired_width, int desired_height,
                            const QString &format);


    static QString CreateAccessibleFilename(
        const QString &pathname, const QString &outFileName);

    virtual bool event(QEvent *e); // QObject
    bool SaveOutFile(const QByteArray &data, const QDateTime &dt);

  protected:
    QWaitCondition     m_previewWaitCondition;
    QMutex             m_previewLock;
    ProgramInfo        m_programInfo;

    Mode               m_mode;
    QObject           *m_listener;
    QString            m_pathname;

    /// tells us whether to use time as seconds or frame number
    bool               m_timeInSeconds;
    /// snapshot time in seconds or frame number, depending on timeInSeconds
    long long          m_captureTime;
    QString            m_outFileName;
    QSize              m_outSize;
    QString            m_outFormat;

    QString            m_token;
    bool               m_gotReply;
    bool               m_pixmapOk;
};

#endif // PREVIEW_GENERATOR_H_
