// -*- Mode: c++ -*-

#ifndef EXTERNAL_STREAMHANDLER_H
#define EXTERNAL_STREAMHANDLER_H

// POSIX
#include <sys/types.h> // for pid_t

#include <cstdint>
#include <vector>

#include <QString>
#include <QAtomicInt>
#include <QByteArray>
#include <QFileInfo>
#include <QMutex>
#include <QMap>
#include <QVariantMap>
#include <QStringList>
#include <QTextStream>

#include "libmythbase/mythchrono.h"

#include "streamhandler.h"

class DTVSignalMonitor;
class ExternalChannel;

class ExternIO
{
    static constexpr uint8_t kMaxErrorCnt { 20 };

  public:
    ExternIO(const QString & app, const QStringList & args);
    ~ExternIO(void);

    bool Ready(int fd, std::chrono::milliseconds timeout, const QString & what);
    int Read(QByteArray & buffer, int maxlen, std::chrono::milliseconds timeout = 2500ms);
    QByteArray GetStatus(std::chrono::milliseconds timeout = 2500ms);
    int Write(const QByteArray & buffer);
    bool Run(void);
    bool Error(void) const { return !m_error.isEmpty(); }
    QString ErrorString(void) const { return m_error; }
    void ClearError(void) { m_error.clear(); }

    static bool KillIfRunning(const QString & cmd);

  private:
    void Fork(void);

    QFileInfo   m_app;
    QStringList m_args;
    int         m_appIn   {-1};
    int         m_appOut  {-1};
    int         m_appErr  {-1};
    pid_t       m_pid     {-1};
    QString     m_error;

    int         m_bufSize {0};
    char       *m_buffer  {nullptr};

    QString     m_statusBuf;
    QTextStream m_status;
    int         m_errCnt  {0};
};

// Note : This class always uses a TS reader.

class ExternalStreamHandler : public StreamHandler
{
    enum constants { MAX_API_VERSION = 3,
                     TS_PACKET_SIZE = 188,
                     PACKET_SIZE = TS_PACKET_SIZE * 8192,
                     TOO_FAST_SIZE = TS_PACKET_SIZE * 32768 };

  public:
    static ExternalStreamHandler *Get(const QString &devname,
                                      int inputid, int majorid);
    static void Return(ExternalStreamHandler * & ref, int inputid);

  public:
    explicit ExternalStreamHandler(const QString & path, int inputid,
                                   int majorid);
    ~ExternalStreamHandler(void) override { CloseApp(); }

    void run(void) override; // MThread
    void PriorityEvent(int fd) override; // DeviceReaderCB

    QString GetDescription(void) { return m_loc; }
    QString UpdateDescription(void);
    bool IsDamaged(void) const { return m_damaged; }
    void ClearDamaged(void) { m_damaged = false; }
    bool IsAppOpen(void);
    bool IsTSOpen(void);
    bool HasTuner(void) const { return m_hasTuner; }
    bool HasPictureAttributes(void) const { return m_hasPictureAttributes; }

    bool RestartStream(void);

    void LockReplay(void) { m_replayLock.lock(); }
    void UnlockReplay(bool enable_replay = false)
        { m_replay = enable_replay; m_replayLock.unlock(); }
    void ReplayStream(void);
    bool StartStreaming(void);
    bool StopStreaming(void);

    bool CheckForError(void);

    void PurgeBuffer(void);

    bool ProcessCommand(const QString & cmd, QString & result,
                        std::chrono::milliseconds timeout = 4s,
                        uint retry_cnt = 3);
    bool ProcessVer1(const QString & cmd, QString & result,
                     std::chrono::milliseconds timeout, uint retry_cnt);
    bool ProcessVer2(const QString & command, QString & result,
                     std::chrono::milliseconds timeout, uint retry_cnt);
    bool ProcessJson(const QVariantMap & vmsg,
                     QVariantMap & elements,
                     QByteArray & response,
                     std::chrono::milliseconds timeout = 4s,
                     uint retry_cnt = 3);
    int APIVersion(void) const { return m_apiVersion; }

  private:
    int  StreamingCount(void) const;
    bool SetAPIVersion(void);
    bool OpenApp(void);
    void CloseApp(void);

    QString       m_loc;
    int           m_majorId;
    QMutex        m_ioLock;
    ExternIO     *m_io                   {nullptr};
    QStringList   m_args;
    QString       m_app;
    bool          m_tsOpen               {false};
    int           m_ioErrCnt             {0};
    bool          m_pollMode             {false};

    int           m_apiVersion           {1};
    uint          m_serialNo             {0};
    bool          m_hasTuner             {false};
    bool          m_hasPictureAttributes {false};

    QByteArray    m_replayBuffer;
    bool          m_replay               {false};
    bool          m_xon                  {false};
    bool          m_damaged              {false};

    // for implementing Get & Return
    static QMutex                            s_handlersLock;
    static QMap<int, ExternalStreamHandler*> s_handlers;
    static QMap<int, uint>                   s_handlersRefCnt;

    QAtomicInt    m_streamingCnt;
    QMutex        m_streamLock;
    QMutex        m_replayLock;
    QMutex        m_processLock;
};

#endif // EXTERNAL_STREAMHANDLER_H
