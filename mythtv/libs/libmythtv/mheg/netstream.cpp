/* Network stream
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#include "netstream.h"

// C/C++ lib
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cinttypes>
#include <utility>

// Qt
#include <QAtomicInt>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QMetaType> // qRegisterMetaType
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimeZone>
#include <QUrl>
#ifndef QT_NO_OPENSSL
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QSslKey>
#endif

// Myth
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"

/*
 * Constants
 */
static const QString LOC { QStringLiteral("[netstream] ") };


/*
 * Private data
 */
static QAtomicInt s_nRequest(1); // Unique NetStream request ID
static QMutex s_mtx; // Guard local static data e.g. NAMThread singleton
static constexpr qint64 kMaxBuffer = 4LL * 1024 * 1024L; // 0= unlimited, 1MB => 4secs @ 1.5Mbps


/*
 * Private types
 */
// Custom event posted to NAMThread
class NetStreamRequest : public QEvent
{
public:
    static const QEvent::Type kType = QEvent::User;

    NetStreamRequest(int id, const QNetworkRequest &req) :
        QEvent(kType),
        m_id(id),
        m_req(req)
    { }

    const int m_id;
    const QNetworkRequest m_req;
    volatile bool m_bCancelled { false };
};

class NetStreamAbort : public QEvent
{
public:
    static const QEvent::Type kType = static_cast< QEvent::Type >(QEvent::User + 1);

    NetStreamAbort(int id, QNetworkReply *reply) :
        QEvent(kType),
        m_id(id),
        m_reply(reply)
    { }

    const int m_id;
    QNetworkReply * const m_reply;
};


/**
 * Network streaming request
 */
NetStream::NetStream(const QUrl &url, EMode mode /*= kPreferCache*/,
        QByteArray cert) :
    m_id(s_nRequest.fetchAndAddRelaxed(1)),
    m_url(url),
    m_cert(std::move(cert))
{
    setObjectName("NetStream " + url.toString());

    QNetworkRequest::CacheLoadControl attr {QNetworkRequest::PreferNetwork};
    if (mode == kAlwaysCache)
        attr =  QNetworkRequest::AlwaysCache;
    else if (mode == kPreferCache)
        attr = QNetworkRequest::PreferCache;
    else if (mode == kNeverCache)
        attr = QNetworkRequest::AlwaysNetwork;
    m_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, attr);

    // Receive requestStarted signals from NAMThread when it processes a NetStreamRequest
    connect(&NAMThread::manager(), &NAMThread::requestStarted,
        this, &NetStream::slotRequestStarted, Qt::DirectConnection );

    QMutexLocker locker(&m_mutex);

    if (Request(url))
        m_state = kPending;
}

// virtual
NetStream::~NetStream()
{
    Abort();

    (NAMThread::manager()).disconnect(this);

    QMutexLocker locker(&m_mutex);

    if (m_reply)
    {
        m_reply->disconnect(this);
        m_reply->deleteLater();
    }
}

static inline QString Source(const QNetworkRequest &request)
{
    switch (request.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt())
    {
    case QNetworkRequest::AlwaysCache: return "cache";
    case QNetworkRequest::PreferCache: return "cache-preferred";
    case QNetworkRequest::PreferNetwork: return "net-preferred";
    case QNetworkRequest::AlwaysNetwork: return "net";
    }
    return "unknown";
}

static inline QString Source(const QNetworkReply* reply)
{
    return reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool() ?
        "cache" : "host";
}

// Send request to the network manager
// Caller must hold m_mutex
bool NetStream::Request(const QUrl& url)
{
    if (!IsSupported(url))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("(%1) Request unsupported URL: %2")
            .arg(m_id).arg(url.toString()) );
        return false;
    }

    if (m_pending)
    {
        // Cancel the pending request
        m_pending->m_bCancelled = true;
        m_pending = nullptr;
    }

    if (m_reply)
    {
        // Abort the current reply
        // NB the abort method appears to only work if called from NAMThread
        m_reply->disconnect(this);
        NAMThread::PostEvent(new NetStreamAbort(m_id, m_reply));
        // NAMthread will delete the reply
        m_reply = nullptr;
    }

    m_request.setUrl(url);

    const QByteArray ua("User-Agent");
    if (!m_request.hasRawHeader(ua))
        m_request.setRawHeader(ua, "UK-MHEG/2 MYT001/001 MHGGNU/001");

    if (m_pos > 0 || m_size >= 0)
        m_request.setRawHeader("Range", QString("bytes=%1-").arg(m_pos).toLatin1());

#ifndef QT_NO_OPENSSL
    if (m_request.url().scheme() == "https")
    {
        QSslConfiguration ssl(QSslConfiguration::defaultConfiguration());

        QList<QSslCertificate> clist;
        if (!m_cert.isEmpty())
        {
            clist = QSslCertificate::fromData(m_cert, QSsl::Der);
            if (clist.isEmpty())
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Invalid certificate: %1")
                    .arg(m_cert.toPercentEncoding().constData()) );
        }

        if (clist.isEmpty())
        {
            // The BBC servers use a self certified cert so don't verify it
            ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        }
        else
        {
            ssl.setCaCertificates(clist);
        }

        // We need to provide a client certificate for the BBC,  See:
        // openssl s_client -state -prexit -connect securegate.iplayer.bbc.co.uk:443
        // for a list of accepted certificates
        QString fname = gCoreContext->GetSetting("MhegClientCert", "");
        if (!fname.isEmpty())
        {
            QFile f1(QFile::exists(fname) ? fname : GetShareDir() + fname);
            if (f1.open(QIODevice::ReadOnly))
            {
                QSslCertificate cert(&f1, QSsl::Pem);
                if (!cert.isNull())
                    ssl.setLocalCertificate(cert);
                else
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("'%1' is an invalid certificate").arg(f1.fileName()) );
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Opening client certificate '%1': %2")
                    .arg(f1.fileName(), f1.errorString()) );
            }

            // Get the private key
            fname = gCoreContext->GetSetting("MhegClientKey", "");
            if (!fname.isEmpty())
            {
                QFile f2(QFile::exists(fname) ? fname : GetShareDir() + fname);
                if (f2.open(QIODevice::ReadOnly))
                {
                    QSslKey key(&f2, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey,
                        gCoreContext->GetSetting("MhegClientKeyPass", "").toLatin1());
                    if (!key.isNull())
                        ssl.setPrivateKey(key);
                    else
                        LOG(VB_GENERAL, LOG_WARNING, LOC +
                            QString("'%1' is an invalid key").arg(f2.fileName()) );
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Opening private key '%1': %2")
                        .arg(f2.fileName(), f2.errorString()) );
                }
            }
        }

        m_request.setSslConfiguration(ssl);
    }
#endif

    LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Request %2 bytes=%3- from %4")
        .arg(m_id).arg(m_request.url().toString())
        .arg(m_pos).arg(Source(m_request)) );
    m_pending = new NetStreamRequest(m_id, m_request);
    NAMThread::PostEvent(m_pending);
    return true;
}

// signal from NAMThread manager that a request has been started
void NetStream::slotRequestStarted(int id, QNetworkReply *reply)
{
    QMutexLocker locker(&m_mutex);

    if (m_id != id)
        return;

    m_pending = nullptr; // Event is no longer valid

    if (!m_reply)
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Started 0x%2")
            .arg(m_id).arg(quintptr(reply),0,16) );

        m_reply = reply;
        m_state = kStarted;

        reply->setReadBufferSize(kMaxBuffer);

        // NB The following signals must be Qt::DirectConnection 'cos this slot
        // was connected Qt::DirectConnection so the current thread is NAMThread

        // QNetworkReply signals
        connect(reply, &QNetworkReply::finished, this, &NetStream::slotFinished, Qt::DirectConnection );
#ifndef QT_NO_OPENSSL
        connect(reply, &QNetworkReply::sslErrors, this,
            &NetStream::slotSslErrors, Qt::DirectConnection );
#endif
        // QIODevice signals
        connect(reply, &QIODevice::readyRead, this, &NetStream::slotReadyRead, Qt::DirectConnection );
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("(%1) Started but m_reply not NULL").arg(m_id));
    }
}

static qlonglong inline ContentLength(const QNetworkReply *reply)
{
    bool ok = false;
    qlonglong len = reply->header(QNetworkRequest::ContentLengthHeader)
        .toLongLong(&ok);
    return ok ? len : -1;
}

static qlonglong inline ContentRange(const QNetworkReply *reply,
                                     qulonglong &first, qulonglong &last)
{
    QByteArray range = reply->rawHeader("Content-Range");
    if (range.isEmpty())
        return -1;

    // See RFC 2616 14.16: 'bytes begin-end/size'
    qulonglong len = 0;
    const char *fmt = " bytes %20" SCNd64 " - %20" SCNd64 " / %20" SCNd64;
    if (3 != std::sscanf(range.constData(), fmt, &first, &last, &len))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid Content-Range:'%1'")
            .arg(range.constData()) );
        return -1;
    }

    return static_cast<qlonglong>(len);
}

#if 0
static bool inline RequestRange(const QNetworkRequest &request,
    qlonglong &first, qlonglong &last)
{
    first = last = -1;

    QByteArray range = request.rawHeader("Range");
    if (range.isEmpty())
        return false;

    if (1 > std::sscanf(range.constData(), " bytes %20lld - %20lld", &first, &last))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid Range:'%1'")
            .arg(range.constData()) );
        return false;
    }

    return true;
}
#endif

// signal from QNetworkReply
void NetStream::slotReadyRead()
{
    QMutexLocker locker(&m_mutex);

    if (m_reply)
    {
        qint64 avail = m_reply->bytesAvailable();
        LOG(VB_FILE, (avail <= 2 * kMaxBuffer) ? LOG_DEBUG :
                (avail <= 4 * kMaxBuffer) ? LOG_INFO : LOG_WARNING,
             LOC + QString("(%1) Ready 0x%2, %3 bytes available").arg(m_id)
                .arg(quintptr(m_reply),0,16).arg(avail) );

        if (m_size < 0 || m_state < kReady)
        {
            qulonglong first = 0;
            qulonglong last = 0;
            qlonglong len = ContentRange(m_reply, first, last);
            if (len >= 0)
            {
                m_size = len;
                LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Ready 0x%2, range %3-%4/%5")
                    .arg(m_id).arg(quintptr(m_reply),0,16).arg(first).arg(last).arg(len) );
            }
            else
            {
                m_size = ContentLength(m_reply);
                if (m_state < kReady || m_size >= 0)
                {
                    LOG(VB_FILE, LOG_INFO, LOC +
                        QString("(%1) Ready 0x%2, content length %3")
                        .arg(m_id).arg(quintptr(m_reply),0,16).arg(m_size) );
                }
            }
        }

        m_state = std::max(m_state, kReady);

        locker.unlock();
        emit ReadyRead(this);
        locker.relock();

        m_ready.wakeAll();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("(%1) ReadyRead but m_reply = NULL").arg(m_id));
    }
}

// signal from QNetworkReply
void NetStream::slotFinished()
{
    QMutexLocker locker(&m_mutex);

    if (m_reply)
    {
        QNetworkReply::NetworkError error = m_reply->error();
        if (QNetworkReply::NoError == error)
        {
            // Check for a re-direct
            QUrl url = m_reply->attribute(
                QNetworkRequest::RedirectionTargetAttribute).toUrl();
            if (!url.isValid())
            {
                m_state = kFinished;
            }
            else if (m_nRedirections++ > 0)
            {
                LOG(VB_FILE, LOG_WARNING, LOC + QString("(%1) Too many redirections")
                    .arg(m_id));
                m_state = kFinished;
            }
            else
            {
                url = m_request.url().resolved(url);
                if (url == m_request.url())
                {
                    LOG(VB_FILE, LOG_WARNING, LOC + QString("(%1) Redirection loop to %2")
                        .arg(m_id).arg(url.toString()) );
                    m_state = kFinished;
                }
                else
                {
                    LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Redirecting").arg(m_id));
                    m_state = Request(url) ? kPending : kFinished;
                }
            }
        }
        else
        {
            LOG(VB_FILE, LOG_WARNING, LOC + QString("(%1): %2")
                .arg(m_id).arg(m_reply->errorString()) );
            m_state = kFinished;
        }

        if (m_state == kFinished)
        {
            if (m_size < 0)
                m_size = m_pos + m_reply->size();

            LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Finished 0x%2 %3/%4 bytes from %5")
                .arg(m_id).arg(quintptr(m_reply),0,16).arg(m_pos).arg(m_size).arg(Source(m_reply)) );

            locker.unlock();
            emit Finished(this);
            locker.relock();

            m_finished.wakeAll();
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("(%1) Finished but m_reply = NULL")
            .arg(m_id));
    }
}

#ifndef QT_NO_OPENSSL
// signal from QNetworkReply
void NetStream::slotSslErrors(const QList<QSslError> &errors)
{
    QMutexLocker locker(&m_mutex);

    if (m_reply)
    {
        bool bIgnore = true;
        for (const auto& e : std::as_const(errors))
        {
            LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) SSL error %2: ")
                .arg(m_id).arg(e.error()) + e.errorString() );
            switch (e.error())
            {
#if 1 // The BBC use a self certified cert
            case QSslError::SelfSignedCertificateInChain:
                break;
#endif
            default:
                bIgnore = false;
                break;
            }
        }

        if (bIgnore)
        {
            LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) SSL errors ignored").arg(m_id));
            m_reply->ignoreSslErrors(errors);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("(%1) SSL error but m_reply = NULL").arg(m_id) );
    }
}
#endif


/**
 * RingBuffer interface
 */
// static
bool NetStream::IsSupported(const QUrl &url)
{
    return url.isValid() &&
        (url.scheme() == "http" || url.scheme() == "https") &&
        !url.authority().isEmpty() &&
        !url.path().isEmpty();
}

bool NetStream::IsOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_state > kClosed;
}

void NetStream::Abort()
{
    QMutexLocker locker(&m_mutex);

    if (m_pending)
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Cancelled").arg(m_id) );
        m_pending->m_bCancelled = true;
        m_pending = nullptr;
    }

    if (m_reply)
    {
        if (m_state >= kStarted && m_state < kFinished)
            LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Abort 0x%2")
                .arg(m_id).arg(quintptr(m_reply),0,16) );

        NAMThread::PostEvent(new NetStreamAbort(m_id, m_reply));
        // NAMthread will delete the reply
        m_reply = nullptr;
    }

    m_state = kFinished;
}

int NetStream::safe_read(void *data, unsigned sz, unsigned millisecs /* = 0 */)
{
    QElapsedTimer t; t.start();
    QMutexLocker locker(&m_mutex);

    if (m_size >= 0 && m_pos >= m_size)
        return 0; // EOF

    while (m_state < kFinished && (!m_reply || m_reply->bytesAvailable() < sz))
    {
        unsigned elapsed = t.elapsed();
        if (elapsed >= millisecs)
            break;
        m_ready.wait(&m_mutex, millisecs - elapsed);
    }

    locker.unlock();
    QMutexLocker lockNAM(NAMThread::GetMutex());
    locker.relock();
    if (!m_reply)
        return -1;

    qint64 avail = m_reply->read(reinterpret_cast< char* >(data), sz);
    if (avail <= 0)
        return m_state >= kFinished ? 0 : -1; // 0= EOF

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("(%1) safe_read @ %4 => %2/%3, %5 mS")
        .arg(m_id).arg(avail).arg(sz).arg(m_pos).arg(t.elapsed()) );
    m_pos += avail;
    return (int)avail;
}

qlonglong NetStream::Seek(qlonglong pos)
{
    QMutexLocker locker(&m_mutex);

    if (pos == m_pos)
        return pos;

    if (pos < 0 || (m_size >= 0 && pos > m_size))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("(%1) Seek(%2) out of range [0..%3]")
            .arg(m_id).arg(pos).arg(m_size) );
        return -1;
    }

    LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Seek(%2) curr %3 end %4")
        .arg(m_id).arg(pos).arg(m_pos).arg(m_size) );
    m_pos = pos;
    return Request(m_request.url()) ? m_pos : -1;
}

qlonglong NetStream::GetReadPosition() const
{
    QMutexLocker locker(&m_mutex);

    return m_pos;
}

qlonglong NetStream::GetSize() const
{
    QMutexLocker locker(&m_mutex);

    return m_size;
}


/**
 * Synchronous interface
 */
bool NetStream::WaitTillReady(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_mutex);

    QElapsedTimer t; t.start();
    while (m_state < kReady)
    {
        auto elapsed = std::chrono::milliseconds(t.elapsed());
        if (elapsed > timeout)
            return false;

        m_ready.wait(&m_mutex, (timeout - elapsed).count());
    }

    return true;
}

bool NetStream::WaitTillFinished(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_mutex);

    QElapsedTimer t; t.start();
    while (m_state < kFinished)
    {
        auto elapsed = std::chrono::milliseconds(t.elapsed());
        if (elapsed > timeout)
            return false;

        m_finished.wait(&m_mutex, (timeout - elapsed).count());
    }

    return true;
}

QNetworkReply::NetworkError NetStream::GetError() const
{
    QMutexLocker locker(&m_mutex);
    return !m_reply ? QNetworkReply::OperationCanceledError : m_reply->error();
}

QString NetStream::GetErrorString() const
{
    QMutexLocker locker(&m_mutex);
    return !m_reply ? "Operation cancelled" : m_reply->errorString();
}

qlonglong NetStream::BytesAvailable() const
{
    QMutexLocker locker(&m_mutex);
    return m_reply ? m_reply->bytesAvailable() : 0;
}

QByteArray NetStream::ReadAll()
{
    QMutexLocker locker(&m_mutex);

    if (!m_reply)
        return nullptr;

    QByteArray data =  m_reply->readAll();
    m_pos += data.size();
    return data;
}

/**
 * Asynchronous interface
 */
bool NetStream::isStarted() const
{
    QMutexLocker locker(&m_mutex);
    return m_state >= kStarted;
}

bool NetStream::isReady() const
{
    QMutexLocker locker(&m_mutex);
    return m_state >= kReady;
}

bool NetStream::isFinished() const
{
    QMutexLocker locker(&m_mutex);
    return m_state >= kFinished;
}

/**
 * Public helpers
 */
// static
bool NetStream::isAvailable()
{
    return NAMThread::isAvailable();
}

// Time when URI was last written to cache or invalid if not cached.
// static
QDateTime NetStream::GetLastModified(const QUrl &url)
{
    return NAMThread::GetLastModified(url);
}


/**
 * NetworkAccessManager event loop thread
 */
//static
NAMThread & NAMThread::manager()
{
    QMutexLocker locker(&s_mtx);

    // Singleton
    static NAMThread s_thread;
    s_thread.start();
    return s_thread;
}

NAMThread::NAMThread()
{
    setObjectName("NAMThread");

#ifndef QT_NO_OPENSSL
    // This ought to be done by the Qt lib but isn't in 4.7
    //Q_DECLARE_METATYPE(QList<QSslError>)
    qRegisterMetaType< QList<QSslError> >();
#endif
}

// virtual
NAMThread::~NAMThread()
{
    QMutexLocker locker(&m_mutex);
    delete m_nam;
}

// virtual
void NAMThread::run()
{
    LOG(VB_FILE, LOG_INFO, LOC + "NAMThread starting");

    m_nam = new QNetworkAccessManager();
    m_nam->setObjectName("NetStream NAM");

    // Setup cache
    std::unique_ptr<QNetworkDiskCache> cache(new QNetworkDiskCache());

    cache->setCacheDirectory(GetConfDir() + "/cache/netstream-" +
                             gCoreContext->GetHostName());

    m_nam->setCache(cache.release());

    // Setup a network proxy e.g. for TOR: socks://localhost:9050
    // TODO get this from mythdb
    QString proxy(qEnvironmentVariable("MYTHMHEG_PROXY"));
    if (!proxy.isEmpty())
    {
        QUrl url(proxy, QUrl::TolerantMode);
        QNetworkProxy::ProxyType type {QNetworkProxy::NoProxy};
        if (url.scheme().isEmpty()
            || (url.scheme() == "http")
            || (url.scheme() == "https"))
            type = QNetworkProxy::HttpProxy;
        else if (url.scheme() == "socks")
            type = QNetworkProxy::Socks5Proxy;
        else if (url.scheme() == "cache")
            type = QNetworkProxy::HttpCachingProxy;
        else if (url.scheme() == "ftp")
            type = QNetworkProxy::FtpCachingProxy;

        if (QNetworkProxy::NoProxy != type)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Using proxy: " + proxy);
            m_nam->setProxy(QNetworkProxy(
                type, url.host(), url.port(), url.userName(), url.password() ));
        }
        else
        {
            LOG(VB_MHEG, LOG_ERR, LOC + QString("Unknown proxy type %1")
                .arg(url.scheme()) );
        }
    }

    // Quit when main app quits
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &NAMThread::quit);

    m_running.release();

    QMutexLocker lockNAM(&m_mutexNAM);
    while(!m_bQuit)
    {
        // Process NAM events
        QCoreApplication::processEvents();

        lockNAM.unlock();

        QMutexLocker locker(&m_mutex);
        m_work.wait(&m_mutex, 100);

        lockNAM.relock();

        while (!m_workQ.isEmpty())
        {
            QScopedPointer< QEvent > ev(m_workQ.dequeue());
            locker.unlock();
            NewRequest(ev.data());
            locker.relock();
        }
    }

    m_running.acquire();

    delete m_nam;
    m_nam = nullptr;

    LOG(VB_FILE, LOG_INFO, LOC + "NAMThread stopped");
}

// slot
void NAMThread::quit()
{
    m_bQuit = true;
    QThread::quit();
}

void NAMThread::Post(QEvent *event)
{
    QMutexLocker locker(&m_mutex);
    m_workQ.enqueue(event);
}

bool NAMThread::NewRequest(QEvent *event)
{
    switch (event->type())
    {
    case NetStreamRequest::kType:
        return StartRequest(dynamic_cast< NetStreamRequest* >(event));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
    case NetStreamAbort::kType:
        return AbortRequest(dynamic_cast< NetStreamAbort* >(event));
#pragma GCC diagnostic pop
    default:
        break;
    }
    return false;
}

bool NAMThread::StartRequest(NetStreamRequest *p)
{
    if (!p)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid NetStreamRequest");
        return false;
    }

    if (!p->m_bCancelled)
    {
        QNetworkReply *reply = m_nam->get(p->m_req);
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("(%1) StartRequest 0x%2")
            .arg(p->m_id).arg(quintptr(reply),0,16) );
        emit requestStarted(p->m_id, reply);
    }
    else
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) NetStreamRequest cancelled").arg(p->m_id) );
    }
    return true;
}

bool NAMThread::AbortRequest(NetStreamAbort *p)
{
    if (!p)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid NetStreamAbort");
        return false;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("(%1) AbortRequest 0x%2").arg(p->m_id)
        .arg(quintptr(p->m_reply),0,16) );
    p->m_reply->abort();
    p->m_reply->disconnect();
    delete p->m_reply;
    return true;
}

// static
bool NAMThread::isAvailable()
{
    auto interfaces = QNetworkInterface::allInterfaces();
    return std::any_of(interfaces.begin(), interfaces.end(),
		       [](const QNetworkInterface& iface)
			   {
                               auto f = iface.flags();
                               if (f.testFlag(QNetworkInterface::IsLoopBack))
                                   return false;
                               return f.testFlag(QNetworkInterface::IsRunning);
                           } );
}

// Time when URI was last written to cache or invalid if not cached.
// static
QDateTime NAMThread::GetLastModified(const QUrl &url)
{
    NAMThread &m = manager();

    QMutexLocker locker(&m.m_mutex);

    if (!m.m_nam)
        return {}; // Invalid

    QAbstractNetworkCache *cache = m.m_nam->cache();
    if (!cache)
        return {}; // Invalid

    QNetworkCacheMetaData meta = cache->metaData(url);
    if (!meta.isValid())
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetLastModified('%1') not in cache")
            .arg(url.toString()));
        return {}; // Invalid
    }

    // Check if expired
    QDateTime const now(QDateTime::currentDateTime()); // local time
    QDateTime expire = meta.expirationDate();
    if (expire.isValid() && expire.toLocalTime() < now)
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("GetLastModified('%1') past expiration %2")
            .arg(url.toString(), expire.toString()));
        return {}; // Invalid
    }

    // Get time URI was modified (Last-Modified header)  NB this may be invalid
    QDateTime lastMod = meta.lastModified();

    QNetworkCacheMetaData::RawHeaderList headers = meta.rawHeaders();
    for (const auto& h : std::as_const(headers))
    {
        // RFC 1123 date format: Thu, 01 Dec 1994 16:00:00 GMT
        static const QString kSzFormat { "ddd, dd MMM yyyy HH:mm:ss 'GMT'" };

        QString const first(h.first.toLower());
        if (first == "cache-control")
        {
            QString const second(h.second.toLower());
            if (second == "no-cache" || second == "no-store")
            {
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("GetLastModified('%1') Cache-Control disabled")
                        .arg(url.toString()) );
                cache->remove(url);
                return {}; // Invalid
            }
        }
        else if (first == "date")
        {
            QDateTime d = QDateTime::fromString(h.second, kSzFormat);
            if (!d.isValid())
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("GetLastModified invalid Date header '%1'")
                    .arg(h.second.constData()));
                continue;
            }
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
            d.setTimeSpec(Qt::UTC);
#else
            d.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
            lastMod = d;
        }
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetLastModified('%1') last modified %2")
        .arg(url.toString(), lastMod.toString()));
    return lastMod;
}

/* End of file */
