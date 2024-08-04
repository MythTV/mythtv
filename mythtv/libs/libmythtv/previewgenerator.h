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

#include "libmythbase/mthread.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/programinfo.h"

#include "mythtvexp.h"

class PreviewGenerator;
class QByteArray;
class MythSocket;
class QObject;
class QEvent;

using FileTimeStampMap = QMap<QString,QDateTime>;

class MTV_PUBLIC PreviewGenerator : public QObject, public MThread
{
    friend int preview_helper(uint           chanid,
                              QDateTime      starttime,
                              long long      previewFrameNumber,
                              std::chrono::seconds previewSeconds,
                              QSize          previewSize,
                              const QString &infile,
                              const QString &outfile);

    Q_OBJECT

  public:
    enum Mode : std::uint8_t
    {
        kNone           = 0x0,
        kLocal          = 0x1,
        kRemote         = 0x2,
        kLocalAndRemote = 0x3,
        kForceLocal     = 0x5,
        kModeMask       = 0x7,
    };

  public:
    PreviewGenerator(const ProgramInfo *pginfo,
                     QString            token,
                     Mode               mode = kLocal);

    void SetPreviewTime(std::chrono::seconds time, long long frame)
        { m_captureTime = time; m_captureFrame = frame; }
    void SetPreviewTimeAsSeconds(std::chrono::seconds seconds)
        { SetPreviewTime(seconds, -1); }
    void SetPreviewTimeAsFrameNumber(long long frame_number)
        { SetPreviewTime(-1s, frame_number); }
    void SetOutputFilename(const QString &fileName);
    void SetOutputSize(const QSize size) { m_outSize = size; }

    QString GetToken(void) const { return m_token; }

    void run(void) override; // MThread
    bool Run(void);

    void AttachSignals(QObject *obj);

  public slots:
    void deleteLater();

  protected:
    ~PreviewGenerator() override;
    void TeardownAll(void);

    bool RemotePreviewRun(void);
    bool LocalPreviewRun(void);
    bool IsLocal(void) const;

    bool RunReal(void);

    static char *GetScreenGrab(const ProgramInfo &pginfo,
                               const QString     &filename,
                               std::chrono::seconds seektime,
                               long long          seekframe,
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

    bool event(QEvent *e) override; // QObject
    bool SaveOutFile(const QByteArray &data, const QDateTime &dt);

  protected:
    QWaitCondition     m_previewWaitCondition;
    QMutex             m_previewLock;
    ProgramInfo        m_programInfo;

    Mode               m_mode;
    QObject           *m_listener      {nullptr};
    QString            m_pathname;

    /// snapshot time in seconds or frame number (seconds has priority)
    std::chrono::seconds m_captureTime   {-1s};
    long long          m_captureFrame  {-1};
    QString            m_outFileName;
    QSize              m_outSize       {0,0};
    QString            m_outFormat     {"PNG"};

    QString            m_token;
    bool               m_gotReply      {false};
    bool               m_pixmapOk      {false};
};

#endif // PREVIEW_GENERATOR_H_
