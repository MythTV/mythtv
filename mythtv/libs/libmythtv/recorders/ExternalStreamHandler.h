// -*- Mode: c++ -*-

#ifndef _External_Streamhandler_H_
#define _External_Streamhandler_H_

#include <vector>
using namespace std;

#include <QString>
#include <QAtomicInt>
#include <QMutex>
#include <QMap>
#include <stdint.h>

#include "streamhandler.h"

// #define USE_MYTHSYSTEM_FOR_RECORDER_IO 1

class DTVSignalMonitor;
class ExternalChannel;

class ExternIO
{
  public:
    ExternIO(const QString & app, const QStringList & args);
    ~ExternIO(void);

    bool Ready(int fd, int timeout);
    int Read(QByteArray & buffer, int maxlen, int timeout = 2500);
    QString GetStatus(int timeout = 2500);
    int Write(const QByteArray & buffer);
    bool Run(void);
    void Term(bool force = false);
    bool Error(void) const { return !m_error.isEmpty(); }
    QString ErrorString(void) const { return m_error; }

  private:
    void Fork(void);

    QFileInfo   m_app;
    QStringList m_args;
    int     m_appin;
    int     m_appout;
    int     m_apperr;
    pid_t   m_pid;
    QString m_error;

    int     m_bufsize;
    char   *m_buffer;
};

// Note : This class always uses a TS reader.

class ExternalStreamHandler : public StreamHandler
{
    enum constants {PACKET_SIZE = 188 * 1024};

  public:
    static ExternalStreamHandler *Get(const QString &devicename);
    static void Return(ExternalStreamHandler * & ref);

  public:
    ExternalStreamHandler(const QString & path);
    ~ExternalStreamHandler(void) { Close(); }

    virtual void run(void); // MThread
    virtual void PriorityEvent(int fd); // DeviceReaderCB

    bool IsOpen(void) const { return (m_IO && m_isOpen); }
    bool HasTuner(void) const { return m_hasTuner; }
    bool HasPictureAttributes(void) const { return m_hasPictureAttributes; }

    bool StartStreaming(bool flush_buffer);
    bool StopStreaming(void);

    void PurgeBuffer(void);

    QString ErrorString(void) const { return m_error; }

    bool ProcessCommand(const QString & cmd, uint timeout,
                        QString & result);

  private:
    int StreamingCount(void) const;
    bool OpenApp(void);
    bool Open(void);
    void Close(void);

    QMutex         m_IO_lock;
#ifdef USE_MYTHSYSTEM_FOR_RECORDER_IO
    MythSystem    *m_IO;
#else
    ExternIO      *m_IO;
#endif
    QStringList    m_args;
    QString        m_app;
    QString        m_error;
    bool           m_isOpen;
    int            m_io_errcnt;
    bool           m_poll_mode;

    bool           m_hasTuner;
    bool           m_hasPictureAttributes;

    QByteArray     m_replay_buffer;
    bool           m_replay;

    // for implementing Get & Return
    static QMutex                           m_handlers_lock;
    static QMap<QString, ExternalStreamHandler*> m_handlers;
    static QMap<QString, uint>              m_handlers_refcnt;

    QAtomicInt    m_streaming_cnt;
    QMutex        m_stream_lock;
};

#endif // _ExternalSTREAMHANDLER_H_
