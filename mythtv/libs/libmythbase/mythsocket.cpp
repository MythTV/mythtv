// Qt
#include <QNetworkInterface> // for QNetworkInterface::allAddresses ()
#include <QCoreApplication>
#include <QWaitCondition>
#include <QSharedPointer>
#include <QByteArray>
#include <QTcpSocket>
#include <QHostInfo>
#include <QThread>
#include <QMetaType>

// setsockopt -- has to be after Qt includes for Q_OS_WIN definition
#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#else
#include <sys/socket.h>
#endif
#include <unistd.h> // for usleep (and socket code on Q_OS_WIN)
#include <algorithm> // for max
#include <vector> // for vector

// MythTV
#include "mythsocket.h"
#include "mythtimer.h"
#include "mythevent.h"
#include "mythversion.h"
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "portchecker.h"

const int MythSocket::kSocketReceiveBufferSize = 128 * 1024;

QMutex MythSocket::s_loopbackCacheLock;
QHash<QString, QHostAddress::SpecialAddress> MythSocket::s_loopbackCache;

QMutex MythSocket::s_thread_lock;
MThread *MythSocket::s_thread = nullptr;
int MythSocket::s_thread_cnt = 0;

Q_DECLARE_METATYPE ( const QStringList * );
Q_DECLARE_METATYPE ( QStringList * );
Q_DECLARE_METATYPE ( const char * );
Q_DECLARE_METATYPE ( char * );
Q_DECLARE_METATYPE ( bool * );
Q_DECLARE_METATYPE ( int * );
Q_DECLARE_METATYPE ( QHostAddress );
static int x0 = qRegisterMetaType< const QStringList * >();
static int x1 = qRegisterMetaType< QStringList * >();
static int x2 = qRegisterMetaType< const char * >();
static int x3 = qRegisterMetaType< char * >();
static int x4 = qRegisterMetaType< bool * >();
static int x5 = qRegisterMetaType< int * >();
static int x6 = qRegisterMetaType< QHostAddress >();
int s_dummy_meta_variable_to_suppress_gcc_warning =
    x0 + x1 + x2 + x3 + x4 + x5 + x6;

static QString to_sample(const QByteArray &payload)
{
    QString sample("");
    for (uint i = 0; (i<60) && (i<(uint)payload.length()); i++)
    {
        sample += QChar(payload[i]).isPrint() ?
            QChar(payload[i]) : QChar('?');
    }
    sample += (payload.length() > 60) ? "..." : "";
    return sample;
}

MythSocket::MythSocket(
    qintptr socket, MythSocketCBs *cb, bool use_shared_thread) :
    ReferenceCounter(QString("MythSocket(%1)").arg(socket)),
    m_tcpSocket(new QTcpSocket()),
    m_callback(cb),
    m_useSharedThread(use_shared_thread)
{
    LOG(VB_SOCKET, LOG_INFO, LOC() + QString("MythSocket(%1, 0x%2) ctor")
        .arg(socket).arg((intptr_t)(cb),0,16));

    if (socket != -1)
    {
        m_tcpSocket->setSocketDescriptor(
            socket, QAbstractSocket::ConnectedState,
            QAbstractSocket::ReadWrite);
        if (!gCoreContext->CheckSubnet(m_tcpSocket))
        {
            m_tcpSocket->abort();
            m_connected = false;
            m_useSharedThread = false;
            return;
        }
        ConnectHandler(); // already called implicitly above?
    }

    // Use direct connections so m_tcpSocket can be used
    // in the handlers safely since they will be running
    // in the same thread as all other m_tcpSocket users.

    connect(m_tcpSocket,  &QAbstractSocket::connected,
            this, &MythSocket::ConnectHandler,
            Qt::DirectConnection);
    connect(m_tcpSocket,  &QAbstractSocket::errorOccurred,
            this, &MythSocket::ErrorHandler,
            Qt::DirectConnection);
    connect(m_tcpSocket,  &QIODevice::aboutToClose,
            this, &MythSocket::AboutToCloseHandler);
    connect(m_tcpSocket,  &QAbstractSocket::disconnected,
            this, &MythSocket::DisconnectHandler,
            Qt::DirectConnection);
    connect(m_tcpSocket,  &QIODevice::readyRead,
            this, &MythSocket::ReadyReadHandler,
            Qt::DirectConnection);

    connect(this, &MythSocket::CallReadyRead,
            this, &MythSocket::CallReadyReadHandler,
            Qt::QueuedConnection);

    if (!use_shared_thread)
    {
        m_thread = new MThread(QString("MythSocketThread(%1)").arg(socket));
        m_thread->start();
    }
    else
    {
        QMutexLocker locker(&s_thread_lock);
        if (!s_thread)
        {
            s_thread = new MThread("SharedMythSocketThread");
            s_thread->start();
        }
        m_thread = s_thread;
        s_thread_cnt++;
    }

    m_tcpSocket->moveToThread(m_thread->qthread());
    moveToThread(m_thread->qthread());
}

MythSocket::~MythSocket()
{
    LOG(VB_SOCKET, LOG_INFO, LOC() + QString("MythSocket dtor : cb 0x%2")
        .arg((intptr_t)(m_callback),0,16));

    if (IsConnected())
        DisconnectFromHost();

    if (!m_useSharedThread)
    {
        if (m_thread)
        {
            m_thread->quit();
            m_thread->wait();
            delete m_thread;
        }
    }
    else
    {
        QMutexLocker locker(&s_thread_lock);
        s_thread_cnt--;
        if (0 == s_thread_cnt)
        {
            s_thread->quit();
            s_thread->wait();
            delete s_thread;
            s_thread = nullptr;
        }
    }
    m_thread = nullptr;

    delete m_tcpSocket;
    m_tcpSocket = nullptr;
}

void MythSocket::ConnectHandler(void)
{
    {
        QMutexLocker locker(&m_lock);
        m_connected = true;
        m_socketDescriptor = m_tcpSocket->socketDescriptor();
        m_peerAddress = m_tcpSocket->peerAddress();
        m_peerPort = m_tcpSocket->peerPort();
    }

    m_tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, QVariant(1));
    m_tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, QVariant(1));

    int reuse_addr_val = 1;
#if defined(Q_OS_WIN)
    int ret = setsockopt(m_tcpSocket->socketDescriptor(), SOL_SOCKET,
                         SO_REUSEADDR, (char*) &reuse_addr_val,
                         sizeof(reuse_addr_val));
#else
    int ret = setsockopt(m_tcpSocket->socketDescriptor(), SOL_SOCKET,
                         SO_REUSEADDR, &reuse_addr_val,
                         sizeof(reuse_addr_val));
#endif
    if (ret < 0)
    {
        LOG(VB_SOCKET, LOG_INFO, LOC() + "Failed to set SO_REUSEADDR" + ENO);
    }

    int rcv_buf_val = kSocketReceiveBufferSize;
#if defined(Q_OS_WIN)
    ret = setsockopt(m_tcpSocket->socketDescriptor(), SOL_SOCKET,
                     SO_RCVBUF, (char*) &rcv_buf_val,
                     sizeof(rcv_buf_val));
#else
    ret = setsockopt(m_tcpSocket->socketDescriptor(), SOL_SOCKET,
                     SO_RCVBUF, &rcv_buf_val,
                     sizeof(rcv_buf_val));
#endif
    if (ret < 0)
    {
        LOG(VB_SOCKET, LOG_INFO, LOC() + "Failed to set SO_RCVBUF" + ENO);
    }

    if (m_callback)
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC() +
            "calling m_callback->connected()");
        m_callback->connected(this);
    }
}

void MythSocket::ErrorHandler(QAbstractSocket::SocketError err)
{
    // Filter these out, we get them because we call waitForReadyRead with a
    // small timeout so we can print our own debugging for long timeouts.
    if (err == QAbstractSocket::SocketTimeoutError)
        return;

    if (m_callback)
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC() +
            "calling m_callback->error() err: " + m_tcpSocket->errorString());
        m_callback->error(this, (int)err);
    }
}

void MythSocket::DisconnectHandler(void)
{
    {
        QMutexLocker locker(&m_lock);
        m_connected = false;
        m_socketDescriptor = -1;
        m_peerAddress.clear();
        m_peerPort = -1;
    }

    if (m_callback)
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC() +
            "calling m_callback->connectionClosed()");
        m_callback->connectionClosed(this);
    }
}

void MythSocket::AboutToCloseHandler(void)
{
    LOG(VB_SOCKET, LOG_DEBUG, LOC() + "AboutToClose");
}

void MythSocket::ReadyReadHandler(void)
{
    m_dataAvailable.fetchAndStoreOrdered(1);
    if (m_callback && m_disableReadyReadCallback.testAndSetOrdered(0,0))
    {
        emit CallReadyRead();
    }
}

void MythSocket::CallReadyReadHandler(void)
{
    // Because the connection to this is a queued connection the
    // data may have already been read by the time this is called
    // so we check that there is still data to read before calling
    // the callback.
    if (IsDataAvailable())
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC() +
            "calling m_callback->readyRead()");
        m_callback->readyRead(this);
    }
}

bool MythSocket::ConnectToHost(
    const QHostAddress &address, quint16 port)
{
    bool ret = false;
    QMetaObject::invokeMethod(
        this, "ConnectToHostReal",
        (QThread::currentThread() != m_thread->qthread()) ?
        Qt::BlockingQueuedConnection : Qt::DirectConnection,
        Q_ARG(QHostAddress, address),
        Q_ARG(quint16, port),
        Q_ARG(bool*, &ret));
    return ret;
}

bool MythSocket::WriteStringList(const QStringList &list)
{
    bool ret = false;
    QMetaObject::invokeMethod(
        this, "WriteStringListReal",
        (QThread::currentThread() != m_thread->qthread()) ?
        Qt::BlockingQueuedConnection : Qt::DirectConnection,
        Q_ARG(const QStringList*, &list),
        Q_ARG(bool*, &ret));
    return ret;
}

bool MythSocket::ReadStringList(QStringList &list, std::chrono::milliseconds timeoutMS)
{
    bool ret = false;
    QMetaObject::invokeMethod(
        this, "ReadStringListReal",
        (QThread::currentThread() != m_thread->qthread()) ?
        Qt::BlockingQueuedConnection : Qt::DirectConnection,
        Q_ARG(QStringList*, &list),
        Q_ARG(std::chrono::milliseconds, timeoutMS),
        Q_ARG(bool*, &ret));
    return ret;
}

bool MythSocket::SendReceiveStringList(
    QStringList &strlist, uint min_reply_length, std::chrono::milliseconds timeoutMS)
{
    if (m_callback && m_disableReadyReadCallback.testAndSetOrdered(0,0))
    {
        // If callbacks are enabled then SendReceiveStringList() will conflict
        // causing failed reads and socket disconnections - see #11777
        // SendReceiveStringList() should NOT be used with an event socket, only
        // the control socket
        LOG(VB_GENERAL, LOG_EMERG, QString("Programmer Error! "
                                           "SendReceiveStringList(%1) used on "
                                           "socket with callbacks enabled.")
                                .arg(strlist.isEmpty() ? "empty" : strlist[0]));
    }

    if (!WriteStringList(strlist))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() + "Failed to send command.");
        return false;
    }

    if (!ReadStringList(strlist, timeoutMS))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() + "No response.");
        return false;
    }

    if (min_reply_length && ((uint)strlist.size() < min_reply_length))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() + "Response too short.");
        return false;
    }

#if 0
    if (!strlist.empty() && strlist[0] == "BACKEND_MESSAGE")
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() + "Got MythEvent on non-event socket");
        return false;
    }
#endif

    return true;
}

/**
 *  \brief connect to host
 *  \return true on success
 */
bool MythSocket::ConnectToHost(const QString &host, quint16 port)
{
    QHostAddress hadr;

    // attempt direct assignment
    if (!hadr.setAddress(host))
    {
        // attempt internal lookup through MythCoreContext
        if (!gCoreContext ||
            !hadr.setAddress(gCoreContext->GetBackendServerIP(host)))
        {
            // attempt external lookup from hosts/DNS
            QHostInfo info = QHostInfo::fromName(host);
            if (!info.addresses().isEmpty())
            {
                hadr = info.addresses().constFirst();
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC() + QString("Unable to lookup: %1")
                        .arg(host));
                return false;
            }
        }
    }

    return MythSocket::ConnectToHost(hadr, port);
}

bool MythSocket::Validate(std::chrono::milliseconds timeout, bool error_dialog_desired)
{
    if (m_isValidated)
        return true;

    QStringList strlist(QString("MYTH_PROTO_VERSION %1 %2")
                        .arg(MYTH_PROTO_VERSION,
                             QString::fromUtf8(MYTH_PROTO_TOKEN)));

    WriteStringList(strlist);

    if (!ReadStringList(strlist, timeout) || strlist.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Protocol version check failure.\n\t\t\t"
                "The response to MYTH_PROTO_VERSION was empty.\n\t\t\t"
                "This happens when the backend is too busy to respond,\n\t\t\t"
                "or has deadlocked due to bugs or hardware failure.");
        return m_isValidated;
    }

    if (strlist[0] == "REJECT" && (strlist.size() >= 2))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Protocol version or token mismatch "
                    "(frontend=%1/%2,backend=%3/\?\?)\n")
            .arg(MYTH_PROTO_VERSION,
                 QString::fromUtf8(MYTH_PROTO_TOKEN),
                 strlist[1]));

        QObject *GUIcontext = gCoreContext->GetGUIContext();
        if (error_dialog_desired && GUIcontext)
        {
            QStringList list(strlist[1]);
            QCoreApplication::postEvent(
                GUIcontext, new MythEvent("VERSION_MISMATCH", list));
        }
    }
    else if (strlist[0] == "ACCEPT")
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Using protocol version %1 %2")
            .arg(MYTH_PROTO_VERSION, QString::fromUtf8(MYTH_PROTO_TOKEN)));
        m_isValidated = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Unexpected response to MYTH_PROTO_VERSION: %1")
            .arg(strlist[0]));
    }

    return m_isValidated;
}

bool MythSocket::Announce(const QStringList &new_announce)
{
    if (!m_isValidated)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            "refusing to announce unvalidated socket");
        return false;
    }

    if (m_isAnnounced)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() + "refusing to re-announce socket");
        return false;
    }

    WriteStringList(new_announce);

    QStringList tmplist;
    if (!ReadStringList(tmplist, MythSocket::kShortTimeout))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            QString("\n\t\t\tCould not read string list from server %1:%2")
            .arg(m_tcpSocket->peerAddress().toString())
            .arg(m_tcpSocket->peerPort()));
        m_announce.clear();
        m_isAnnounced = false;
    }
    else
    {
        m_announce = new_announce;
        m_isAnnounced = true;
    }

    return m_isAnnounced;
}

void MythSocket::SetAnnounce(const QStringList &new_announce)
{
    m_announce = new_announce;
    m_isAnnounced = true;
}

void MythSocket::DisconnectFromHost(void)
{
    if (QThread::currentThread() != m_thread->qthread() &&
        gCoreContext && gCoreContext->IsExiting())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            QString("Programmer error, QEventLoop isn't running and deleting "
                    "MythSocket(0x%1)").arg(reinterpret_cast<intptr_t>(this),0,16));
        return;
    }
    QMetaObject::invokeMethod(
        this, "DisconnectFromHostReal",
        (QThread::currentThread() != m_thread->qthread()) ?
        Qt::BlockingQueuedConnection : Qt::DirectConnection);
}

int MythSocket::Write(const char *data, int size)
{
    int ret = -1;
    QMetaObject::invokeMethod(
        this, "WriteReal",
        (QThread::currentThread() != m_thread->qthread()) ?
        Qt::BlockingQueuedConnection : Qt::DirectConnection,
        Q_ARG(const char*, data),
        Q_ARG(int, size),
        Q_ARG(int*, &ret));
    return ret;
}

int MythSocket::Read(char *data, int size,  std::chrono::milliseconds max_wait)
{
    int ret = -1;
    QMetaObject::invokeMethod(
        this, "ReadReal",
        (QThread::currentThread() != m_thread->qthread()) ?
        Qt::BlockingQueuedConnection : Qt::DirectConnection,
        Q_ARG(char*, data),
        Q_ARG(int, size),
        Q_ARG(std::chrono::milliseconds, max_wait),
        Q_ARG(int*, &ret));
    return ret;
}

void MythSocket::Reset(void)
{
    QMetaObject::invokeMethod(
        this, "ResetReal",
        (QThread::currentThread() != m_thread->qthread()) ?
        Qt::BlockingQueuedConnection : Qt::DirectConnection);
}

//////////////////////////////////////////////////////////////////////////

bool MythSocket::IsConnected(void) const
{
    QMutexLocker locker(&m_lock);
    return m_connected;
}

bool MythSocket::IsDataAvailable(void)
{
    if (QThread::currentThread() == m_thread->qthread())
        return m_tcpSocket->bytesAvailable() > 0;

    if (m_dataAvailable.testAndSetOrdered(0,0))
        return false;

    bool ret = false;

    QMetaObject::invokeMethod(
        this, "IsDataAvailableReal",
        Qt::BlockingQueuedConnection,
        Q_ARG(bool*, &ret));

    return ret;
}

int MythSocket::GetSocketDescriptor(void) const
{
    QMutexLocker locker(&m_lock);
    return m_socketDescriptor;
}

QHostAddress MythSocket::GetPeerAddress(void) const
{
    QMutexLocker locker(&m_lock);
    return m_peerAddress;
}

int MythSocket::GetPeerPort(void) const
{
    QMutexLocker locker(&m_lock);
    return m_peerPort;
}

//////////////////////////////////////////////////////////////////////////

void MythSocket::IsDataAvailableReal(bool *ret) const
{
    *ret = (m_tcpSocket->bytesAvailable() > 0);
    m_dataAvailable.fetchAndStoreOrdered((*ret) ? 1 : 0);
}

void MythSocket::ConnectToHostReal(const QHostAddress& _addr, quint16 port, bool *ret)
{
    if (m_tcpSocket->state() == QAbstractSocket::ConnectedState)
    {
        LOG(VB_SOCKET, LOG_ERR, LOC() +
            "connect() called with already open socket, closing");
        m_tcpSocket->close();
    }

    QHostAddress addr = _addr;
    addr.setScopeId(QString());

    s_loopbackCacheLock.lock();
    bool usingLoopback = s_loopbackCache.contains(addr.toString());
    s_loopbackCacheLock.unlock();

    if (usingLoopback)
    {
        addr = QHostAddress(s_loopbackCache.value(addr.toString()));
    }
    else
    {
        QList<QHostAddress> localIPs = QNetworkInterface::allAddresses();
        for (int i = 0; i < localIPs.count() && !usingLoopback; ++i)
        {
            QHostAddress local = localIPs[i];
            local.setScopeId(QString());

            if (addr == local)
            {
                QHostAddress::SpecialAddress loopback = QHostAddress::LocalHost;
                if (addr.protocol() == QAbstractSocket::IPv6Protocol)
                    loopback = QHostAddress::LocalHostIPv6;

                QMutexLocker locker(&s_loopbackCacheLock);
                s_loopbackCache[addr.toString()] = loopback;
                addr = QHostAddress(loopback);
                usingLoopback = true;
            }
        }
    }

    if (usingLoopback)
    {
        LOG(VB_SOCKET, LOG_INFO, LOC() +
            "IP is local, using loopback address instead");
    }

    LOG(VB_SOCKET, LOG_INFO, LOC() + QString("attempting connect() to (%1:%2)")
        .arg(addr.toString()).arg(port));

    bool ok = true;

    // Sort out link-local address scope if applicable
    if (!usingLoopback)
    {
        QString host = addr.toString();
        if (PortChecker::resolveLinkLocal(host, port))
            addr.setAddress(host);
    }

    if (ok)
    {
        m_tcpSocket->connectToHost(addr, port, QAbstractSocket::ReadWrite);
        ok = m_tcpSocket->waitForConnected(5000);
    }

    if (ok)
    {
        LOG(VB_SOCKET, LOG_INFO, LOC() + QString("Connected to (%1:%2)")
            .arg(addr.toString()).arg(port));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            QString("Failed to connect to (%1:%2) %3")
            .arg(addr.toString()).arg(port)
            .arg(m_tcpSocket->errorString()));
    }

    *ret = ok;
}

void MythSocket::DisconnectFromHostReal(void)
{
    m_tcpSocket->disconnectFromHost();
}

void MythSocket::WriteStringListReal(const QStringList *list, bool *ret)
{
    if (list->empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            "WriteStringList: Error, invalid string list.");
        *ret = false;
        return;
    }

    if (m_tcpSocket->state() != QAbstractSocket::ConnectedState)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            "WriteStringList: Error, called with unconnected socket.");
        *ret = false;
        return;
    }

    QString str = list->join("[]:[]");
    if (str.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            "WriteStringList: Error, joined null string.");
        *ret = false;
        return;
    }

    QByteArray utf8 = str.toUtf8();
    int size = utf8.length();
    int written = 0;
    int written_since_timer_restart = 0;

    QByteArray payload;
    payload = payload.setNum(size);
    payload += "        ";
    payload.truncate(8);
    payload += utf8;
    size = payload.length();

    if (VERBOSE_LEVEL_CHECK(VB_NETWORK, LOG_INFO))
    {
        QString msg = QString("write -> %1 %2")
            .arg(m_tcpSocket->socketDescriptor(), 2).arg(payload.data());

        if (logLevel < LOG_DEBUG && msg.length() > 128)
        {
            msg.truncate(127);
            msg += "…";
        }
        LOG(VB_NETWORK, LOG_INFO, LOC() + msg);
    }

    MythTimer timer; timer.start();
    unsigned int errorcount = 0;
    while (size > 0)
    {
        if (m_tcpSocket->state() != QAbstractSocket::ConnectedState)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC() +
                "WriteStringList: Error, socket went unconnected." +
                QString("\n\t\t\tWe wrote %1 of %2 bytes with %3 errors")
                    .arg(written).arg(written+size).arg(errorcount) +
                    QString("\n\t\t\tstarts with: %1").arg(to_sample(payload)));
            *ret = false;
            return;
        }

        int temp = m_tcpSocket->write(payload.data() + written, size);
        if (temp > 0)
        {
            written += temp;
            written_since_timer_restart += temp;
            size -= temp;
            if ((timer.elapsed() > 500ms) && written_since_timer_restart != 0)
            {
                timer.restart();
                written_since_timer_restart = 0;
            }
        }
        else
        {
            errorcount++;
            if (timer.elapsed() > 1s)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC() + "WriteStringList: Error, " +
                    QString("No data written on write (%1 errors)")
                        .arg(errorcount) +
                    QString("\n\t\t\tstarts with: %1")
                    .arg(to_sample(payload)));
                *ret = false;
                return;
            }
            usleep(1000);
        }
    }

    m_tcpSocket->flush();

    *ret = true;
}

void MythSocket::ReadStringListReal(
    QStringList *list, std::chrono::milliseconds timeoutMS, bool *ret)
{
    list->clear();
    *ret = false;

    MythTimer timer;
    timer.start();
    std::chrono::milliseconds elapsed { 0ms };

    while (m_tcpSocket->bytesAvailable() < 8)
    {
        elapsed = timer.elapsed();
        if (elapsed >= timeoutMS)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC() + "ReadStringList: " +
                QString("Error, timed out after %1 ms.").arg(timeoutMS.count()));
            m_tcpSocket->close();
            m_dataAvailable.fetchAndStoreOrdered(0);
            return;
        }

        if (m_tcpSocket->state() != QAbstractSocket::ConnectedState)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC() + "ReadStringList: Connection died.");
            m_dataAvailable.fetchAndStoreOrdered(0);
            return;
        }

        m_tcpSocket->waitForReadyRead(50);
    }

    QByteArray sizestr(8, '\0');
    if (m_tcpSocket->read(sizestr.data(), 8) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            QString("ReadStringList: Error, read return error (%1)")
                .arg(m_tcpSocket->errorString()));
        m_tcpSocket->close();
        m_dataAvailable.fetchAndStoreOrdered(0);
        return;
    }

    QString sizes = sizestr;
    bool ok { false };
    int btr = sizes.trimmed().toInt(&ok);

    if (btr < 1)
    {
        int pending = m_tcpSocket->bytesAvailable();
        LOG(VB_GENERAL, LOG_ERR, LOC() +
            QString("Protocol error: %1'%2' is not a valid size "
                    "prefix. %3 bytes pending.")
                .arg(ok ? "" : "(parse failed) ",
                     sizestr.data(), QString::number(pending)));
        ResetReal();
        return;
    }

    QByteArray utf8(btr + 1, 0);

    qint64 readoffset = 0;
    std::chrono::milliseconds errmsgtime { 0ms };
    timer.start();

    while (btr > 0)
    {
        if (m_tcpSocket->bytesAvailable() < 1)
        {
            if (m_tcpSocket->state() == QAbstractSocket::ConnectedState)
            {
                m_tcpSocket->waitForReadyRead(50);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC() +
                    "ReadStringList: Connection died.");
                m_dataAvailable.fetchAndStoreOrdered(0);
                return;
            }
        }

        qint64 sret = m_tcpSocket->read(utf8.data() + readoffset, btr);
        if (sret > 0)
        {
            readoffset += sret;
            btr -= sret;
            if (btr > 0)
            {
                timer.start();
            }
        }
        else if (sret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC() + "ReadStringList: Error, read");
            m_tcpSocket->close();
            m_dataAvailable.fetchAndStoreOrdered(0);
            return;
        }
        else if (!m_tcpSocket->isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC() +
                "ReadStringList: Error, socket went unconnected");
            m_tcpSocket->close();
            m_dataAvailable.fetchAndStoreOrdered(0);
            return;
        }
        else
        {
            elapsed = timer.elapsed();
            if (elapsed  > 10s)
            {
                if ((elapsed - errmsgtime) > 10s)
                {
                    errmsgtime = elapsed;
                    LOG(VB_GENERAL, LOG_ERR, LOC() +
                        QString("ReadStringList: Waiting for data: %1 %2")
                            .arg(readoffset).arg(btr));
                }
            }

            if (elapsed > 100s)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC() +
                    "Error, ReadStringList timeout (readBlock)");
                m_dataAvailable.fetchAndStoreOrdered(0);
                return;
            }
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    if (VERBOSE_LEVEL_CHECK(VB_NETWORK, LOG_INFO))
    {
        QByteArray payload;
        payload = payload.setNum(str.length());
        payload += "        ";
        payload.truncate(8);
        payload += utf8.data();

        QString msg = QString("read  <- %1 %2")
            .arg(m_tcpSocket->socketDescriptor(), 2)
            .arg(payload.data());

        if (logLevel < LOG_DEBUG && msg.length() > 128)
        {
            msg.truncate(127);
            msg += "…";
        }
        LOG(VB_NETWORK, LOG_INFO, LOC() + msg);
    }

    *list = str.split("[]:[]");

    m_dataAvailable.fetchAndStoreOrdered(
        (m_tcpSocket->bytesAvailable() > 0) ? 1 : 0);

    *ret = true;
}

void MythSocket::WriteReal(const char *data, int size, int *ret)
{
    *ret = m_tcpSocket->write(data, size);
}

void MythSocket::ReadReal(char *data, int size, std::chrono::milliseconds max_wait_ms, int *ret)
{
    MythTimer t; t.start();
    while ((m_tcpSocket->state() == QAbstractSocket::ConnectedState) &&
           (m_tcpSocket->bytesAvailable() < size) &&
           (t.elapsed() < max_wait_ms))
    {
        m_tcpSocket->waitForReadyRead(max(2ms, max_wait_ms - t.elapsed()).count());
    }
    *ret = m_tcpSocket->read(data, size);

    if (t.elapsed() > 50ms)
    {
        LOG(VB_NETWORK, LOG_INFO,
            QString("ReadReal(?, %1, %2) -> %3 took %4 ms")
            .arg(size).arg(max_wait_ms.count()).arg(*ret)
            .arg(t.elapsed().count()));
    }

    m_dataAvailable.fetchAndStoreOrdered(
        (m_tcpSocket->bytesAvailable() > 0) ? 1 : 0);
}

void MythSocket::ResetReal(void)
{
    uint avail {0};
    std::vector<char> trash;

    m_tcpSocket->waitForReadyRead(30);
    while ((avail = m_tcpSocket->bytesAvailable()) > 0)
    {
        trash.resize(std::max((uint)trash.size(),avail));
        m_tcpSocket->read(trash.data(), avail);

        LOG(VB_NETWORK, LOG_INFO, LOC() + "Reset() " +
            QString("%1 bytes available").arg(avail));

        m_tcpSocket->waitForReadyRead(30);
    }

    m_dataAvailable.fetchAndStoreOrdered(0);
}
