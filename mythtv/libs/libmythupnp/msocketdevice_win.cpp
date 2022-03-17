/****************************************************************************
**
** Copyright (C) 1992-2008 Trolltech ASA. All rights reserved.
**
** This file is part of the Qt3Support module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the files LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file.  Alternatively you may (at
** your option) use any later version of the GNU General Public
** License if such license has been publicly approved by Trolltech ASA
** (or its successors, if any) and the KDE Free Qt Foundation. In
** addition, as a special exception, Trolltech gives you certain
** additional rights. These rights are described in the Trolltech GPL
** Exception version 1.2, which can be found at
** http://www.trolltech.com/products/qt/gplexception/ and in the file
** GPL_EXCEPTION.txt in this package.
**
** Please review the following information to ensure GNU General
** Public Licensing requirements will be met:
** http://trolltech.com/products/qt/licenses/licensing/opensource/. If
** you are unsure which license is appropriate for your use, please
** review the following information:
** http://trolltech.com/products/qt/licenses/licensing/licensingoverview
** or contact the sales department at sales@trolltech.com.
**
** In addition, as a special exception, Trolltech, as the sole
** copyright holder for Qt Designer, grants users of the Qt/Eclipse
** Integration plug-in the right for the Qt/Eclipse Integration to
** link to functionality provided by Qt Designer and its related
** libraries.
**
** This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
** INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE. Trolltech reserves all rights not expressly
** granted herein.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "msocketdevice.h"
#include "qwindowdefs.h"
#include <QDateTime>

#include <QCoreApplication>
#include "libmythbase/mythlogging.h"

#include <cstring>

#if defined (QT_NO_IPV6)
#  include <windows.h>
#  include <winsock.h>
#else
#  if defined (Q_CC_BOR) || defined (Q_CC_GNU) || ( _MSC_VER )
#    include <winsock2.h>
#  elif defined (Q_CC_INTEL)
#    include <winsock.h>
#  else
#    include <windows.h>
#  if defined(Q_OS_WINCE)
#    include <winsock.h>
#  endif
#  endif
// Use our own defines and structs which we know are correct
#  define QT_SS_MAXSIZE 128
#  define QT_SS_ALIGNSIZE (sizeof(__int64))
#  define QT_SS_PAD1SIZE (QT_SS_ALIGNSIZE - sizeof (short))
#  define QT_SS_PAD2SIZE (QT_SS_MAXSIZE - (sizeof (short) + QT_SS_PAD1SIZE + QT_SS_ALIGNSIZE))

struct qt_sockaddr_storage
{
    short ss_family;
    char __ss_pad1[QT_SS_PAD1SIZE];
    __int64 __ss_align;
    char __ss_pad2[QT_SS_PAD2SIZE];
};

// sockaddr_in6 size changed between old and new SDK
// Only the new version is the correct one, so always
// use this structure.

struct qt_in6_addr
{
    u_char qt_s6_addr[16];
};

struct qt_sockaddr_in6
{
    short   sin6_family;            /* AF_INET6 */
    u_short sin6_port;              /* Transport level port number */
    u_long  sin6_flowinfo;          /* IPv6 flow information */

    struct  qt_in6_addr sin6_addr;  /* IPv6 address */
    u_long  sin6_scope_id;          /* set of interfaces for a scope */
};

#endif

#ifndef AF_INET6
#define AF_INET6        23              /* Internetwork Version 6 */
#endif

#ifndef NO_ERRNO_H
#  if defined(Q_OS_WINCE)
#     include "qfunctions_wince.h"
#  else
#     include <cerrno>
#  endif
#endif


#if defined(SOCKLEN_T)
#undef SOCKLEN_T
#endif

#define SOCKLEN_T int // #### Winsock 1.1

static int initialized = 0x00; // Holds the Winsock version

static void cleanupWinSock() // post-routine
{
    WSACleanup();
    initialized = 0x00;
}

static inline void qt_socket_getportaddr(struct sockaddr *sa,
        quint16 *port, QHostAddress *addr)
{
#if !defined (QT_NO_IPV6)

    if (sa->sa_family == AF_INET6)
    {
        qt_sockaddr_in6 *sa6 = (qt_sockaddr_in6 *)sa;
        Q_IPV6ADDR tmp;

        for (int i = 0; i < 16; ++i)
            tmp.c[i] = sa6->sin6_addr.qt_s6_addr[i];

        QHostAddress a(tmp);

        *addr = a;

        *port = ntohs(sa6->sin6_port);

        return;
    }

#endif

    struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;

    QHostAddress a(ntohl(sa4->sin_addr.s_addr));

    *port = ntohs(sa4->sin_port);

    *addr = a;
}

void MSocketDevice::init()
{
#if !defined(QT_NO_IPV6)

    if (!initialized)
    {
        WSAData wsadata;
        // IPv6 requires Winsock v2.0 or better.

        if (WSAStartup(MAKEWORD(2, 0), &wsadata) != 0)
        {
#  if defined(QSOCKETDEVICE_DEBUG)
            qDebug("MSocketDevice: WinSock v2.0 initialization failed, disabling IPv6 support.");
#  endif
        }
        else
        {
            qAddPostRoutine(cleanupWinSock);
            initialized = 0x20;
            return;
        }
    }

#endif

    if (!initialized)
    {
        WSAData wsadata;

        if (WSAStartup(MAKEWORD(1, 1), &wsadata) != 0)
        {
#if defined(QT_CHECK_NULL)
            qWarning("MSocketDevice: WinSock initialization failed");
#endif
#if defined(QSOCKETDEVICE_DEBUG)
            qDebug("MSocketDevice: WinSock initialization failed");
#endif
            return;
        }

        qAddPostRoutine(cleanupWinSock);

        initialized = 0x11;
    }
}

MSocketDevice::Protocol MSocketDevice::getProtocol() const
{
    if (isValid())
    {
#if !defined (QT_NO_IPV6)

        struct qt_sockaddr_storage sa;
#else

        struct sockaddr_in sa;
#endif
        memset(&sa, 0, sizeof(sa));
        SOCKLEN_T sz = sizeof(sa);

        if (!::getsockname(m_fd, (struct sockaddr *)&sa, &sz))
        {
#if !defined (QT_NO_IPV6)

            switch (sa.ss_family)
            {

                case AF_INET:
                    return IPv4;

                case AF_INET6:
                    return IPv6;

                default:
                    return Unknown;
            }

#else
            switch (sa.sin_family)
            {

                case AF_INET:
                    return IPv4;

                default:
                    return Unknown;
            }

#endif
        }
    }

    return Unknown;
}

int MSocketDevice::createNewSocket()
{
#if !defined(QT_NO_IPV6)
    SOCKET s;
    // Support IPv6 for Winsock v2.0++

    if (initialized >= 0x20 && protocol() == IPv6)
    {
        s = ::socket(AF_INET6, m_t == Datagram ? SOCK_DGRAM : SOCK_STREAM, 0);
    }
    else
    {
        s = ::socket(AF_INET, m_t == Datagram ? SOCK_DGRAM : SOCK_STREAM, 0);
    }

#else
    SOCKET s = ::socket(AF_INET, m_t == Datagram ? SOCK_DGRAM : SOCK_STREAM, 0);

#endif
    if (s == INVALID_SOCKET)
    {
        switch (WSAGetLastError())
        {

            case WSANOTINITIALISED:
                m_e = Impossible;
                break;

            case WSAENETDOWN:
                // ### what to use here?
                m_e = NetworkFailure;
                //m_e = Inaccessible;
                break;

            case WSAEMFILE:
                m_e = NoFiles; // special case for this
                break;

            case WSAEINPROGRESS:

            case WSAENOBUFS:
                m_e = NoResources;
                break;

            case WSAEAFNOSUPPORT:

            case WSAEPROTOTYPE:

            case WSAEPROTONOSUPPORT:

            case WSAESOCKTNOSUPPORT:
                m_e = InternalError;
                break;

            default:
                m_e = UnknownError;
                break;
        }
    }
    else
    {
        return s;
    }

    return -1;
}


void MSocketDevice::close()
{
    if (m_fd == -1 || !isOpen())    // already closed
        return;

    setOpenMode(NotOpen);

    ::closesocket(m_fd);

#if defined(QSOCKETDEVICE_DEBUG)
    qDebug("MSocketDevice::close: Closed socket %x", m_fd);

#endif
    m_fd = -1;

    fetchConnectionParameters();

    QIODevice::close();
}


bool MSocketDevice::blocking() const
{
    return true;
}


void MSocketDevice::setBlocking(bool enable)
{
#if defined(QSOCKETDEVICE_DEBUG)
    qDebug("MSocketDevice::setBlocking( %d )", enable);
#endif

    if (!isValid())
        return;

    unsigned long dummy = enable ? 0 : 1;

    ioctlsocket(m_fd, FIONBIO, &dummy);
}


int MSocketDevice::option(Option opt) const
{
    if (!isValid())
        return -1;

    int n = -1;

    int v = -1;

    switch (opt)
    {

        case Broadcast:
            n = SO_BROADCAST;
            break;

        case ReceiveBuffer:
            n = SO_RCVBUF;
            break;

        case ReuseAddress:
            n = SO_REUSEADDR;
            break;

        case SendBuffer:
            n = SO_SNDBUF;
            break;

        case Keepalive:
            n = SO_KEEPALIVE;
            break;
    }

    if (n != -1)
    {
        SOCKLEN_T len = sizeof(v);
        int r = ::getsockopt(m_fd, SOL_SOCKET, n, (char*) & v, &len);

        if (r != SOCKET_ERROR)
            return v;

        if (!m_e)
        {
            MSocketDevice *that = (MSocketDevice*)this; // mutable function

            switch (WSAGetLastError())
            {

                case WSANOTINITIALISED:
                    that->m_e = Impossible;
                    break;

                case WSAENETDOWN:
                    that->m_e = NetworkFailure;
                    break;

                case WSAEFAULT:

                case WSAEINVAL:

                case WSAENOPROTOOPT:
                    that->m_e = InternalError;
                    break;

                case WSAEINPROGRESS:
                    that->m_e = NoResources;
                    break;

                case WSAENOTSOCK:
                    that->m_e = Impossible;
                    break;

                default:
                    that->m_e = UnknownError;
                    break;
            }
        }

        return -1;
    }

    return v;
}


void MSocketDevice::setOption(Option opt, int v)
{
    if (!isValid())
        return;

    int n = -1; // for really, really bad compilers

    switch (opt)
    {

        case Broadcast:
            n = SO_BROADCAST;
            break;

        case ReceiveBuffer:
            n = SO_RCVBUF;
            break;

        case ReuseAddress:
            n = SO_REUSEADDR;
            break;

        case SendBuffer:
            n = SO_SNDBUF;
            break;

        case Keepalive:
            n = SO_KEEPALIVE;
            break;

        default:
            return;
    }

    int r = ::setsockopt(m_fd, SOL_SOCKET, n, (char*) & v, sizeof(v));

    if (r == SOCKET_ERROR && m_e == NoError)
    {
        switch (WSAGetLastError())
        {

            case WSANOTINITIALISED:
                m_e = Impossible;
                break;

            case WSAENETDOWN:
                m_e = NetworkFailure;
                break;

            case WSAEFAULT:

            case WSAEINVAL:

            case WSAENOPROTOOPT:
                m_e = InternalError;
                break;

            case WSAEINPROGRESS:
                m_e = NoResources;
                break;

            case WSAENETRESET:

            case WSAENOTCONN:
                m_e =  Impossible; // ### ?
                break;

            case WSAENOTSOCK:
                m_e = Impossible;
                break;

            default:
                m_e = UnknownError;
                break;
        }
    }
}


bool MSocketDevice::connect(const QHostAddress &addr, quint16 port)
{
    if (isValid() && addr.protocol() != m_pa.protocol())
    {
        close();
        m_fd = -1;
    }

    if (!isValid())
    {
#if !defined(QT_NO_IPV6)

        if (addr.protocol() == QAbstractSocket::IPv6Protocol)
        {
            setProtocol(IPv6);
            LOG(VB_SOCKET, LOG_INFO,
                "MSocketDevice::connect: setting Protocol to IPv6");
        }
        else
#endif
            if (addr.protocol() == QAbstractSocket::IPv4Protocol)
            {
                setProtocol(IPv4);
                LOG(VB_SOCKET, LOG_INFO,
                    "MSocketDevice::connect: setting Protocol to IPv4");
            }

        LOG(VB_SOCKET, LOG_INFO,

            "MSocketDevice::connect: attempting to create new socket");
        MSocketDevice::setSocket(createNewSocket(), m_t);

        // If still not valid, give up.

        if (!isValid())
            return false;
    }

    m_pa = addr;

    m_pp = port;

    struct sockaddr_in a4;

    struct sockaddr *aa;
    SOCKLEN_T aalen;

#if !defined(QT_NO_IPV6)
    qt_sockaddr_in6 a6;

    if (initialized >= 0x20 && addr.protocol() == QAbstractSocket::IPv6Protocol)
    {
        memset(&a6, 0, sizeof(a6));
        a6.sin6_family = AF_INET6;
        a6.sin6_port = htons(port);
        Q_IPV6ADDR ip6 = addr.toIPv6Address();
        memcpy(&a6.sin6_addr.qt_s6_addr, &ip6, sizeof(ip6));

        aalen = sizeof(a6);
        aa = (struct sockaddr *) & a6;
    }
    else
#endif
        if (addr.protocol() == QAbstractSocket::IPv4Protocol)
        {
            memset(&a4, 0, sizeof(a4));
            a4.sin_family = AF_INET;
            a4.sin_port = htons(port);
            a4.sin_addr.s_addr = htonl(addr.toIPv4Address());

            aalen = sizeof(a4);
            aa = (struct sockaddr *) & a4;
        }
        else
        {
            m_e = Impossible;
            return false;
        }

    int r = ::connect(m_fd, aa, aalen);

    if (r == SOCKET_ERROR)
    {
        switch (WSAGetLastError())
        {

            case WSANOTINITIALISED:
                m_e = Impossible;
                break;

            case WSAENETDOWN:
                m_e = NetworkFailure;
                break;

            case WSAEADDRINUSE:

            case WSAEINPROGRESS:

            case WSAENOBUFS:
                m_e = NoResources;
                break;

            case WSAEINTR:
                m_e = UnknownError; // ### ?
                break;

            case WSAEALREADY:
                // ### ?
                break;

            case WSAEADDRNOTAVAIL:
                m_e = ConnectionRefused; // ### ?
                break;

            case WSAEAFNOSUPPORT:

            case WSAEFAULT:
                m_e = InternalError;
                break;

            case WSAEINVAL:
                break;

            case WSAECONNREFUSED:
                m_e = ConnectionRefused;
                break;

            case WSAEISCONN:
                goto successful;

            case WSAENETUNREACH:

            case WSAETIMEDOUT:
                m_e = NetworkFailure;
                break;

            case WSAENOTSOCK:
                m_e = Impossible;
                break;

            case WSAEWOULDBLOCK:
                break;

            case WSAEACCES:
                m_e = Inaccessible;
                break;

            case 10107:
                // Workaround for a problem with the WinSock Proxy Server. See
                // also support/arc-12/25557 for details on the problem.
                goto successful;

            default:
                m_e = UnknownError;
                break;
        }

        return false;
    }

successful:

    fetchConnectionParameters();
    return true;
}


bool MSocketDevice::bind(const QHostAddress &address, quint16 port)
{
    if (!isValid())
        return false;

    int r;

    struct sockaddr_in a4;

#if !defined(QT_NO_IPV6)
    qt_sockaddr_in6 a6;

    if (initialized >= 0x20 && address.protocol() == QAbstractSocket::IPv6Protocol)
    {
        memset(&a6, 0, sizeof(a6));
        a6.sin6_family = AF_INET6;
        a6.sin6_port = htons(port);
        Q_IPV6ADDR tmp = address.toIPv6Address();
        memcpy(&a6.sin6_addr.qt_s6_addr, &tmp, sizeof(tmp));

        r = ::bind(m_fd, (struct sockaddr *) & a6, sizeof(struct qt_sockaddr_storage));
    }
    else
#endif
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            memset(&a4, 0, sizeof(a4));
            a4.sin_family = AF_INET;
            a4.sin_port = htons(port);
            a4.sin_addr.s_addr = htonl(address.toIPv4Address());

            r = ::bind(m_fd, (struct sockaddr*) & a4, sizeof(struct sockaddr_in));
        }
        else
        {
            m_e = Impossible;
            return false;
        }

    if (r == SOCKET_ERROR)
    {
        switch (WSAGetLastError())
        {

            case WSANOTINITIALISED:
                m_e = Impossible;
                break;

            case WSAENETDOWN:
                m_e = NetworkFailure;
                break;

            case WSAEACCES:
                m_e = Inaccessible;
                break;

            case WSAEADDRNOTAVAIL:
                m_e = Inaccessible;
                break;

            case WSAEFAULT:
                m_e = InternalError;
                break;

            case WSAEINPROGRESS:

            case WSAENOBUFS:
                m_e = NoResources;
                break;

            case WSAEADDRINUSE:

            case WSAEINVAL:
                m_e = AlreadyBound;
                break;

            case WSAENOTSOCK:
                m_e = Impossible;
                break;

            default:
                m_e = UnknownError;
                break;
        }

        return false;
    }

    fetchConnectionParameters();

    return true;
}


bool MSocketDevice::listen(int backlog)
{
    if (!isValid())
        return false;

    if (::listen(m_fd, backlog) >= 0)
        return true;

    if (!m_e)
        m_e = Impossible;

    return false;
}


int MSocketDevice::accept()
{
    if (!isValid())
        return -1;

#if !defined(QT_NO_IPV6)

    struct qt_sockaddr_storage a;

#else

    struct sockaddr a;

#endif
    SOCKLEN_T l = sizeof(a);

    bool done;

    SOCKET s;

    do
    {
        s = ::accept(m_fd, (struct sockaddr*) & a, &l);
        // we'll blithely throw away the stuff accept() wrote to a
        done = true;

        if (s == INVALID_SOCKET && m_e == NoError)
        {
            switch (WSAGetLastError())
            {

                case WSAEINTR:
                    done = false;
                    break;

                case WSANOTINITIALISED:
                    m_e = Impossible;
                    break;

                case WSAENETDOWN:

                case WSAEOPNOTSUPP:
                    // in all these cases, an error happened during connection
                    // setup.  we're not interested in what happened, so we
                    // just treat it like the client-closed-quickly case.
                    break;

                case WSAEFAULT:
                    m_e = InternalError;
                    break;

                case WSAEMFILE:

                case WSAEINPROGRESS:

                case WSAENOBUFS:
                    m_e = NoResources;
                    break;

                case WSAEINVAL:

                case WSAENOTSOCK:
                    m_e = Impossible;
                    break;

                case WSAEWOULDBLOCK:
                    break;

                default:
                    m_e = UnknownError;
                    break;
            }
        }
    }
    while (!done);

    return s;
}


qint64 MSocketDevice::bytesAvailable() const
{
    if (!isValid())
        return -1;

    u_long nbytes = 0;

    if (::ioctlsocket(m_fd, FIONREAD, &nbytes) < 0)
        return -1;

    // ioctlsocket sometimes reports 1 byte available for datagrams
    // while the following recvfrom returns -1 and claims connection
    // was reset (udp is connectionless). so we peek one byte to
    // catch this case and return 0 bytes available if recvfrom
    // fails.
    if (nbytes == 1 && m_t == Datagram)
    {
        char c;

        if (::recvfrom(m_fd, &c, sizeof(c), MSG_PEEK, nullptr, nullptr) == SOCKET_ERROR)
            return 0;
    }

    return nbytes;
}

qint64 MSocketDevice::waitForMore(std::chrono::milliseconds msecs, bool *timeout) const
{
    if (!isValid())
        return -1;

    fd_set fds;

    memset(&fds, 0, sizeof(fd_set));

    fds.fd_count = 1;

    fds.fd_array[0] = m_fd;

    struct timeval tv {};

    tv.tv_sec = msecs.count() / 1000;

    tv.tv_usec = (msecs.count() % 1000) * 1000;

    int rv = select(m_fd + 1, &fds, nullptr, nullptr, msecs < 0ms ? nullptr : &tv);

    if (rv < 0)
        return -1;

    if (timeout)
    {
        if (rv == 0)
            *timeout = true;
        else
            *timeout = false;
    }

    return bytesAvailable();
}


qint64 MSocketDevice::readData(char *data, qint64 maxlen)
{
    if (maxlen == 0)
        return 0;

#if defined(QT_CHECK_NULL)
    if (data == 0)
    {
        qWarning("MSocketDevice::readBlock: Null pointer error");
        return -1;
    }

#endif
#if defined(QT_CHECK_STATE)
    if (!isValid())
    {
        qWarning("MSocketDevice::readBlock: Invalid socket");
        return -1;
    }

    if (!isOpen())
    {
        qWarning("MSocketDevice::readBlock: Device is not open");
        return -1;
    }

    if (!isReadable())
    {
        qWarning("MSocketDevice::readBlock: Read operation not permitted");
        return -1;
    }

#endif
    qint64 r = 0;

    if (m_t == Datagram)
    {
#if !defined(QT_NO_IPV6)
        // With IPv6 support, we must be prepared to receive both IPv4
        // and IPv6 packets. The generic SOCKADDR_STORAGE (struct
        // sockaddr_storage on unix) replaces struct sockaddr.

        struct qt_sockaddr_storage a;
#else

        struct sockaddr_in a;
#endif
        memset(&a, 0, sizeof(a));
        SOCKLEN_T sz;
        sz = sizeof(a);
        r = ::recvfrom(m_fd, data, maxlen, 0, (struct sockaddr *) & a, &sz);
        qt_socket_getportaddr((struct sockaddr *)(&a), &m_pp, &m_pa);
    }
    else
    {
        r = ::recv(m_fd, data, maxlen, 0);
    }

    if (r == 0 && m_t == Stream && maxlen > 0)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
            // connection closed
            close();
        }
    }
    else if (r == SOCKET_ERROR && m_e == NoError)
    {
        switch (WSAGetLastError())
        {

            case WSANOTINITIALISED:
                m_e = Impossible;
                break;

            case WSAECONNABORTED:
                close();
                r = 0;
                break;

            case WSAETIMEDOUT:

            case WSAECONNRESET:
                /*
                From msdn doc:
                On a UDP datagram socket this error would indicate that a previous
                send operation resulted in an ICMP "Port Unreachable" message.

                So we should not close this socket just because one sendto failed.
                */

                if (m_t != Datagram)
                    close(); // connection closed

                r = 0;

                break;

            case WSAENETDOWN:

            case WSAENETRESET:
                m_e = NetworkFailure;

                break;

            case WSAEFAULT:

            case WSAENOTCONN:

            case WSAESHUTDOWN:

            case WSAEINVAL:
                m_e = Impossible;

                break;

            case WSAEINTR:
                // ### ?
                r = 0;

                break;

            case WSAEINPROGRESS:
                m_e = NoResources;

                break;

            case WSAENOTSOCK:
                m_e = Impossible;

                break;

            case WSAEOPNOTSUPP:
                m_e = InternalError; // ### ?

                break;

            case WSAEWOULDBLOCK:
                break;

            case WSAEMSGSIZE:
                m_e = NoResources; // ### ?

                break;

            case WSAEISCONN:
                // ### ?
                r = 0;

                break;

            default:
                m_e = UnknownError;

                break;
        }
    }

    return r;
}


qint64 MSocketDevice::writeData(const char *data, qint64 len)
{
    if (len == 0)
        return 0;

    if (data == nullptr)
    {
#if defined(QT_CHECK_NULL) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::writeBlock: Null pointer error");
#endif
        return -1;
    }

    if (!isValid())
    {
#if defined(QT_CHECK_STATE) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::writeBlock: Invalid socket");
#endif
        return -1;
    }

    if (!isOpen())
    {
#if defined(QT_CHECK_STATE) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::writeBlock: Device is not open");
#endif
        return -1;
    }

    if (!isWritable())
    {
#if defined(QT_CHECK_STATE) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::writeBlock: Write operation not permitted");
#endif
        return -1;
    }

    bool done = false;

    qint64 r = 0;

    while (!done)
    {
        // Don't write more than 64K (see Knowledge Base Q201213).
        r = ::send(m_fd, data, (len > 64 * 1024 ? 64 * 1024 : len), 0);
        done = true;

        if (r == SOCKET_ERROR && m_e == NoError)    //&& errno != WSAEAGAIN ) {
        {
            switch (WSAGetLastError())
            {

                case WSANOTINITIALISED:
                    m_e = Impossible;
                    break;

                case WSAENETDOWN:

                case WSAEACCES:

                case WSAENETRESET:

                case WSAESHUTDOWN:

                case WSAEHOSTUNREACH:
                    m_e = NetworkFailure;
                    break;

                case WSAECONNABORTED:

                case WSAECONNRESET:
                    // connection closed
                    close();
                    r = 0;
                    break;

                case WSAEINTR:
                    done = false;
                    break;

                case WSAEINPROGRESS:
                    m_e = NoResources;
                    // ### perhaps try it later?
                    break;

                case WSAEFAULT:

                case WSAEOPNOTSUPP:
                    m_e = InternalError;
                    break;

                case WSAENOBUFS:
                    // ### try later?
                    break;

                case WSAEMSGSIZE:
                    m_e = NoResources;
                    break;

                case WSAENOTCONN:

                case WSAENOTSOCK:

                case WSAEINVAL:
                    m_e = Impossible;
                    break;

                case WSAEWOULDBLOCK:
                    r = 0;
                    break;

                default:
                    m_e = UnknownError;
                    break;
            }
        }
    }

    return r;
}


qint64 MSocketDevice::writeBlock(const char * data, quint64 len,
                                 const QHostAddress & host, quint16 port)
{
    if (len == 0)
        return 0;

    if (m_t != Datagram)
    {
#if defined(QT_CHECK_STATE) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::sendBlock: Not datagram");
#endif
        return -1; // for now - later we can do t/tcp
    }

    if (data == nullptr)
    {
#if defined(QT_CHECK_NULL) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::sendBlock: Null pointer error");
#endif
        return -1;
    }

    if (!isValid())
    {
#if defined(QT_CHECK_STATE) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::sendBlock: Invalid socket");
#endif
        return -1;
    }

    if (!isOpen())
    {
#if defined(QT_CHECK_STATE) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::sendBlock: Device is not open");
#endif
        return -1;
    }

    if (!isWritable())
    {
#if defined(QT_CHECK_STATE) || defined(QSOCKETDEVICE_DEBUG)
        qWarning("MSocketDevice::sendBlock: Write operation not permitted");
#endif
        return -1;
    }

    struct sockaddr_in a4;

    struct sockaddr *aa;

    SOCKLEN_T slen;

#if !defined(QT_NO_IPV6)
    qt_sockaddr_in6 a6;

    if (initialized >= 0x20 && host.protocol() == QAbstractSocket::IPv6Protocol)
    {
        memset(&a6, 0, sizeof(a6));
        a6.sin6_family = AF_INET6;
        a6.sin6_port = htons(port);

        Q_IPV6ADDR tmp = host.toIPv6Address();
        memcpy(&a6.sin6_addr.qt_s6_addr, &tmp, sizeof(tmp));
        slen = sizeof(a6);
        aa = (struct sockaddr *) & a6;
    }
    else
#endif
        if (host.protocol() == QAbstractSocket::IPv4Protocol)
        {

            memset(&a4, 0, sizeof(a4));
            a4.sin_family = AF_INET;
            a4.sin_port = htons(port);
            a4.sin_addr.s_addr = htonl(host.toIPv4Address());
            slen = sizeof(a4);
            aa = (struct sockaddr *) & a4;
        }
        else
        {
            m_e = Impossible;
            return -1;
        }

    // we'd use MSG_DONTWAIT + MSG_NOSIGNAL if Stevens were right.
    // but apparently Stevens and most implementors disagree
    bool done = false;

    qint64 r = 0;

    while (!done)
    {
        r = ::sendto(m_fd, data, len, 0, aa, slen);
        done = true;

        if (r == SOCKET_ERROR && m_e == NoError)    //&& e != EAGAIN ) {
        {
            switch (WSAGetLastError())
            {

                case WSANOTINITIALISED:
                    m_e = Impossible;
                    break;

                case WSAENETDOWN:

                case WSAEACCES:

                case WSAENETRESET:

                case WSAESHUTDOWN:

                case WSAEHOSTUNREACH:

                case WSAECONNABORTED:

                case WSAECONNRESET:

                case WSAEADDRNOTAVAIL:

                case WSAENETUNREACH:

                case WSAETIMEDOUT:
                    m_e = NetworkFailure;
                    break;

                case WSAEINTR:
                    done = false;
                    break;

                case WSAEINPROGRESS:
                    m_e = NoResources;
                    // ### perhaps try it later?
                    break;

                case WSAEFAULT:

                case WSAEOPNOTSUPP:

                case WSAEAFNOSUPPORT:
                    m_e = InternalError;
                    break;

                case WSAENOBUFS:

                case WSAEMSGSIZE:
                    m_e = NoResources;
                    break;

                case WSAENOTCONN:

                case WSAENOTSOCK:

                case WSAEINVAL:

                case WSAEDESTADDRREQ:
                    m_e = Impossible;
                    break;

                case WSAEWOULDBLOCK:
                    r = 0;
                    break;

                default:
                    m_e = UnknownError;
                    break;
            }
        }
    }

    return r;
}


void MSocketDevice::fetchConnectionParameters()
{
    if (!isValid())
    {
        m_p = 0;
        m_a = QHostAddress();
        m_pp = 0;
        m_pa = QHostAddress();
        return;
    }

#if !defined (QT_NO_IPV6)

    struct qt_sockaddr_storage sa;

#else

    struct sockaddr_in sa;

#endif
    memset(&sa, 0, sizeof(sa));

    SOCKLEN_T sz;

    sz = sizeof(sa);

    if (!::getsockname(m_fd, (struct sockaddr *)(&sa), &sz))
        qt_socket_getportaddr((struct sockaddr *)(&sa), &m_p, &m_a);

    m_pp = 0;

    m_pa = QHostAddress();
}


void MSocketDevice::fetchPeerConnectionParameters()
{
    // do the getpeername() lazy on Windows (sales/arc-18/37759 claims that
    // there will be problems otherwise if you use MS Proxy server)
#if !defined (QT_NO_IPV6)

    struct qt_sockaddr_storage sa;
#else

    struct sockaddr_in sa;
#endif
    memset(&sa, 0, sizeof(sa));
    SOCKLEN_T sz;
    sz = sizeof(sa);

    if (!::getpeername(m_fd, (struct sockaddr *)(&sa), &sz))
        qt_socket_getportaddr((struct sockaddr *)(&sa), &m_pp, &m_pa);
}

quint16 MSocketDevice::peerPort() const
{
    if (m_pp == 0 && isValid())
    {
        MSocketDevice *that = (MSocketDevice*)this; // mutable
        that->fetchPeerConnectionParameters();
    }

    return m_pp;
}


QHostAddress MSocketDevice::peerAddress() const
{
    if (m_pp == 0 && isValid())
    {
        MSocketDevice *that = (MSocketDevice*)this; // mutable
        that->fetchPeerConnectionParameters();
    }

    return m_pa;
}

