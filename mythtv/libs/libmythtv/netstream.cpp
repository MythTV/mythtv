/* Network stream
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#include "netstream.h"

// C/C++ lib
#include <cstdlib>
using std::getenv;
#include <cstddef>
#include <cstdio>

// Qt
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkDiskCache>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QUrl>
#include <QThread>
#include <QMutexLocker>
#include <QEvent>
#include <QCoreApplication>
#include <QAtomicInt>
#include <QMetaType> // qRegisterMetaType
#include <QDesktopServices>
#include <QScopedPointer>

// Myth
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythdirs.h"


/*
 * Constants
 */
#define LOC "[netstream] "


/*
 * Private data
 */
static QAtomicInt s_nRequest(1); // Unique NetStream request ID
static QMutex s_mtx; // Guard local static data e.g. NAMThread singleton


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
        m_req(req),
        m_bCancelled(false)
    { }

    const int m_id;
    const QNetworkRequest m_req;
    volatile bool m_bCancelled;
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
NetStream::NetStream(const QUrl &url, EMode mode /*= kPreferCache*/) :
    m_id(s_nRequest.fetchAndAddRelaxed(1)),
    m_state(kClosed),
    m_pending(0),
    m_reply(0),
    m_nRedirections(0),
    m_size(-1),
    m_pos(0)
{
    setObjectName("NetStream " + url.toString());

    m_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
        mode == kAlwaysCache ? QNetworkRequest::AlwaysCache :
        mode == kPreferCache ? QNetworkRequest::PreferCache :
        mode == kNeverCache ? QNetworkRequest::AlwaysNetwork :
            QNetworkRequest::PreferNetwork );

    // Receive requestStarted signals from NAMThread when it processes a NetStreamRequest
    connect(&NAMThread::manager(), SIGNAL(requestStarted(int, QNetworkReply*)),
        this, SLOT(slotRequestStarted(int, QNetworkReply*)), Qt::DirectConnection );

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
        m_pending = 0;
    }

    if (m_reply)
    {
        // Abort the current request
        // NB the abort method appears to only work if called from NAMThread
        m_reply->disconnect(this);
        NAMThread::PostEvent(new NetStreamAbort(m_id, m_reply));
        // NAMthread will delete the reply
        m_reply = 0;
    }

    m_request.setUrl(url);

    const QByteArray ua("User-Agent");
    if (!m_request.hasRawHeader(ua))
        m_request.setRawHeader(ua, "UK-MHEG/2 MYT001/001 MHGGNU/001");

    if (m_pos > 0 || m_size >= 0)
        m_request.setRawHeader("Range", QString("bytes=%1-").arg(m_pos).toLatin1());

#ifndef QT_NO_OPENSSL
#if 1 // The BBC use a self certified cert so don't verify it
    if (m_request.url().scheme() == "https")
    {
        // TODO use cert from carousel auth.tls.<x>
        QSslConfiguration ssl(QSslConfiguration::defaultConfiguration());
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        m_request.setSslConfiguration(ssl);
    }
#endif
#endif

    LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Requesting %2 from %3")
        .arg(m_id).arg(m_request.url().toString()).arg(Source(m_request)) );
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

    m_pending = 0; // Event is no longer valid

    if (!m_reply)
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("(%1) Started %2-").arg(m_id).arg(m_pos) );

        m_reply = reply;
        m_state = kStarted;

        reply->setReadBufferSize(4*1024*1024L); // 0= unlimited, 1MB => 4secs @ 1.5Mbps

        // NB The following signals must be Qt::DirectConnection 'cos this slot
        // was connected Qt::DirectConnection so the current thread is NAMThread

        // QNetworkReply signals
        connect(reply, SIGNAL(finished()), this, SLOT(slotFinished()), Qt::DirectConnection );
#ifndef QT_NO_OPENSSL
        connect(reply, SIGNAL(sslErrors(const QList<QSslError> &)), this,
            SLOT(slotSslErrors(const QList<QSslError> &)), Qt::DirectConnection );
#endif
        // QIODevice signals
        connect(reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()), Qt::DirectConnection );
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("(%1) Started but m_reply not NULL").arg(m_id));
}

static qlonglong inline ContentLength(const QNetworkReply *reply)
{
    bool ok;
    qlonglong len = reply->header(QNetworkRequest::ContentLengthHeader)
        .toLongLong(&ok);
    return ok ? len : -1;
}

static qlonglong inline ContentRange(const QNetworkReply *reply,
    qlonglong &first, qlonglong &last)
{
    first = last = -1;

    QByteArray range = reply->rawHeader("Content-Range");
    if (range.isEmpty())
        return -1;

    // See RFC 2616 14.16: 'bytes begin-end/size'
    qlonglong len;
    if (3 != std::sscanf(range.constData(), " bytes %lld - %lld / %lld", &first, &last, &len))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid Content-Range:'%1'")
            .arg(range.constData()) );
        return -1;
    }

    return len;
}

static bool inline RequestRange(const QNetworkRequest &request,
    qlonglong &first, qlonglong &last)
{
    first = last = -1;

    QByteArray range = request.rawHeader("Range");
    if (range.isEmpty())
        return false;

    if (1 > std::sscanf(range.constData(), " bytes %lld - %lld", &first, &last))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Invalid Range:'%1'")
            .arg(range.constData()) );
        return false;
    }

    return true;
}

// signal from QNetworkReply
void NetStream::slotReadyRead()
{
    QMutexLocker locker(&m_mutex);

    if (m_reply)
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("(%1) Ready %2 bytes")
            .arg(m_id).arg(m_reply->bytesAvailable()) );

        if (m_size < 0)
        {
            qlonglong first, last, len = ContentRange(m_reply, first, last);
            if (len >= 0)
            {
                m_size = len;
                LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) range %2-%3/%4")
                    .arg(m_id).arg(first).arg(last).arg(len) );
            }
            else
            {
                m_size = ContentLength(m_reply);
                LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) content length %2")
                    .arg(m_id).arg(m_size) );
            }
        }

        if (m_state < kReady)
            m_state = kReady;

        locker.unlock();
        emit ReadyRead(this);
        locker.relock();

        m_ready.wakeAll();
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("(%1) ReadyRead but m_reply = NULL").arg(m_id));
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
            else if ((url = m_request.url().resolved(url)) == m_request.url())
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
        else
        {
            LOG(VB_FILE, LOG_WARNING, LOC + QString("(%1): %2")
                .arg(m_id).arg(m_reply->errorString()) );
            m_state = kFinished;
        }

        if (m_state == kFinished)
        {
            LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Finished %2/%3 bytes from %4")
                .arg(m_id).arg(m_pos).arg(m_size).arg(Source(m_reply)) );

            locker.unlock();
            emit Finished(this);
            locker.relock();

            m_finished.wakeAll();
        }
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("(%1) Finished but m_reply = NULL")
            .arg(m_id));
}

#ifndef QT_NO_OPENSSL
// signal from QNetworkReply
void NetStream::slotSslErrors(const QList<QSslError> &errors)
{
    QMutexLocker locker(&m_mutex);

    if (m_reply)
    {
        bool bIgnore = true;
        Q_FOREACH(const QSslError &e, errors)
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
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("(%1) SSL error but m_reply = NULL").arg(m_id) );
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
        m_pending = 0;
    }

    if (m_reply && m_reply->isRunning())
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) Abort").arg(m_id) );
        NAMThread::PostEvent(new NetStreamAbort(m_id, m_reply));
        // NAMthread will delete the reply
        m_reply = 0;
    }

    m_state = kFinished;
}

int NetStream::safe_read(void *data, unsigned sz, unsigned millisecs /* = 0 */)
{
    QTime t; t.start();
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
bool NetStream::WaitTillReady(unsigned long time)
{
    QMutexLocker locker(&m_mutex);

    QTime t; t.start();
    while (m_state < kReady)
    {
        unsigned elapsed = t.elapsed();
        if (elapsed > time)
            return false;

        m_ready.wait(&m_mutex, time - elapsed);
    }

    return true;
}

bool NetStream::WaitTillFinished(unsigned long time)
{
    QMutexLocker locker(&m_mutex);

    QTime t; t.start();
    while (m_state < kFinished)
    {
        unsigned elapsed = t.elapsed();
        if (elapsed > time)
            return false;

        m_finished.wait(&m_mutex, time - elapsed);
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
        return 0;

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
QDateTime NetStream::GetLastModified(const QString &url)
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
    static NAMThread thread;
    thread.start();
    return thread;
}

NAMThread::NAMThread() : m_bQuit(false), m_nam(0)
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
    LOG(VB_MHEG, LOG_INFO, LOC "NAMThread starting");

    m_nam = new QNetworkAccessManager();
    m_nam->setObjectName("NetStream NAM");

    // Setup cache
    QScopedPointer<QNetworkDiskCache> cache(new QNetworkDiskCache());
    cache->setCacheDirectory(
        QDesktopServices::storageLocation(QDesktopServices::CacheLocation) );
    m_nam->setCache(cache.take());

    // Setup a network proxy e.g. for TOR: socks://localhost:9050
    // TODO get this from mythdb
    QString proxy(getenv("HTTP_PROXY"));
    if (!proxy.isEmpty())
    {
        QUrl url(proxy, QUrl::TolerantMode);
        QNetworkProxy::ProxyType type =
            url.scheme().isEmpty() ? QNetworkProxy::HttpProxy :
            url.scheme() == "socks" ? QNetworkProxy::Socks5Proxy :
            url.scheme() == "http" ? QNetworkProxy::HttpProxy :
            url.scheme() == "https" ? QNetworkProxy::HttpProxy :
            url.scheme() == "cache" ? QNetworkProxy::HttpCachingProxy :
            url.scheme() == "ftp" ? QNetworkProxy::FtpCachingProxy :
            QNetworkProxy::NoProxy;
        if (QNetworkProxy::NoProxy != type)
        {
            LOG(VB_MHEG, LOG_INFO, LOC "Using proxy: " + proxy);
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
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(quit()) );

    m_running.release();

    while(!m_bQuit)
    {
        // Process NAM events
        QCoreApplication::processEvents();

        QMutexLocker locker(&m_mutex);
        m_work.wait(&m_mutex, 100);
        while (!m_workQ.isEmpty())
        {
            QScopedPointer< QEvent > ev(m_workQ.dequeue());
            locker.unlock();
            NewRequest(ev.data());
        }
    }

    m_running.acquire();

    delete m_nam;
    m_nam = 0;

    LOG(VB_MHEG, LOG_INFO, LOC "NAMThread stopped");
}

// slot
void NAMThread::quit()
{
    m_bQuit = true;
    QThread::quit();
}

// static
void NAMThread::PostEvent(QEvent *event)
{
    NAMThread &m = manager();
    QMutexLocker locker(&m.m_mutex);
    m.m_workQ.enqueue(event);
}

bool NAMThread::NewRequest(QEvent *event)
{
    switch (event->type())
    {
    case NetStreamRequest::kType:
        return StartRequest(dynamic_cast< NetStreamRequest* >(event));
    case NetStreamAbort::kType:
        return AbortRequest(dynamic_cast< NetStreamAbort* >(event));
    default:
        break;
    }
    return false;
}

bool NAMThread::StartRequest(NetStreamRequest *p)
{
    if (!p)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC "Invalid NetStreamRequest");
        return false;
    }

    if (!p->m_bCancelled)
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("(%1) StartRequest").arg(p->m_id) );
        QNetworkReply *reply = m_nam->get(p->m_req);
        emit requestStarted(p->m_id, reply);
    }
    else
        LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) NetStreamRequest cancelled").arg(p->m_id) );
    return true;
}

bool NAMThread::AbortRequest(NetStreamAbort *p)
{
    if (!p)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC "Invalid NetStreamAbort");
        return false;
    }

    LOG(VB_FILE, LOG_INFO, LOC + QString("(%1) AbortRequest").arg(p->m_id) );
    p->m_reply->abort();
    p->m_reply->disconnect();
    delete p->m_reply;
    return true;
}

// static
bool NAMThread::isAvailable()
{
    NAMThread &m = manager();

    if (!m.m_running.tryAcquire(1, 3000))
        return false;

    m.m_running.release();

    QMutexLocker locker(&m.m_mutex);

    if (!m.m_nam)
        return false;

    switch (m.m_nam->networkAccessible())
    {
    case QNetworkAccessManager::Accessible: return true;
    case QNetworkAccessManager::NotAccessible: return false;
    case QNetworkAccessManager::UnknownAccessibility: return true;
    }
    return false;
}

// Time when URI was last written to cache or invalid if not cached.
// static
QDateTime NAMThread::GetLastModified(const QString &url)
{
    NAMThread &m = manager();

    QMutexLocker locker(&m.m_mutex);

    if (!m.m_nam)
        return QDateTime(); // Invalid

    QAbstractNetworkCache *cache = m.m_nam->cache();
    if (!cache)
        return QDateTime(); // Invalid

    QNetworkCacheMetaData meta = cache->metaData(QUrl(url));
    if (!meta.isValid())
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetLastModified('%1') not in cache")
            .arg(url));
        return QDateTime(); // Invalid
    }

    // Check if expired
    QDateTime const now(QDateTime::currentDateTime()); // local time
    QDateTime expire = meta.expirationDate();
    if (expire.isValid() && expire.toLocalTime() < now)
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("GetLastModified('%1') past expiration %2")
            .arg(url).arg(expire.toString()));
        return QDateTime(); // Invalid
    }

    // Get time URI was modified (Last-Modified header)  NB this may be invalid
    QDateTime lastMod = meta.lastModified();

    QNetworkCacheMetaData::RawHeaderList headers = meta.rawHeaders();
    Q_FOREACH(const QNetworkCacheMetaData::RawHeader &h, headers)
    {
        // RFC 1123 date format: Thu, 01 Dec 1994 16:00:00 GMT
        static const char kszFormat[] = "ddd, dd MMM yyyy HH:mm:ss 'GMT'";

        QString const first(h.first.toLower());
        if (first == "cache-control")
        {
            QString const second(h.second.toLower());
            if (second == "no-cache" || second == "no-store")
            {
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("GetLastModified('%1') Cache-Control disabled").arg(url));
                cache->remove(QUrl(url));
                return QDateTime(); // Invalid
            }
        }
        else if (first == "date")
        {
            QDateTime d = QDateTime::fromString(h.second, kszFormat);
            if (!d.isValid())
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("GetLastModified invalid Date header '%1'")
                    .arg(h.second.constData()));
                continue;
            }
            d.setTimeSpec(Qt::UTC);
            lastMod = d;
        }
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetLastModified('%1') last modified %2")
        .arg(url).arg(lastMod.toString()));
    return lastMod;
}

/* End of file */
