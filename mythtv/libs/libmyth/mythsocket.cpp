#include <qapplication.h>
#include <qstring.h>
#include <unistd.h>
#include "mythcontext.h"
#include "util.h"
#include "mythsocket.h"
#include <sys/select.h>
#include <cassert>

#define SLOC(a) QString("MythSocket(%1:%2): ").arg((unsigned long)a, 0, 16)\
                    .arg(a->socket())
#define LOC SLOC(this)

const uint MythSocket::kSocketBufferSize = 128000;

pthread_t MythSocket::m_readyread_thread = (pthread_t)0;
bool MythSocket::m_readyread_run = false;
QMutex MythSocket::m_readyread_lock;
QPtrList<MythSocket> MythSocket::m_readyread_list;
QPtrList<MythSocket> MythSocket::m_readyread_dellist;
QPtrList<MythSocket> MythSocket::m_readyread_addlist;
int MythSocket::m_readyread_pipe[2] = {-1, -1};

MythSocket::MythSocket(int socket, MythSocketCBs *cb)
    : QSocketDevice(QSocketDevice::Stream),            m_cb(cb),
      m_state(Idle),         m_addr(),                 m_port(0),
      m_ref_count(0),        m_notifyread(0) 
{
    VERBOSE(VB_SOCKET, LOC + "new socket");
    if (socket > -1)
        setSocket(socket);

    if (m_cb)
        AddToReadyRead(this);
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
        AddToReadyRead(this);
    else
        RemoveFromReadyRead(this);
}

void MythSocket::UpRef(void)
{
    m_ref_lock.lock();
    m_ref_count++;
    m_ref_lock.unlock();
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
        RemoveFromReadyRead(this);
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

    QSocketDevice::setSocket(socket, type);
    setBlocking(false);
    setState(Connected);
}

void MythSocket::close(void)
{
    setState(Idle);
    QSocketDevice::close();
}

Q_LONG MythSocket::readBlock(char *data, Q_ULONG len)
{
    // VERBOSE(VB_SOCKET, LOC + "readBlock called");
    if (state() != Connected)
    {
        VERBOSE(VB_SOCKET, LOC + "readBlock called while not in " 
                "connected state");
        return -1;
    }

    m_notifyread = false;

    Q_LONG rval = QSocketDevice::readBlock(data, len);
    if (rval == 0)
    {
        close();
        if (m_cb)
        {
            m_cb->connectionClosed(this);
            VERBOSE(VB_SOCKET, LOC + "calling cb->connectionClosed()");
        }
    }
    return rval;
}

/** \fn writeBlock(char*, Q_ULONG)
 *  \brief Attempt to write 'len' bytes to socket
 *  \return actual bytes written
 */
Q_LONG MythSocket::writeBlock(const char *data, Q_ULONG len)
{
    //VERBOSE(VB_SOCKET, LOC + "writeBlock called");
    if (state() != Connected)
    {
        VERBOSE(VB_SOCKET, LOC + "writeBlock called while not in " 
                "connected state");
        return -1;
    }

    Q_LONG rval = QSocketDevice::writeBlock(data, len);

    // see if socket went away
    if (!isValid() || error() != QSocketDevice::NoError)
    {
        close();
        if (m_cb)
        {
            VERBOSE(VB_SOCKET, LOC + "cb->connectionClosed()");
            m_cb->connectionClosed(this);
        }
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
    if (str == QString::null)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "writeStringList: Error, joined null string.");
        return false;
    }

    QCString utf8 = str.utf8();
    int size = utf8.length();
    int written = 0;

    QCString payload;
    payload = payload.setNum(size);
    payload += "        ";
    payload.truncate(8);
    payload += utf8;
    size = payload.length();

    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = QString("write -> %1 %2")
            .arg(socket(), 2).arg(payload);

        if ((print_verbose_messages != VB_ALL) && msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    unsigned int errorcount = 0;
    while (size > 0)
    {
        if (state() != Connected)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "writeStringList: Error, socket went unconnected.");
            return false;
        }

        int temp = writeBlock(payload.data() + written, size);
        if (temp > 0)
        {
            written += temp;
            size -= temp;
        }
        else if (temp < 0 && error() != QSocketDevice::NoError)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("writeStringList: Error, writeBlock failed. (%1)")
                    .arg(errorToString()));
            return false;
        } 
        else if (temp <= 0)
        {
            errorcount++;
            if (errorcount > 50)
            {          
                VERBOSE(VB_GENERAL, LOC +
                        "writeStringList: No data written on writeBlock");
                return false;
            }
            usleep(500);
        }
    }
    return true;
}

/** \fn writeData(char*,Q_ULONG)
 *  \brief Write len bytes to data to socket
 *  \return true if entire len of data is written
 */
bool MythSocket::writeData(const char *data, Q_ULONG len)
{
    if (state() != Connected)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "writeData: Error, called with unconnected socket.");
        return false;
    }

    Q_ULONG written = 0;
    uint zerocnt = 0;

    while (written < len)
    {
        Q_LONG btw = len - written >= kSocketBufferSize ?
                                       kSocketBufferSize : len - written;
        Q_LONG sret = writeBlock(data + written, btw);
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
        else if (sret < 0 && error() != QSocketDevice::NoError)
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
            if (zerocnt > 200)
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

bool MythSocket::readStringList(QStringList &list, bool quickTimeout)
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
        if (!quickTimeout && elapsed >= 30000)
        {
            VERBOSE(VB_GENERAL, LOC + "readStringList: Error, timeout.");
            close();
            if (m_cb)
            {
                m_cb->connectionClosed(this);
                VERBOSE(VB_SOCKET, LOC + "calling cb->connectionClosed()");
            }
            return false;
        }
        else if (quickTimeout && elapsed >= 7000)
        {
            VERBOSE(VB_GENERAL, LOC + 
                    "readStringList: Error, timeout (quick).");
            close();
            if (m_cb)
            {
                m_cb->connectionClosed(this);
                VERBOSE(VB_SOCKET, LOC + "calling cb->connectionClosed()");
            }
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

    QCString sizestr(8 + 1);
    if (readBlock(sizestr.data(), 8) < 0)
    {
        VERBOSE(VB_GENERAL, LOC +
                QString("readStringList: Error, readBlock return error (%1)")
                .arg(errorToString()));
        close();
        return false;
    }

    sizestr = sizestr.stripWhiteSpace();
    Q_LONG btr = sizestr.toInt();

    if (btr < 1)
    {
        int pending = bytesAvailable();
        QCString dump(pending + 1);
        readBlock(dump.data(), pending);
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Protocol error: '%1' is not a valid size " 
                        "prefix. %2 bytes pending.")
                        .arg(sizestr).arg(pending));
        return false;
    }

    QCString utf8(btr + 1);

    Q_LONG read = 0;
    int errmsgtime = 0;
    timer.start();
    
    while (btr > 0)
    {
        Q_LONG sret = readBlock(utf8.data() + read, btr);
        if (sret > 0)
        {
            read += sret;
            btr -= sret;
            if (btr > 0)
            {
                timer.start();
            }    
        } 
        else if (sret < 0 && error() != QSocketDevice::NoError)
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

    QCString payload;
    payload = payload.setNum(str.length());
    payload += "        ";
    payload.truncate(8);
    payload += str;
    
    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = QString("read  <- %1 %2").arg(socket(), 2)
                                               .arg(payload);

        if ((print_verbose_messages != VB_ALL) && msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    list = QStringList::split("[]:[]", str, true);

    m_notifyread = false;
    WakeReadyReadThread();
    return true;
}

void MythSocket::Lock(void)
{
    m_lock.lock();
    WakeReadyReadThread();
}

void MythSocket::Unlock(void)
{
    m_lock.unlock();
    WakeReadyReadThread();
}

/** \fn connect(QString &, Q_UINT16)
 *  \brief connect to host
 *  \return true on success
 */
bool MythSocket::connect(const QString &host, Q_UINT16 port)
{
    QHostAddress hadr;
    hadr.setAddress(host);
    return MythSocket::connect(hadr, port);
}

/** \fn connect(QHostAddress &, Q_UINT16)
 *  \brief connect to host
 *  \return true on success
 */
bool MythSocket::connect(const QHostAddress &addr, Q_UINT16 port)
{
    if (state() == Connected)
    {
        VERBOSE(VB_SOCKET, LOC +
                "connect() called with already open socket, closing");
        close();
    }

    VERBOSE(VB_SOCKET, LOC + QString("attempting connect() to (%1:%2)")
            .arg(addr.toString()).arg(port));

    if (!QSocketDevice::connect(addr, port))
    {
        VERBOSE(VB_SOCKET, LOC + QString("connect() failed (%1)")
                .arg(errorToString()));
        setState(Idle);
        return false;
    }
        
    setReceiveBufferSize(kSocketBufferSize);
    setAddressReusable(true);
    if (state() == Connecting)
    {
        setState(Connected);
        if (m_cb)
        {
            VERBOSE(VB_SOCKET, LOC + "cb->connected()");
            m_cb->connected(this);
            WakeReadyReadThread();
        }
    }
    else
    {
        setState(Connected);
    }

    return true;
}

void MythSocket::ShutdownReadyReadThread(void)
{
    m_readyread_run = false;
    WakeReadyReadThread();
    pthread_join(m_readyread_thread, NULL);

    ::close(m_readyread_pipe[0]);
    ::close(m_readyread_pipe[1]);
}

void MythSocket::StartReadyReadThread(void)
{
    if (m_readyread_run == false)
    {
        QMutexLocker locker(&m_readyread_lock);
        if (m_readyread_run == false)
        {
            int ret = pipe(m_readyread_pipe);
            assert(ret >= 0);

            m_readyread_run = true;
            pthread_create(&m_readyread_thread, NULL, readyReadThread, NULL);

            atexit(ShutdownReadyReadThread);
        }
    }   
}

void MythSocket::AddToReadyRead(MythSocket *sock)
{
    StartReadyReadThread();

    sock->UpRef();
    m_readyread_lock.lock();
    m_readyread_addlist.append(sock);
    m_readyread_lock.unlock();

    WakeReadyReadThread();
}

void MythSocket::RemoveFromReadyRead(MythSocket *sock)
{
    m_readyread_lock.lock();
    m_readyread_dellist.append(sock);
    m_readyread_lock.unlock();

    WakeReadyReadThread();
}

void MythSocket::WakeReadyReadThread(void)
{
    if (m_readyread_pipe[1] >= 0)
    {
        char buf[1] = { '0' };
        write(m_readyread_pipe[1], &buf, 1);
    }
}

void *MythSocket::readyReadThread(void *)
{
    VERBOSE(VB_SOCKET, "MythSocket: readyread thread start");
    fd_set rfds;
    struct timeval timeout;
    MythSocket *sock;
    int maxfd;
    bool found;

    while (m_readyread_run)
    {
        m_readyread_lock.lock();
        while (m_readyread_dellist.count() > 0)
        {
            sock = m_readyread_dellist.take();
            bool del = m_readyread_list.removeRef(sock);

            if (del)
            {
                m_readyread_lock.unlock();
                sock->DownRef();
                m_readyread_lock.lock();
            }
        }

        while (m_readyread_addlist.count() > 0)
        {
            sock = m_readyread_addlist.take();
            //sock->UpRef();  Did upref in AddToReadyRead()
            m_readyread_list.append(sock);
        }
        m_readyread_lock.unlock();

        // add check for bad fd?
        FD_ZERO(&rfds);
        maxfd = m_readyread_pipe[0];

        FD_SET(m_readyread_pipe[0], &rfds);

        QPtrListIterator<MythSocket> it(m_readyread_list);
        while ((sock = it.current()) != 0)
        {
            if (sock->state() == Connected &&
                sock->m_notifyread == false &&
                !sock->m_lock.locked())
            {
                FD_SET(sock->socket(), &rfds);
                maxfd = max(sock->socket(), maxfd);
            }
            ++it;
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;

        int rval = select(maxfd + 1, &rfds, NULL, NULL, &timeout);
        if (rval == -1)
        {
            VERBOSE(VB_SOCKET, "MythSocket: select returned error");
        }
        else if (rval)
        {
            found = false;
            QPtrListIterator<MythSocket> it(m_readyread_list);
            while ((sock = it.current()) != 0)
            {
                if (sock->state() == Connected &&
                    FD_ISSET(sock->socket(), &rfds) &&
                    !sock->m_lock.locked())
                {
                    found = true;
                    break;
                }
                ++it;
            }

            if (found)
            {
                VERBOSE(VB_SOCKET, SLOC(sock) + "socket is readable");
                if (sock->bytesAvailable() == 0)
                {
                    VERBOSE(VB_SOCKET, SLOC(sock) + "socket closed");
                    sock->close();

                    if (sock->m_cb)
                    {
                        VERBOSE(VB_SOCKET, SLOC(sock) + "cb->connectionClosed()");
                        sock->m_cb->connectionClosed(sock);
                    }
                }
                else if (sock->m_cb)
                {
                    sock->m_notifyread = true;
                    VERBOSE(VB_SOCKET, SLOC(sock) + "cb->readyRead()");
                    sock->m_cb->readyRead(sock);
                }
            }
            if (FD_ISSET(m_readyread_pipe[0], &rfds))
            {
                char buf[128];
                read(m_readyread_pipe[0], buf, 128);
            }
        }
        else
        {
//          VERBOSE(VB_SOCKET, SLOC + "select timeout");
        }
    }

    VERBOSE(VB_SOCKET, "MythSocket: readyread thread exit");

    return NULL;
}

