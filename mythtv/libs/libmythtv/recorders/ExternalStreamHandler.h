// -*- Mode: c++ -*-

#ifndef _External_Streamhandler_H_
#define _External_Streamhandler_H_

#include <cstdint>
#include <vector>
using namespace std;

#include <QString>
#include <QAtomicInt>
#include <QMutex>
#include <QMap>

#include "streamhandler.h"

class DTVSignalMonitor;
class ExternalChannel;

class ExternIO
{
    enum constants { kMaxErrorCnt = 20 };

  public:
    ExternIO(const QString & app, const QStringList & args);
    ~ExternIO(void);

    bool Ready(int fd, int timeout, const QString & what);
    int Read(QByteArray & buffer, int maxlen, int timeout = 2500);
    QString GetStatus(int timeout = 2500);
    int Write(const QByteArray & buffer);
    bool Run(void);
    bool Error(void) const { return !m_error.isEmpty(); }
    QString ErrorString(void) const { return m_error; }
    void ClearError(void) { m_error.clear(); }

    bool KillIfRunning(const QString & cmd);

  private:
    void Fork(void);

    QFileInfo   m_app;
    QStringList m_args;
    int         m_appin   {-1};
    int         m_appout  {-1};
    int         m_apperr  {-1};
    pid_t       m_pid     {-1};
    QString     m_error;

    int         m_bufsize {0};
    char       *m_buffer  {nullptr};

    QString     m_status_buf;
    QTextStream m_status;
    int         m_errcnt  {0};
};

// Note : This class always uses a TS reader.

class ExternalStreamHandler : public StreamHandler
{
    enum constants { MAX_API_VERSION = 2,
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
    ~ExternalStreamHandler(void) { CloseApp(); }

    void run(void) override; // MThread
    void PriorityEvent(int fd) override; // DeviceReaderCB

    QString GetDescription(void) { return m_loc; }
    QString UpdateDescription(void);
    bool IsAppOpen(void);
    bool IsTSOpen(void);
    bool HasTuner(void) const { return m_hasTuner; }
    bool HasPictureAttributes(void) const { return m_hasPictureAttributes; }

    bool RestartStream(void);

    void LockReplay(void) { m_replay_lock.lock(); }
    void UnlockReplay(bool enable_replay = false)
        { m_replay = enable_replay; m_replay_lock.unlock(); }
    void ReplayStream(void);
    bool StartStreaming(void);
    bool StopStreaming(void);

    bool CheckForError(void);

    void PurgeBuffer(void);

    bool ProcessCommand(const QString & cmd, QString & result,
                        int timeout = 4000 /* ms */,uint retry_cnt = 3);
    bool ProcessVer1(const QString & cmd, QString & result,
                     int timeout /* ms */, uint retry_cnt);
    bool ProcessVer2(const QString & command, QString & result,
                     int timeout /* ms */, uint retry_cnt);

  private:
    int  StreamingCount(void) const;
    bool SetAPIVersion(void);
    bool OpenApp(void);
    void CloseApp(void);

    QString       m_loc;
    int           m_majorid;
    QMutex        m_io_lock;
    ExternIO     *m_io                   {nullptr};
    QStringList   m_args;
    QString       m_app;
    bool          m_tsopen               {false};
    int           m_io_errcnt            {0};
    bool          m_poll_mode            {false};

    int           m_apiVersion           {1};
    uint          m_serialNo             {0};
    bool          m_hasTuner             {false};
    bool          m_hasPictureAttributes {false};

    QByteArray    m_replay_buffer;
    bool          m_replay               {false};
    bool          m_xon                  {false};

    // for implementing Get & Return
    static QMutex                            s_handlers_lock;
    static QMap<int, ExternalStreamHandler*> s_handlers;
    static QMap<int, uint>                   s_handlers_refcnt;

    QAtomicInt    m_streaming_cnt;
    QMutex        m_stream_lock;
    QMutex        m_replay_lock;
    QMutex        m_process_lock;
};

#endif // _ExternalSTREAMHANDLER_H_
