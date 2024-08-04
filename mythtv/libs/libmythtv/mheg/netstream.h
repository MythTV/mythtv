/* Network stream
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#ifndef NETSTREAM_H
#define NETSTREAM_H

#include <QList>
#include <QString>
#include <QByteArray>
#include <QObject>
#include <QMutex>
#include <QSemaphore>
#include <QThread>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRecursiveMutex>
#include <QSslError>
#include <QWaitCondition>
#include <QQueue>
#include <QDateTime>

class QUrl;
class QNetworkAccessManager;
class NetStreamRequest;
class NetStreamAbort;


/**
 * Stream content from a URI
 */
class NetStream : public QObject
{
    Q_OBJECT

public:
    enum EMode : std::uint8_t { kNeverCache, kPreferCache, kAlwaysCache };
    explicit NetStream(const QUrl &url, EMode mode = kPreferCache,
                       QByteArray cert = QByteArray());
    ~NetStream() override;

public:
    // RingBuffer interface
    static bool IsSupported(const QUrl &url);
    bool IsOpen() const;
    void Abort();
    int safe_read(void *data, unsigned sz, unsigned millisecs = 0);
    qlonglong Seek(qlonglong pos);
    qlonglong GetReadPosition() const;
    qlonglong GetSize() const;

    // Properties
    const QUrl &Url() const { return m_url; }

    // Synchronous interface
    bool WaitTillReady(std::chrono::milliseconds timeout);
    bool WaitTillFinished(std::chrono::milliseconds timeout);
    QNetworkReply::NetworkError GetError() const;
    QString GetErrorString() const;
    qlonglong BytesAvailable() const;
    QByteArray ReadAll();

    // Async interface
    bool isStarted() const;
    bool isReady() const;
    bool isFinished() const;

signals:
    void ReadyRead(QObject*);
    void Finished(QObject*);

public:
    // Time when a URI was last written to cache or invalid if not cached.
    static QDateTime GetLastModified(const QUrl &url);
    // Is the network accessible
    static bool isAvailable();

    // Implementation
private slots:
    // NAMThread signals
    void slotRequestStarted(int id, QNetworkReply *reply);
    // QNetworkReply signals
    void slotFinished();
#ifndef QT_NO_OPENSSL
    void slotSslErrors(const QList<QSslError> & errors);
#endif
    // QIODevice signals
    void slotReadyRead();

private:
    Q_DISABLE_COPY(NetStream)

    bool Request(const QUrl &url);

    const int m_id; // Unique request ID
    const QUrl m_url;

    mutable QMutex    m_mutex; // Protects r/w access to the following data
    QNetworkRequest   m_request;
    enum : std::uint8_t
         { kClosed, kPending, kStarted, kReady, kFinished } m_state {kClosed};
    NetStreamRequest* m_pending       {nullptr};
    QNetworkReply*    m_reply         {nullptr};
    int               m_nRedirections {0};
    qlonglong         m_size          {-1};
    qlonglong         m_pos           {0};
    QByteArray        m_cert;
    QWaitCondition    m_ready;
    QWaitCondition    m_finished;
};


/**
 * Thread to process NetStream requests
 */
class NAMThread : public QThread
{
    Q_OBJECT

    // Use manager() to create
    NAMThread();

public:
    static NAMThread & manager(); // Singleton
    ~NAMThread() override;

    static void PostEvent(QEvent *e) { manager().Post(e); }
    void Post(QEvent *event);

    static QRecursiveMutex* GetMutex() { return &manager().m_mutexNAM; }

    static bool isAvailable(); // is network usable
    static QDateTime GetLastModified(const QUrl &url);

signals:
     void requestStarted(int, QNetworkReply *);

    // Implementation
protected:
    void run() override; // QThread
    bool NewRequest(QEvent *event);
    bool StartRequest(NetStreamRequest *p);
    static bool AbortRequest(NetStreamAbort *p);

private slots:
    void quit();

private:
    Q_DISABLE_COPY(NAMThread)

    volatile bool          m_bQuit    {false};
    QSemaphore             m_running;
    mutable QRecursiveMutex m_mutexNAM; // Provides recursive access to m_nam
    QNetworkAccessManager *m_nam      {nullptr};
    mutable QMutex         m_mutex; // Protects r/w access to the following data
    QQueue< QEvent * >     m_workQ;
    QWaitCondition         m_work;
};

#endif /* ndef NETSTREAM_H */
