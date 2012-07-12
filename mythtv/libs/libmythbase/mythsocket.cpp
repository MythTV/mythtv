// ANSI C
#include <cstdlib>
#include <cassert>
#include <cerrno>

#include "compat.h"

// POSIX
#ifndef USING_MINGW
#include <sys/select.h> // for select
#endif

// Qt
#include <QByteArray>
#include <QHostInfo>
#include <QNetworkInterface> // for QNetworkInterface::allAddresses ()
#include <QAbstractSocket> // for QAbstractSocket::NetworkLayerProtocol
#include <QMap>
#include <QCoreApplication>

// MythTV
#include "mythsocketthread.h"
#include "mythsocket.h"
#include "mythtimer.h"
#include "mythsocket.h"
#include "mythevent.h"
#include "mythversion.h"
#include "mythlogging.h"
#include "mythcorecontext.h"

#define SLOC(a) QString("MythSocket(%1:%2): ")\
                    .arg((uint64_t)a, 0, 16).arg(a->socket())
#define LOC SLOC(this)

const uint MythSocket::kSocketBufferSize = 128000;
const uint MythSocket::kShortTimeout = kMythSocketShortTimeout;
const uint MythSocket::kLongTimeout  = kMythSocketLongTimeout;

QMutex MythSocket::s_readyread_thread_lock;
MythSocketThread *MythSocket::s_readyread_thread = NULL;

QMap<QString, QHostAddress::SpecialAddress> MythSocket::s_loopback_cache;

MythSocket::MythSocket(int socket, MythSocketCBs *cb) :
    MSocketDevice(MSocketDevice::Stream),
    ReferenceCounter(QString("MythSocket(%1)").arg(socket)),
    m_cb(cb),                   m_useReadyReadCallback(true),
    m_state(Idle),
    m_addr(),                   m_port(0),
    m_notifyread(false),        m_expectingreply(false),
    m_isValidated(false),       m_isAnnounced(false)
{
    LOG(VB_SOCKET, LOG_DEBUG, LOC + "new socket");

    if (socket > -1)
    {
        setSocket(socket);
#ifdef USING_MINGW
        // Windows sockets' default buffersize is too small for streaming
        // Could this apply to other platforms, too?
        setSendBufferSize(kSocketBufferSize);
#endif
    }

    if (!s_readyread_thread)
    {
        QMutexLocker locker(&s_readyread_thread_lock);
        if (!s_readyread_thread)
            s_readyread_thread = new MythSocketThread();
    }

    if (m_cb)
        s_readyread_thread->AddToReadyRead(this);
}

MythSocket::~MythSocket()
{
    close();
    LOG(VB_SOCKET, LOG_DEBUG, LOC + "delete socket");
}

void MythSocket::setCallbacks(MythSocketCBs *cb)
{
    if (m_cb && cb)
    {
       m_cb = cb;
       return;
    }

    m_cb = cb;

    if (m_cb)
        s_readyread_thread->AddToReadyRead(this);
    else
        s_readyread_thread->RemoveFromReadyRead(this);
}

int MythSocket::DecrRef(void)
{
    int ref = ReferenceCounter::DecrRef();

    if (m_cb && ref == 1)
    {
        m_cb = NULL;
        s_readyread_thread->RemoveFromReadyRead(this);
        // ready read thread will call DecrRef() & delete obj
    }

    return ref;
}

MythSocket::State MythSocket::state(void) const
{
    return m_state;
}

void MythSocket::setState(const State state)
{
    if (state != m_state)
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC + QString("state change %1 -> %2")
                .arg(stateToString(m_state)).arg(stateToString(state)));

        m_state = state;
    }
}

QString MythSocket::stateToString(const State state) const
{
    switch(state)
    {
        case Connected:
            return "Connected";
        case Connecting:
            return "Connecting";
        case HostLookup:
            return "HostLookup";
        case Idle:
            return "Idle";
        default:
            return QString("Invalid State: %1").arg(state);
    }
}

QString MythSocket::errorToString(const Error error) const
{
    switch(error)
    {
        case NoError:
            return "NoError";
        case AlreadyBound:
            return "AlreadyBound";
        case Inaccessible:
            return "Inaccessible";
        case NoResources:
            return "NoResources";
        case InternalError:
            return "InternalError";
        case Impossible:
            return "Impossible";
        case NoFiles:
            return "NoFiles";
        case ConnectionRefused:
            return "ConnectionRefused";
        case NetworkFailure:
            return "NetworkFailure";
        case UnknownError:
            return "UnknownError";
        default:
            return QString("Invalid error: %1").arg(error);
    }
}

void MythSocket::setSocket(int socket, Type type)
{
    LOG(VB_SOCKET, LOG_DEBUG, LOC + QString("setSocket: %1").arg(socket));
    if (socket < 0)
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC + "setSocket called with invalid socket");
        return;
    }

    if (state() == Connected)
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC +
            "setSocket called while in Connected state, closing");
        close();
    }

    MSocketDevice::setSocket(socket, type);
    setBlocking(false);
    setState(Connected);
    setKeepalive(true);
}

void MythSocket::close(void)
{
    setState(Idle);
    MSocketDevice::close();
    if (m_cb)
    {
        LOG(VB_SOCKET, LOG_DEBUG, LOC + "calling m_cb->connectionClosed()");
        m_cb->connectionClosed(this);
    }
}

bool MythSocket::closedByRemote(void)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(socket(), &rfds);

    struct timeval to;
    to.tv_sec = 0;
    to.tv_usec = 1000;

    int rval = select(socket() + 1, &rfds, NULL, NULL, &to);

    if (rval > 0 && FD_ISSET(socket(), &rfds) && !bytesAvailable())
        return true;

    return false;
}

qint64 MythSocket::readBlock(char *data, quint64 len)
{
    LOG(VB_SOCKET, LOG_DEBUG, LOC + QString("readBlock(0x%1, %2) called")
            .arg((uint64_t)data,0,16).arg(len));

    if (state() != Connected)
    {
        LOG(VB_SOCKET, LOG_ERR, LOC + "readBlock called while not in "
                                      "connected state");
        return -1;
    }

    m_notifyread = false;

    qint64 rval = MSocketDevice::readBlock(data, len);
    if (rval == 0)
        close();

    return rval;
}

/**
 *  \brief Attempt to write 'len' bytes to socket
 *  \return actual bytes written
 */
qint64 MythSocket::writeBlock(const char *data, quint64 len)
{
    LOG(VB_SOCKET, LOG_DEBUG, LOC + QString("writeBlock(0x%1, %2)")
            .arg((uint64_t)data, 0, 16).arg(len));

    if (state() != Connected)
    {
        LOG(VB_SOCKET, LOG_ERR, LOC + "writeBlock called while not in "
                                      "connected state");
        return -1;
    }

    qint64 rval = MSocketDevice::writeBlock(data, len);

    // see if socket went away
    if (!isValid() || error() != MSocketDevice::NoError)
    {
        close();
        return -1;
    }
    return rval;
}

static QString toSample(const QByteArray &payload)
{
    QString sample("");
    for (uint i = 0; (i<60) && (i<(uint)payload.length()); i++)
    {
        sample += QChar(payload.data()[i]).isPrint() ?
            QChar(payload.data()[i]) : QChar('?');
    }
    sample += (payload.length() > 60) ? "..." : "";
    return sample;
}

bool MythSocket::writeStringList(QStringList &list)
{
    if (list.size() <= 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "writeStringList: Error, invalid string list.");
        return false;
    }

    if (state() != Connected)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "writeStringList: Error, called with unconnected socket.");
        return false;
    }

    QString str = list.join("[]:[]");
    if (str.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "writeStringList: Error, joined null string.");
        return false;
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
            .arg(socket(), 2).arg(payload.data());

        if (logLevel < LOG_DEBUG && msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        LOG(VB_NETWORK, LOG_INFO, LOC + msg);
    }

    MythTimer timer; timer.start();
    unsigned int errorcount = 0;
    while (size > 0)
    {
        if (state() != Connected)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "writeStringList: Error, socket went unconnected." +
                QString("\n\t\t\tWe wrote %1 of %2 bytes with %3 errors")
                    .arg(written).arg(written+size).arg(errorcount) +
                    QString("\n\t\t\tstarts with: %1").arg(toSample(payload)));
            return false;
        }

        int temp = writeBlock(payload.data() + written, size);
        if (temp > 0)
        {
            written += temp;
            written_since_timer_restart += temp;
            size -= temp;
            if ((timer.elapsed() > 500) && written_since_timer_restart != 0)
            {
                timer.restart();
                written_since_timer_restart = 0;
            }
        }
        else if (temp < 0 && error() != MSocketDevice::NoError)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("writeStringList: Error, writeBlock failed. (%1)")
                    .arg(errorToString()));
            return false;
        }
        else if (temp <= 0)
        {
            errorcount++;
            if (timer.elapsed() > 1000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "writeStringList: Error, " +
                    QString("No data written on writeBlock (%1 errors)")
                        .arg(errorcount) +
                    QString("\n\t\t\tstarts with: %1") .arg(toSample(payload)));
                return false;
            }
            usleep(1000);
        }
    }

    flush();

    return true;
}

/**
 *  \brief Read len bytes to data from socket
 *  \return true if desired len of data is read
 */
bool MythSocket::readData(char *data, quint64 len)
{
    if (state() != Connected)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "readData: Error, called with unconnected socket.");
        return false;
    }

    quint64 bytes_read = 0;
    uint zerocnt = 0;

    while (bytes_read < len)
    {
        qint64 btw = len - bytes_read >= kSocketBufferSize ?
                                       kSocketBufferSize : len - bytes_read;
        qint64 sret = readBlock(data + bytes_read, btw);
        if (sret > 0)
        {
            zerocnt = 0;
            bytes_read += sret;
        }
        else if (!isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "readData: Error, socket went unconnected");
            close();
            return false;
        }
        else if (sret < 0 && error() != MSocketDevice::NoError)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("readData: Error, readBlock: %1")
                    .arg(errorToString()));
            close();
            return false;
        }
        else
        {
            zerocnt++;
            if (zerocnt > 5000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "readData: Error, zerocnt timeout");
                return false;
            }
            usleep(1000);
         }
    }
    return true;
}

/**
 *  \brief Write len bytes to data to socket
 *  \return true if entire len of data is written
 */
bool MythSocket::writeData(const char *data, quint64 len)
{
    if (state() != Connected)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "writeData: Error, called with unconnected socket.");
        return false;
    }

    quint64 written = 0;
    uint zerocnt = 0;

    while (written < len)
    {
        qint64 btw = len - written >= kSocketBufferSize ?
                                       kSocketBufferSize : len - written;
        qint64 sret = writeBlock(data + written, btw);
        if (sret > 0)
        {
            zerocnt = 0;
            written += sret;
        }
        else if (!isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "writeData: Error, socket went unconnected");
            close();
            return false;
        }
        else if (sret < 0 && error() != MSocketDevice::NoError)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("writeData: Error, writeBlock: %1")
                    .arg(errorToString()));
            close();
            return false;
        }
        else
        {
            zerocnt++;
            if (zerocnt > 5000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "writeData: Error, zerocnt timeout");
                return false;
            }
            usleep(1000);
         }
    }
    return true;
}

bool MythSocket::readStringList(QStringList &list, uint timeoutMS)
{
    list.clear();

    if (state() != Connected)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "readStringList: Error, called with unconnected socket.");
        return false;
    }

    MythTimer timer;
    timer.start();
    int elapsed = 0;

    while (waitForMore(5) < 8)
    {
        elapsed = timer.elapsed();
        if (elapsed >= (int)timeoutMS)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "readStringList: " +
                QString("Error, timed out after %1 ms.").arg(timeoutMS));
            close();
            return false;
        }

        if (state() != Connected)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "readStringList: Connection died.");
            return false;
        }

        {
            struct timeval tv;
            int maxfd;
            fd_set rfds;

            FD_ZERO(&rfds);
            FD_SET(socket(), &rfds);
            maxfd = socket();

            tv.tv_sec = 0;
            tv.tv_usec = 0;

            int rval = select(maxfd + 1, &rfds, NULL, NULL, &tv);
            if (rval)
            {
                if (bytesAvailable() == 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "readStringList: Connection died (select).");
                    return false;
                }
            }
        }
    }

    QByteArray sizestr(8 + 1, '\0');
    if (readBlock(sizestr.data(), 8) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("readStringList: Error, readBlock return error (%1)")
                .arg(errorToString()));
        close();
        return false;
    }

    QString sizes = sizestr;
    qint64 btr = sizes.trimmed().toInt();

    if (btr < 1)
    {
        int pending = bytesAvailable();
        QByteArray dump(pending + 1, 0);
        readBlock(dump.data(), pending);
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Protocol error: '%1' is not a valid size "
                    "prefix. %2 bytes pending.")
                .arg(sizestr.data()).arg(pending));
        return false;
    }

    QByteArray utf8(btr + 1, 0);

    qint64 read = 0;
    int errmsgtime = 0;
    timer.start();

    while (btr > 0)
    {
        qint64 sret = readBlock(utf8.data() + read, btr);
        if (sret > 0)
        {
            read += sret;
            btr -= sret;
            if (btr > 0)
            {
                timer.start();
            }
        }
        else if (sret < 0 && error() != MSocketDevice::NoError)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("readStringList: Error, readBlock %1")
                    .arg(errorToString()));
            close();
            return false;
        }
        else if (!isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "readStringList: Error, socket went unconnected");
            close();
            return false;
        }
        else
        {
            elapsed = timer.elapsed();
            if (elapsed  > 10000)
            {
                if ((elapsed - errmsgtime) > 10000)
                {
                    errmsgtime = elapsed;
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("readStringList: Waiting for data: %1 %2")
                            .arg(read).arg(btr));
                }
            }

            if (elapsed > 100000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Error, readStringList timeout (readBlock)");
                return false;
            }

            usleep(500);
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    QByteArray payload;
    payload = payload.setNum(str.length());
    payload += "        ";
    payload.truncate(8);
    payload += str;

    if (VERBOSE_LEVEL_CHECK(VB_NETWORK, LOG_INFO))
    {
        QString msg = QString("read  <- %1 %2").arg(socket(), 2)
            .arg(payload.data());

        if (logLevel < LOG_DEBUG && msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        LOG(VB_NETWORK, LOG_INFO, LOC + msg);
    }

    list = str.split("[]:[]");

    m_notifyread = false;
    s_readyread_thread->WakeReadyReadThread();
    return true;
}

bool MythSocket::SendReceiveStringList(
            QStringList &strlist, uint min_reply_length)
{
    bool ok = false;

    Lock();
    m_expectingreply = true;

    writeStringList(strlist);
    ok = readStringList(strlist);

    while (ok && strlist[0] == "BACKEND_MESSAGE")
    {
        // not for us
        // TODO: sockets should be one directional
        // a socket that would use this call should never 
        // receive events
        if (strlist.size() >= 2)
        {
            QString message = strlist[1];
            strlist.pop_front();
            strlist.pop_front();
            MythEvent me(message, strlist);
            gCoreContext->dispatch(me);
        }

        ok = readStringList(strlist);
    }

    m_expectingreply = false;
    Unlock();

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No response.");
        return false;
    }

    if (min_reply_length && ((uint)strlist.size() < min_reply_length))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Response too short.");
        return false;
    }

    return true;
}

void MythSocket::Lock(void) const
{
    m_lock.lock();
    s_readyread_thread->WakeReadyReadThread();
}

bool MythSocket::TryLock(bool wake_readyread) const
{
    if (m_lock.tryLock())
    {
        if (wake_readyread)
            s_readyread_thread->WakeReadyReadThread();
        return true;
    }
    return false;
}

void MythSocket::Unlock(bool wake_readyread) const
{
    m_lock.unlock();
    if (wake_readyread)
        s_readyread_thread->WakeReadyReadThread();
}

/**
 *  \brief connect to host
 *  \return true on success
 */
bool MythSocket::connect(const QString &host, quint16 port)
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
                hadr = info.addresses().first();
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to lookup: %1")
                        .arg(host));
                return false;
            }
        }
    }

    return MythSocket::connect(hadr, port);
}

/**
 *  \brief connect to host
 *  \return true on success
 */
bool MythSocket::connect(const QHostAddress &hadr, quint16 port)
{
    QHostAddress addr = hadr;

    if (state() == Connected)
    {
        LOG(VB_SOCKET, LOG_ERR, LOC +
            "connect() called with already open socket, closing");
        close();
    }

    bool usingLoopback = false;
    if (s_loopback_cache.contains(addr.toString()))
    {
        addr = QHostAddress(s_loopback_cache.value(addr.toString()));
        usingLoopback = true;
    }
    else
    {
        QList<QHostAddress> localIPs = QNetworkInterface::allAddresses();
        for (int i = 0; i < localIPs.count() && !usingLoopback; ++i)
        {
            if (addr == localIPs[i])
            {
                QHostAddress::SpecialAddress loopback = QHostAddress::LocalHost;
                if (addr.protocol() == QAbstractSocket::IPv6Protocol)
                    loopback = QHostAddress::LocalHostIPv6;
                
                s_loopback_cache[addr.toString()] = loopback;
                addr = QHostAddress(loopback);
                usingLoopback = true;
            }
        }
    }

    if (usingLoopback)
        LOG(VB_SOCKET, LOG_INFO, LOC + QString("IP is local, using "
                                               "loopback address instead"));

    LOG(VB_SOCKET, LOG_INFO, LOC + QString("attempting connect() to (%1:%2)")
            .arg(addr.toString()).arg(port));

    if (!MSocketDevice::connect(addr, port))
    {
        LOG(VB_SOCKET, LOG_ERR, LOC + QString("connect() failed (%1)")
                .arg(errorToString()));
        setState(Idle);
        return false;
    }

    setReceiveBufferSize(kSocketBufferSize);
    setAddressReusable(true);
    setKeepalive(true);
    if (state() == Connecting)
    {
        setState(Connected);
        if (m_cb)
        {
            LOG(VB_SOCKET, LOG_DEBUG, LOC + "calling m_cb->connected()");
            m_cb->connected(this);
            s_readyread_thread->WakeReadyReadThread();
        }
    }
    else
    {
        setState(Connected);
    }

    return true;
}

bool MythSocket::Validate(uint timeout_ms, bool error_dialog_desired)
{
    if (m_isValidated)
        return true;

    QStringList strlist(QString("MYTH_PROTO_VERSION %1 %2")
                            .arg(MYTH_PROTO_VERSION).arg(MYTH_PROTO_TOKEN));
    writeStringList(strlist);

    if (!readStringList(strlist, timeout_ms) || strlist.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Protocol version check failure.\n\t\t\t"
                "The response to MYTH_PROTO_VERSION was empty.\n\t\t\t"
                "This happens when the backend is too busy to respond,\n\t\t\t"
                "or has deadlocked due to bugs or hardware failure.");
        return false;
    }
    else if (strlist[0] == "REJECT" && strlist.size() >= 2)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Protocol version or token mismatch "
                                    "(frontend=%1/%2,"
                                    "backend=%3/\?\?)\n")
                                    .arg(MYTH_PROTO_VERSION)
                                    .arg(MYTH_PROTO_TOKEN)
                                    .arg(strlist[1]));

        QObject *GUIcontext = gCoreContext->GetGUIObject();
        if (error_dialog_desired && GUIcontext)
        {
            QStringList list(strlist[1]);
            QCoreApplication::postEvent(
                GUIcontext, new MythEvent("VERSION_MISMATCH", list));
        }

        return false;
    }
    else if (strlist[0] == "ACCEPT")
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Using protocol version %1")
                               .arg(MYTH_PROTO_VERSION));
        setValidated();
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR,
        QString("Unexpected response to MYTH_PROTO_VERSION: %1")
                               .arg(strlist[0]));
    return false;
}

bool MythSocket::Announce(QStringList &strlist)
{
    if (!m_isValidated)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "refusing to announce unvalidated socket");
    }

    if (m_isAnnounced)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "refusing to re-announce socket");
        return false;
    }

    m_announce << strlist;

    writeStringList(strlist);
    strlist.clear();

    if (!readStringList(strlist, true))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("\n\t\t\tCould not read string list from server %1:%2")
                .arg(m_addr.toString()).arg(m_port));
        m_announce.clear();
        return false;
    }

    m_isAnnounced = true;
    return true;
}

void MythSocket::setAnnounce(QStringList &strlist)
{
    m_announce.clear();
    m_announce << strlist;
    m_isAnnounced = true;
}
