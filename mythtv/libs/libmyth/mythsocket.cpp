#include <qapplication.h>
#include <qstring.h>
#include "mythcontext.h"
#include "util.h"
#include "mythsocket.h"
#include <sys/select.h>


#define LOC QString("MythSocket(%1:%2): ").arg((unsigned long)this, 0, 16)\
                    .arg(this->socket())

const uint MythSocket::kSocketBufferSize = 128000;

MythSocket::MythSocket(int socket)
    : QSocketDevice(QSocketDevice::Stream),            m_cb(NULL),
      m_state(Idle),         m_addr(),                 m_port(0),
      m_ref_count(0),        m_readyread_thread(),     m_readyread_run(false) 
{
    VERBOSE(VB_SOCKET, LOC + "new socket");
    if (socket > -1)
        setSocket(socket);
}

MythSocket::MythSocket(MythSocketCBs *cb, int socket)
    : QSocketDevice(QSocketDevice::Stream),         m_cb(cb),
      m_state(Idle),         m_addr(),              m_port(0),
      m_ref_count(0),        m_readyread_thread(),  m_readyread_run(false)
{
    VERBOSE(VB_SOCKET, LOC + "new socket");
    if (socket > -1)
        setSocket(socket);

    if (m_cb)
    {
        UpRef();
        pthread_create(&m_readyread_thread, NULL, readyReadThreadStart, this);
        pthread_detach(m_readyread_thread);
    }
}

MythSocket::~MythSocket()
{

    close();

    m_cb = NULL;
    if (m_readyread_run)
    {
      m_readyread_run = false;
      m_readyread_sleep.wakeAll();
      VERBOSE(VB_SOCKET, LOC + 
              "shouldnt happen, being deleted while thread is still running");
    }

    m_ref_count--;
    if (m_ref_count >= 0)
        VERBOSE(VB_IMPORTANT, LOC + "socket deleted while still referenced");

    VERBOSE(VB_SOCKET, LOC + "delete socket");
}

void MythSocket::setCallbacks(MythSocketCBs *cb)
{
    // TODO: deal with m_cb already being defined, or passed cb being NULL
    m_cb = cb;
    if (m_cb)
    {
        UpRef();
        pthread_create(&m_readyread_thread, NULL, readyReadThreadStart, this);
        pthread_detach(m_readyread_thread);
    }
}

void *MythSocket::readyReadThreadStart(void *param)
{
    MythSocket *sock = (MythSocket *)param;
    sock->readyReadThread();
    sock->DownRef();
    return NULL;
}

void MythSocket::readyReadThread(void)
{
    VERBOSE(VB_SOCKET, LOC + "readyread thread start");
    fd_set rfds;
    struct timeval timeout;

    m_readyread_run = true;
    while (m_readyread_run)
    {
        if (state() != Connected)
        {
            VERBOSE(VB_SOCKET, LOC + "readyread thread sleeping");
            m_readyread_sleep.wait();
            VERBOSE(VB_SOCKET, LOC + "readyread thread woke up");
        }
        else
        {
            // add check for bad fd?
            FD_ZERO(&rfds);
            FD_SET(socket(), &rfds);
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000;

            int rval = select(socket() + 1, &rfds, NULL, NULL, &timeout);
            if (rval == -1)
            {
                VERBOSE(VB_SOCKET, LOC + "select returned error");
            }
            else if(rval && FD_ISSET(socket(), &rfds))
            {
                VERBOSE(VB_SOCKET, LOC + "socket is readable");
                if (bytesAvailable() == 0)
                {
                    VERBOSE(VB_SOCKET, LOC + "socket closed");
                    close();

                    if (m_cb)
                    {
                        VERBOSE(VB_SOCKET, LOC + "cb->connectionClosed()");
                        m_cb->connectionClosed(this);
                    }
                }
                else if (m_cb)
                {
                    VERBOSE(VB_SOCKET, LOC + "cb->readyRead()");
                    m_cb->readyRead(this);
                }
                   
            }
            else
            {
//                VERBOSE(VB_SOCKET, LOC + "select timeout");
            }
        }
    }
    VERBOSE(VB_SOCKET, LOC + "readyread thread exit");
}

void MythSocket::UpRef(void)
{
    m_ref_count++;
    VERBOSE(VB_SOCKET, LOC + QString("UpRef: %1").arg(m_ref_count));
}

bool MythSocket::DownRef(void)
{
    m_ref_count--;
    VERBOSE(VB_SOCKET, LOC + QString("DownRef: %1").arg(m_ref_count));

    if (m_readyread_run && m_ref_count == 0)
    {
        m_readyread_run = false;
        m_readyread_sleep.wakeAll();
        // thread will delete obj
        return true;
    } 
    else if (m_ref_count < 0) 
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
    if (m_readyread_run)
        m_readyread_sleep.wakeAll();
}

void MythSocket::close(void)
{

    setState(Idle);
    QSocketDevice::close();
}

Q_LONG MythSocket::readBlock(char *data, Q_ULONG len)
{
    // VERBOSE(VB_SOCKET, LOC + "readBlock called");
    if(state() != Connected)
    {
        VERBOSE(VB_SOCKET, LOC + "readBlock called while not in " 
                "connected state");
        return -1;
    }

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
    if(state() != Connected)
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
        if (!quickTimeout && elapsed >= 300000)
        {
            VERBOSE(VB_GENERAL, LOC + "readStringList: Error, timeout.");
            close();
            return false;
        }
        else if (quickTimeout && elapsed >= 20000)
        {
            VERBOSE(VB_GENERAL, LOC + 
                    "readStringList: Error, timeout (quick).");
            close();
            return false;
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

    return true;
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
        }
    }
    else
    {
        setState(Connected);
    }

    if (m_readyread_run)
        m_readyread_sleep.wakeAll();

    return true;
}

