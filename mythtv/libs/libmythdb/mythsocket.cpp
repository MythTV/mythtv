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
#include <QMap>

// MythTV
#include "mythsocketthread.h"
#include "mythsocket.h"
#include "mythtimer.h"
#include "mythsocket.h"
#include "mythverbose.h"

#define SLOC(a) QString("MythSocket(%1:%2): ")\
                    .arg((quint64)a, 0, 16).arg(a->socket())
#define LOC SLOC(this)

const uint MythSocket::kSocketBufferSize = 128000;
const uint MythSocket::kShortTimeout = kMythSocketShortTimeout;
const uint MythSocket::kLongTimeout  = kMythSocketLongTimeout;

MythSocketThread *MythSocket::s_readyread_thread = new MythSocketThread();

MythSocket::MythSocket(int socket, MythSocketCBs *cb)
    : MSocketDevice(MSocketDevice::Stream),            m_cb(cb),
      m_useReadyReadCallback(true),
      m_state(Idle),         m_addr(),                 m_port(0),
      m_ref_count(0),        m_notifyread(false)
{
    VERBOSE(VB_SOCKET, LOC + "new socket");
    if (socket > -1)
    {
        setSocket(socket);
#ifdef USING_MINGW
        // Windows sockets' default buffersize is too small for streaming
        // Could this apply to other platforms, too?
        setSendBufferSize(kSocketBufferSize);
#endif
    }

    if (m_cb)
        s_readyread_thread->AddToReadyRead(this);
}

MythSocket::~MythSocket()
{
    close();
    VERBOSE(VB_SOCKET, LOC + "delete socket");
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

void MythSocket::UpRef(void)
{
    QMutexLocker locker(&m_ref_lock);
    m_ref_count++;
    VERBOSE(VB_SOCKET, LOC + QString("UpRef: %1").arg(m_ref_count));
}

bool MythSocket::DownRef(void)
{
    m_ref_lock.lock();
    int ref = --m_ref_count;
    m_ref_lock.unlock();

    VERBOSE(VB_SOCKET, LOC + QString("DownRef: %1").arg(m_ref_count));

    if (m_cb && ref == 0)
    {
        m_cb = NULL;
        s_readyread_thread->RemoveFromReadyRead(this);
        // thread will downref & delete obj
        return true;
    }
    else if (ref < 0)
    {
        delete this;
        return true;
    }

    return false;
}

MythSocket::State MythSocket::state(void)
{
    return m_state;
}

void MythSocket::setState(const State state)
{
    if (state != m_state)
    {
        VERBOSE(VB_SOCKET, LOC + QString("state change %1 -> %2")
                .arg(stateToString(m_state)).arg(stateToString(state)));

        m_state = state;
    }
}

QString MythSocket::stateToString(const State state)
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

QString MythSocket::errorToString(const Error error)
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
    VERBOSE(VB_SOCKET, LOC + QString("setSocket: %1").arg(socket));
    if (socket < 0)
    {
        VERBOSE(VB_SOCKET, LOC + "setSocket called with invalid socket");
        return;
    }

    if (state() == Connected)
    {
        VERBOSE(VB_SOCKET, LOC +
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
        VERBOSE(VB_SOCKET, LOC + "calling m_cb->connectionClosed()");
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
    VERBOSE(VB_SOCKET|VB_EXTRA, LOC + QString("readBlock(0x%1, %2) called")
            .arg((quint64)data).arg(len));

    if (state() != Connected)
    {
        VERBOSE(VB_SOCKET, LOC + "readBlock called while not in "
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
    VERBOSE(VB_SOCKET|VB_EXTRA, LOC + QString("writeBlock(0x%1, %2)")
            .arg((quint64)data).arg(len));

    if (state() != Connected)
    {
        VERBOSE(VB_SOCKET, LOC + "writeBlock called while not in "
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

bool MythSocket::writeStringList(QStringList &list)
{
    if (list.size() <= 0)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "writeStringList: Error, invalid string list.");
        return false;
    }

    if (state() != Connected)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "writeStringList: Error, called with unconnected socket.");
        return false;
    }

    QString str = list.join("[]:[]");
    if (str.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC +
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

    if (VERBOSE_LEVEL_CHECK(VB_NETWORK))
    {
        QString msg = QString("write -> %1 %2")
            .arg(socket(), 2).arg(payload.data());

        if (!VERBOSE_LEVEL_CHECK(VB_EXTRA) && msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, LOC + msg);
    }

    MythTimer timer; timer.start();
    unsigned int errorcount = 0;
    while (size > 0)
    {
        if (state() != Connected)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "writeStringList: Error, socket went unconnected." +
                    QString("\n\t\t\tWe wrote %1 of %2 bytes with %3 errors")
                    .arg(written).arg(written+size).arg(errorcount));
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
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("writeStringList: Error, writeBlock failed. (%1)")
                    .arg(errorToString()));
            return false;
        }
        else if (temp <= 0)
        {
            errorcount++;
            if (timer.elapsed() > 1000)
            {
                VERBOSE(VB_GENERAL, LOC + "writeStringList: Error, " +
                        QString("No data written on writeBlock (%1 errors)")
                        .arg(errorcount));
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
        VERBOSE(VB_IMPORTANT, LOC +
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
            VERBOSE(VB_IMPORTANT, LOC +
                    "readData: Error, socket went unconnected");
            close();
            return false;
        }
        else if (sret < 0 && error() != MSocketDevice::NoError)
        {
            VERBOSE(VB_IMPORTANT, LOC +
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
                VERBOSE(VB_IMPORTANT, LOC +
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
        VERBOSE(VB_IMPORTANT, LOC +
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
            VERBOSE(VB_IMPORTANT, LOC +
                    "writeData: Error, socket went unconnected");
            close();
            return false;
        }
        else if (sret < 0 && error() != MSocketDevice::NoError)
        {
            VERBOSE(VB_IMPORTANT, LOC +
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
                VERBOSE(VB_IMPORTANT, LOC +
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
        VERBOSE(VB_IMPORTANT, LOC +
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
            VERBOSE(VB_IMPORTANT, LOC + "readStringList: " +
                    QString("Error, timed out after %1 ms.").arg(timeoutMS));
            close();
            return false;
        }

        if (state() != Connected)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "readStringList: Connection died.");
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
                    VERBOSE(VB_IMPORTANT, LOC +
                            "readStringList: Connection died (select).");
                    return false;
                }
            }
        }
    }

    QByteArray sizestr(8 + 1, '\0');
    if (readBlock(sizestr.data(), 8) < 0)
    {
        VERBOSE(VB_GENERAL, LOC +
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
        VERBOSE(VB_IMPORTANT, LOC +
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
            VERBOSE(VB_GENERAL, LOC +
                    QString("readStringList: Error, readBlock %1")
                    .arg(errorToString()));
            close();
            return false;
        }
        else if (!isValid())
        {
            VERBOSE(VB_IMPORTANT, LOC +
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
                    VERBOSE(VB_GENERAL, LOC +
                            QString("readStringList: Waiting for data: %1 %2")
                            .arg(read).arg(btr));
                }
            }

            if (elapsed > 100000)
            {
                VERBOSE(VB_GENERAL, LOC +
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

    if (VERBOSE_LEVEL_CHECK(VB_NETWORK))
    {
        QString msg = QString("read  <- %1 %2").arg(socket(), 2)
            .arg(payload.data());

        if (!VERBOSE_LEVEL_CHECK(VB_EXTRA) && msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, LOC + msg);
    }

    list = str.split("[]:[]");

    m_notifyread = false;
    s_readyread_thread->WakeReadyReadThread();
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
    if (!hadr.setAddress(host))
    {
        QHostInfo info = QHostInfo::fromName(host);
        if (!info.addresses().isEmpty())
        {
            hadr = info.addresses().first();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + QString("Unable to lookup: %1")
                    .arg(host));
            return false;
        }
    }

    return MythSocket::connect(hadr, port);
}

/**
 *  \brief connect to host
 *  \return true on success
 */
bool MythSocket::connect(const QHostAddress &addr, quint16 port)
{
    if (state() == Connected)
    {
        VERBOSE(VB_SOCKET, LOC +
                "connect() called with already open socket, closing");
        close();
    }

    VERBOSE(VB_SOCKET, LOC + QString("attempting connect() to (%1:%2)")
            .arg(addr.toString()).arg(port));

    if (!MSocketDevice::connect(addr, port))
    {
        VERBOSE(VB_SOCKET, LOC + QString("connect() failed (%1)")
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
            VERBOSE(VB_SOCKET, LOC + "calling m_cb->connected()");
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
