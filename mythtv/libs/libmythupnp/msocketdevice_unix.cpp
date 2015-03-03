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

#include "qplatformdefs.h"
#include "mythlogging.h"

// Almost always the same. If not, specify in qplatformdefs.h.
#if !defined(QT_SOCKOPTLEN_T)
# define QT_SOCKOPTLEN_T QT_SOCKLEN_T
#endif

// Tru64 redefines accept -> _accept with _XOPEN_SOURCE_EXTENDED
static inline int qt_socket_accept(int s, struct sockaddr *addr, QT_SOCKLEN_T *addrlen)
{
    return ::accept(s, addr, addrlen);
}

#if defined(accept)
# undef accept
#endif

// UnixWare 7 redefines listen -> _listen
static inline int qt_socket_listen(int s, int backlog)
{
    return ::listen(s, backlog);
}

#if defined(listen)
# undef listen
#endif

// UnixWare 7 redefines socket -> _socket
static inline int qt_socket_socket(int domain, int type, int protocol)
{
    return ::socket(domain, type, protocol);
}

#if defined(socket)
# undef socket
#endif

#include "msocketdevice.h"

#include "qwindowdefs.h"

#include <errno.h>
#include <sys/types.h>

static inline void qt_socket_getportaddr(struct sockaddr *sa,
        quint16 *port, QHostAddress *addr)
{
#if !defined(QT_NO_IPV6)

    if (sa->sa_family == AF_INET6)
    {

        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
        Q_IPV6ADDR tmp;
        memcpy(&tmp, &sa6->sin6_addr.s6_addr, sizeof(tmp));
        QHostAddress a(tmp);
        *addr = a;
        *port = ntohs(sa6->sin6_port);
        return;
    }

#endif

    struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;

    QHostAddress a(ntohl(sa4->sin_addr.s_addr));

    *port = ntohs(sa4->sin_port);

    *addr = QHostAddress(ntohl(sa4->sin_addr.s_addr));

    return;
}

// internal
void MSocketDevice::init()
{
}


MSocketDevice::Protocol MSocketDevice::getProtocol() const
{
    if (isValid())
    {
#if !defined (QT_NO_IPV6)

        struct sockaddr_storage sa;
#else

        struct sockaddr sa;
#endif
        memset(&sa, 0, sizeof(sa));
        QT_SOCKLEN_T sz = sizeof(sa);
#if !defined (QT_NO_IPV6)

        struct sockaddr *sap = reinterpret_cast<struct sockaddr *>(&sa);

        if (!::getsockname(fd, sap, &sz))
        {
            switch (sap->sa_family)
            {

                case AF_INET:
                    return IPv4;

                case AF_INET6:
                    return IPv6;

                default:
                    return Unknown;
            }
        }

#else
        if (!::getsockname(fd, &sa, &sz))
        {
            switch (sa.sa_family)
            {

                case AF_INET:
                    return IPv4;

                default:
                    return Unknown;
            }
        }

#endif
    }

    return Unknown;
}

/*!
    Creates a new socket identifier. Returns -1 if there is a failure
    to create the new identifier; error() explains why.

    \sa setSocket()
*/

int MSocketDevice::createNewSocket()
{
#if !defined(QT_NO_IPV6)
    int s = qt_socket_socket(protocol() == IPv6 ? AF_INET6 : AF_INET,
                             t == Datagram ? SOCK_DGRAM : SOCK_STREAM, 0);
#else
    int s = qt_socket_socket(AF_INET, t == Datagram ? SOCK_DGRAM : SOCK_STREAM, 0);
#endif

    if (s < 0)
    {
        switch (errno)
        {

            case EPROTONOSUPPORT:
                e = InternalError; // 0 is supposed to work for both types
                break;

            case ENFILE:
                e = NoFiles; // special case for this
                break;

            case EACCES:
                e = Inaccessible;
                break;

            case ENOBUFS:

            case ENOMEM:
                e = NoResources;
                break;

            case EINVAL:
                e = Impossible;
                break;

            default:
                e = UnknownError;
                break;
        }
    }
    else
    {
        return s;
    }

    return -1;
}

/*!
    \reimp

    Closes the socket and sets the socket identifier to -1 (invalid).

    (This function ignores errors; if there are any then a file
    descriptor leakage might result. As far as we know, the only error
    that can arise is EBADF, and that would of course not cause
    leakage. There may be OS-specific errors that we haven't come
    across, however.)

    \sa open()
*/
void MSocketDevice::close()
{
    if (fd == -1 || !isOpen())                  // already closed
        return;

    setOpenMode(NotOpen);

    ::close(fd);

    LOG(VB_SOCKET, LOG_DEBUG,
        QString("MSocketDevice::close: Closed socket %1").arg(fd));

    fd = -1;

    fetchConnectionParameters();

    QIODevice::close();
}


/*!
    Returns true if the socket is valid and in blocking mode;
    otherwise returns false.

    Note that this function does not set error().

    \warning On Windows, this function always returns true since the
    ioctlsocket() function is broken.

    \sa setBlocking(), isValid()
*/
bool MSocketDevice::blocking() const
{
    if (!isValid())
        return true;

    int s = fcntl(fd, F_GETFL, 0);

    return !(s >= 0 && ((s & O_NDELAY) != 0));
}


/*!
    Makes the socket blocking if \a enable is true or nonblocking if
    \a enable is false.

    Sockets are blocking by default, but we recommend using
    nonblocking socket operations, especially for GUI programs that
    need to be responsive.

    \warning On Windows, this function should be used with care since
    whenever you use a QSocketNotifier on Windows, the socket is
    immediately made nonblocking.

    \sa blocking(), isValid()
*/
void MSocketDevice::setBlocking(bool enable)
{
    LOG(VB_SOCKET, LOG_DEBUG, QString("MSocketDevice::setBlocking(%1)")
        .arg((enable) ? "true" : "false"));

    if (!isValid())
        return;

    int tmp = ::fcntl(fd, F_GETFL, 0);

    if (tmp >= 0)
        tmp = ::fcntl(fd, F_SETFL, enable ? (tmp & ~O_NDELAY) : (tmp | O_NDELAY));

    if (tmp >= 0)
        return;

    if (e)
        return;

    switch (errno)
    {

        case EACCES:

        case EBADF:
            e = Impossible;
            break;

        case EFAULT:

        case EAGAIN:
#if EAGAIN != EWOULDBLOCK

        case EWOULDBLOCK:
#endif

        case EDEADLK:

        case EINTR:

        case EINVAL:

        case EMFILE:

        case ENOLCK:

        case EPERM:

        default:
            e = UnknownError;
    }
}


/*!
    Returns the value of the socket option \a opt.
*/
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
        QT_SOCKOPTLEN_T len;
        len = sizeof(v);
        int r = ::getsockopt(fd, SOL_SOCKET, n, (char*) & v, &len);

        if (r >= 0)
            return v;

        if (!e)
        {
            MSocketDevice *that = (MSocketDevice*)this; // mutable function

            switch (errno)
            {

                case EBADF:

                case ENOTSOCK:
                    that->e = Impossible;
                    break;

                case EFAULT:
                    that->e = InternalError;
                    break;

                default:
                    that->e = UnknownError;
                    break;
            }
        }

        return -1;
    }

    return v;
}


/*!
    Sets the socket option \a opt to \a v.
*/
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

    if (::setsockopt(fd, SOL_SOCKET, n, (char*)&v, sizeof(v)) < 0 &&
            e == NoError)
    {
        switch (errno)
        {

            case EBADF:

            case ENOTSOCK:
                e = Impossible;
                break;

            case EFAULT:
                e = InternalError;
                break;

            default:
                e = UnknownError;
                break;
        }
    }
}


/*!
    Connects to the IP address and port specified by \a addr and \a
    port. Returns true if it establishes a connection; otherwise returns false.
    If it returns false, error() explains why.

    Note that error() commonly returns NoError for non-blocking
    sockets; this just means that you can call connect() again in a
    little while and it'll probably succeed.
*/
bool MSocketDevice::connect(const QHostAddress &addr, quint16 port)
{
    if (isValid() && addr.protocol() != pa.protocol())
    {
        close();
        fd = -1;
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
        MSocketDevice::setSocket(createNewSocket(), t);

        // If still not valid, give up.

        if (!isValid())
            return false;
    }

    pa = addr;

    pp = port;

    struct sockaddr_in a4;

    struct sockaddr *aa;
    QT_SOCKLEN_T aalen;

#if !defined(QT_NO_IPV6)

    struct sockaddr_in6 a6;

    if (addr.protocol() == QAbstractSocket::IPv6Protocol)
    {
        memset(&a6, 0, sizeof(a6));
        a6.sin6_family = AF_INET6;
        a6.sin6_port = htons(port);
        Q_IPV6ADDR ip6 = addr.toIPv6Address();
        memcpy(&a6.sin6_addr.s6_addr, &ip6, sizeof(ip6));

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
            e = Impossible;
            return false;
        }

    int r = QT_SOCKET_CONNECT(fd, aa, aalen);

    if (r == 0)
    {
        fetchConnectionParameters();
        return true;
    }

    if (errno == EISCONN || errno == EALREADY || errno == EINPROGRESS)
    {
        fetchConnectionParameters();
        return true;
    }

    if (e != NoError || errno == EAGAIN || errno == EWOULDBLOCK)
    {
        return false;
    }

    switch (errno)
    {

        case EBADF:

        case ENOTSOCK:
            e = Impossible;
            break;

        case EFAULT:

        case EAFNOSUPPORT:
            e = InternalError;
            break;

        case ECONNREFUSED:
            e = ConnectionRefused;
            break;

        case ETIMEDOUT:

        case ENETUNREACH:
            e = NetworkFailure;
            break;

        case EADDRINUSE:
            e = NoResources;
            break;

        case EACCES:

        case EPERM:
            e = Inaccessible;
            break;

        default:
            e = UnknownError;
            break;
    }

    return false;
}


/*!
    Assigns a name to an unnamed socket. The name is the host address
    \a address and the port number \a port. If the operation succeeds,
    bind() returns true; otherwise it returns false without changing
    what port() and address() return.

    bind() is used by servers for setting up incoming connections.
    Call bind() before listen().
*/
bool MSocketDevice::bind(const QHostAddress &address, quint16 port)
{
    if (!isValid())
        return false;

    int r;

    struct sockaddr_in a4;

#if !defined(QT_NO_IPV6)

    struct sockaddr_in6 a6;

    if (address.protocol() == QAbstractSocket::IPv6Protocol)
    {
        memset(&a6, 0, sizeof(a6));
        a6.sin6_family = AF_INET6;
        a6.sin6_port = htons(port);
        Q_IPV6ADDR tmp = address.toIPv6Address();
        memcpy(&a6.sin6_addr.s6_addr, &tmp, sizeof(tmp));

        r = QT_SOCKET_BIND(fd, (struct sockaddr *) & a6, sizeof(a6));
    }
    else
#endif
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            memset(&a4, 0, sizeof(a4));
            a4.sin_family = AF_INET;
            a4.sin_port = htons(port);
            a4.sin_addr.s_addr = htonl(address.toIPv4Address());

            r = QT_SOCKET_BIND(fd, (struct sockaddr*) & a4, sizeof(a4));
        }
        else
        {
            e = Impossible;
            return false;
        }

    if (r < 0)
    {
        switch (errno)
        {

            case EINVAL:
                e = AlreadyBound;
                break;

            case EACCES:
                e = Inaccessible;
                break;

            case ENOMEM:
                e = NoResources;
                break;

            case EFAULT: // a was illegal

            case ENAMETOOLONG: // sz was wrong
                e = InternalError;
                break;

            case EBADF: // AF_UNIX only

            case ENOTSOCK: // AF_UNIX only

            case EROFS: // AF_UNIX only

            case ENOENT: // AF_UNIX only

            case ENOTDIR: // AF_UNIX only

            case ELOOP: // AF_UNIX only
                e = Impossible;
                break;

            default:
                e = UnknownError;
                break;
        }

        return false;
    }

    fetchConnectionParameters();

    return true;
}


/*!
    Specifies how many pending connections a server socket can have.
    Returns true if the operation was successful; otherwise returns
    false. A \a backlog value of 50 is quite common.

    The listen() call only applies to sockets where type() is \c
    Stream, i.e. not to \c Datagram sockets. listen() must not be
    called before bind() or after accept().

    \sa bind(), accept()
*/
bool MSocketDevice::listen(int backlog)
{
    if (!isValid())
        return false;

    if (qt_socket_listen(fd, backlog) >= 0)
        return true;

    if (!e)
        e = Impossible;

    return false;
}


/*!
    Extracts the first connection from the queue of pending
    connections for this socket and returns a new socket identifier.
    Returns -1 if the operation failed.

    \sa bind(), listen()
*/
int MSocketDevice::accept()
{
    if (!isValid())
        return -1;

#if !defined (QT_NO_IPV6)

    struct sockaddr_storage aa;

#else

    struct sockaddr aa;

#endif
    QT_SOCKLEN_T l = sizeof(aa);

    bool done;

    int s;

    do
    {
        s = qt_socket_accept(fd, (struct sockaddr*) & aa, &l);
        // we'll blithely throw away the stuff accept() wrote to aa
        done = true;

        if (s < 0 && e == NoError)
        {
            switch (errno)
            {

                case EINTR:
                    done = false;
                    break;
#if defined(EPROTO)

                case EPROTO:
#endif
#if defined(ENONET)

                case ENONET:
#endif

                case ENOPROTOOPT:

                case EHOSTDOWN:

                case EOPNOTSUPP:

                case EHOSTUNREACH:

                case ENETDOWN:

                case ENETUNREACH:

                case ETIMEDOUT:
                    // in all these cases, an error happened during connection
                    // setup.  we're not interested in what happened, so we
                    // just treat it like the client-closed-quickly case.

                case EPERM:
                    // firewalling wouldn't let us accept.  we treat it like
                    // the client-closed-quickly case.

                case EAGAIN:
#if EAGAIN != EWOULDBLOCK

                case EWOULDBLOCK:
#endif
                    // the client closed the connection before we got around
                    // to accept()ing it.
                    break;

                case EBADF:

                case ENOTSOCK:
                    e = Impossible;
                    break;

                case EFAULT:
                    e = InternalError;
                    break;

                case ENOMEM:

                case ENOBUFS:
                    e = NoResources;
                    break;

                default:
                    e = UnknownError;
                    break;
            }
        }
    }
    while (!done);

    return s;
}


/*!
    Returns the number of bytes available for reading, or -1 if an
    error occurred.

    \warning On Microsoft Windows, we use the ioctlsocket() function
    to determine the number of bytes queued on the socket. According
    to Microsoft (KB Q125486), ioctlsocket() sometimes returns an
    incorrect number. The only safe way to determine the amount of
    data on the socket is to read it using readBlock(). QSocket has
    workarounds to deal with this problem.
*/
qint64 MSocketDevice::bytesAvailable() const
{
    if (!isValid())
        return -1;

    /*
      Apparently, there is not consistency among different operating
      systems on how to use FIONREAD.

      FreeBSD, Linux and Solaris all expect the 3rd argument to
      ioctl() to be an int, which is normally 32-bit even on 64-bit
      machines.

      IRIX, on the other hand, expects a size_t, which is 64-bit on
      64-bit machines.

      So, the solution is to use size_t initialized to zero to make
      sure all bits are set to zero, preventing underflow with the
      FreeBSD/Linux/Solaris ioctls.
    */
    union { size_t st;
        int i;
    } nbytes;

    nbytes.st = 0;

    // gives shorter than true amounts on Unix domain sockets.

    if (::ioctl(fd, FIONREAD, (char*)&nbytes.i) < 0)
        return -1;

    return (qint64) nbytes.i + QIODevice::bytesAvailable();
}


/*!
    Wait up to \a msecs milliseconds for more data to be available. If
    \a msecs is -1 the call will block indefinitely.

    Returns the number of bytes available for reading, or -1 if an
    error occurred.

    If \a timeout is non-null and no error occurred (i.e. it does not
    return -1): this function sets *\a timeout to true, if the reason
    for returning was that the timeout was reached; otherwise it sets
    *\a timeout to false. This is useful to find out if the peer
    closed the connection.

    \warning This is a blocking call and should be avoided in event
    driven applications.

    \sa bytesAvailable()
*/
qint64 MSocketDevice::waitForMore(int msecs, bool *timeout) const
{
    if (!isValid())
        return -1;

    if (fd >= static_cast<int>(FD_SETSIZE))
        return -1;

    fd_set fds;

    struct timeval tv;

    FD_ZERO(&fds);

    FD_SET(fd, &fds);

    tv.tv_sec = msecs / 1000;

    tv.tv_usec = (msecs % 1000) * 1000;

    int rv = select(fd + 1, &fds, 0, 0, msecs < 0 ? 0 : &tv);

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


/*!
    Reads \a maxlen bytes from the socket into \a data and returns the
    number of bytes read. Returns -1 if an error occurred.
*/
qint64 MSocketDevice::readData(char *data, qint64 maxlen)
{
#if !defined(QT_NO_IPV6)

    struct sockaddr_storage aa;
#else

    struct sockaddr_in aa;
#endif

    if (maxlen == 0)
    {
        // Cannot short circuit on zero bytes for datagram because zero-
        // byte datagrams are a real thing and if you don't issue a read
        // then select() will keep falling through
        if (t == Datagram)
        {
            QT_SOCKLEN_T sz;
            sz = sizeof(aa);
            ::recvfrom(fd, data, 0, 0, (struct sockaddr *) & aa, &sz);
            qt_socket_getportaddr((struct sockaddr *)&aa, &pp, &pa);
        }
        return 0;
    }

    if (data == 0)
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::readBlock: Null pointer error");
        return -1;
    }

    if (!isValid())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::readBlock: Invalid socket");
        return -1;
    }

    if (!isOpen())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::readBlock: Device is not open");
        return -1;
    }

    if (!isReadable())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::readBlock: Read operation not permitted");
        return -1;
    }

    bool done = false;

    int r = 0;

    while (done == false)
    {
        if (t == Datagram)
        {
            memset(&aa, 0, sizeof(aa));
            QT_SOCKLEN_T sz;
            sz = sizeof(aa);
            r = ::recvfrom(fd, data, maxlen, 0,
                           (struct sockaddr *) & aa, &sz);

            qt_socket_getportaddr((struct sockaddr *)&aa, &pp, &pa);

        }
        else
        {
            r = ::read(fd, data, maxlen);
        }

        done = true;

        if (r == 0 && t == Stream && maxlen > 0)
        {
            // connection closed
            close();
        }
        else if (r >= 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // nothing
        }
        else if (errno == EINTR)
        {
            done = false;
        }
        else if (e == NoError)
        {
            switch (errno)
            {

                case EIO:

                case EISDIR:

                case EBADF:

                case EINVAL:

                case EFAULT:

                case ENOTCONN:

                case ENOTSOCK:
                    e = Impossible;
                    break;
#if defined(ENONET)

                case ENONET:
#endif

                case EHOSTUNREACH:

                case ENETDOWN:

                case ENETUNREACH:

                case ETIMEDOUT:
                    e = NetworkFailure;
                    break;

                case EPIPE:

                case ECONNRESET:
                    // connection closed
                    close();
                    r = 0;
                    break;

                default:
                    e = UnknownError;
                    break;
            }
        }
    }

    return r;
}


/*!
    Writes \a len bytes to the socket from \a data and returns the
    number of bytes written. Returns -1 if an error occurred.

    This is used for MSocketDevice::Stream sockets.
*/
qint64 MSocketDevice::writeData(const char *data, qint64 len)
{
    if (len == 0)
        return 0;

    if (data == 0)
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::writeBlock: Null pointer error");
        return -1;
    }

    if (!isValid())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::writeBlock: Invalid socket");
        return -1;
    }

    if (!isOpen())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::writeBlock: Device is not open");
        return -1;
    }

    if (!isWritable())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::writeBlock: Write operation not permitted");
        return -1;
    }

    bool done = false;

    int r = 0;
    bool timeout;

    while (!done)
    {
        r = ::write(fd, data, len);
        done = true;

        if (r < 0 && e == NoError &&
                errno != EAGAIN && errno != EWOULDBLOCK)
        {
            switch (errno)
            {

                case EINTR: // signal - call read() or whatever again
                    done = false;
                    break;

                case EPIPE:

                case ECONNRESET:
                    // connection closed
                    close();
                    r = 0;
                    break;

                case ENOSPC:

                case EIO:

                case EISDIR:

                case EBADF:

                case EINVAL:

                case EFAULT:

                case ENOTCONN:

                case ENOTSOCK:
                    e = Impossible;
                    break;
#if defined(ENONET)

                case ENONET:
#endif

                case EHOSTUNREACH:

                case ENETDOWN:

                case ENETUNREACH:

                case ETIMEDOUT:
                    e = NetworkFailure;
                    break;

                default:
                    e = UnknownError;
                    break;
            }
        }
        else if (waitForMore(0, &timeout) == 0)
        {
            if (!timeout)
            {
                // connection closed
                close();
            }
        }
    }

    return r;
}


/*!
    \overload

    Writes \a len bytes to the socket from \a data and returns the
    number of bytes written. Returns -1 if an error occurred.

    This is used for MSocketDevice::Datagram sockets. You must
    specify the \a host and \a port of the destination of the data.
*/
qint64 MSocketDevice::writeBlock(const char * data, quint64 len,
                                 const QHostAddress & host, quint16 port)
{
    if (len == 0)
        return 0;

    if (t != Datagram)
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::sendBlock: Not datagram");
        return -1; // for now - later we can do t/tcp
    }

    if (data == 0)
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::sendBlock: Null pointer error");
        return -1;
    }

    if (!isValid())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::sendBlock: Invalid socket");
        return -1;
    }

    if (!isOpen())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::sendBlock: Device is not open");
        return -1;
    }

    if (!isWritable())
    {
        LOG(VB_SOCKET, LOG_DEBUG,
            "MSocketDevice::sendBlock: Write operation not permitted");
        return -1;
    }

    struct sockaddr_in a4;

    struct sockaddr *aa;

    QT_SOCKLEN_T slen;

#if !defined(QT_NO_IPV6)

    struct sockaddr_in6 a6;

    if (host.protocol() == QAbstractSocket::IPv6Protocol)
    {
        memset(&a6, 0, sizeof(a6));
        a6.sin6_family = AF_INET6;
        a6.sin6_port = htons(port);

        Q_IPV6ADDR tmp = host.toIPv6Address();
        memcpy(&a6.sin6_addr.s6_addr, &tmp, sizeof(tmp));
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
            e = Impossible;
            return -1;
        }

    // we'd use MSG_DONTWAIT + MSG_NOSIGNAL if Stevens were right.
    // but apparently Stevens and most implementors disagree
    bool done = false;

    int r = 0;

    while (!done)
    {
        r = ::sendto(fd, data, len, 0, aa, slen);
        done = true;

        if (r < 0 && e == NoError &&
                errno != EAGAIN && errno != EWOULDBLOCK)
        {
            switch (errno)
            {

                case EINTR: // signal - call read() or whatever again
                    done = false;
                    break;

                case ENOSPC:

                case EPIPE:

                case EIO:

                case EISDIR:

                case EBADF:

                case EINVAL:

                case EFAULT:

                case ENOTCONN:

                case ENOTSOCK:
                    e = Impossible;
                    break;
#if defined(ENONET)

                case ENONET:
#endif

                case EHOSTUNREACH:

                case ENETDOWN:

                case ENETUNREACH:

                case ETIMEDOUT:
                    e = NetworkFailure;
                    break;

                default:
                    e = UnknownError;
                    break;
            }
        }
    }

    return r;
}


/*!
    Fetches information about both ends of the connection: whatever is
    available.
*/
void MSocketDevice::fetchConnectionParameters()
{
    if (!isValid())
    {
        p = 0;
        a = QHostAddress();
        pp = 0;
        pa = QHostAddress();
        return;
    }

#if !defined(QT_NO_IPV6)

    struct sockaddr_storage sa;

#else

    struct sockaddr_in sa;

#endif
    memset(&sa, 0, sizeof(sa));

    QT_SOCKLEN_T sz;

    sz = sizeof(sa);

    if (!::getsockname(fd, (struct sockaddr *)(&sa), &sz))
        qt_socket_getportaddr((struct sockaddr *)&sa, &p, &a);

    sz = sizeof(sa);

    if (!::getpeername(fd, (struct sockaddr *)(&sa), &sz))
        qt_socket_getportaddr((struct sockaddr *)&sa, &pp, &pa);
}


/*!
    Returns the port number of the port this socket device is
    connected to. This may be 0 for a while, but is set to something
    sensible as soon as a sensible value is available.

    Note that for Datagram sockets, this is the source port of the
    last packet received, and that it is in native byte order.
*/
quint16 MSocketDevice::peerPort() const
{
    return pp;
}


/*!
    Returns the address of the port this socket device is connected
    to. This may be 0.0.0.0 for a while, but is set to something
    sensible as soon as a sensible value is available.

    Note that for Datagram sockets, this is the source port of the
    last packet received.
*/
QHostAddress MSocketDevice::peerAddress() const
{
    return pa;
}

