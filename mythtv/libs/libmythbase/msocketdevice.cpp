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
#include <string.h>

//#define MSOCKETDEVICE_DEBUG


class MSocketDevicePrivate
{

public:
    MSocketDevicePrivate(MSocketDevice::Protocol p)
            : protocol(p)
    { }

    MSocketDevice::Protocol protocol;
};


/*!
    \class MSocketDevice
    \brief The MSocketDevice class provides a platform-independent low-level socket API.

    \compat
    \reentrant

    This class provides a low level API for working with sockets.  Users of
    this class are assumed to have networking experience. For most users the
    MSocket class provides a much easier and high level alternative, but
    certain things (like UDP) can't be done with MSocket and if you need a
    platform-independent API for those, MSocketDevice is the right choice.

    The essential purpose of the class is to provide a QIODevice that
    works on sockets, wrapped in a platform-independent API.

    When calling connect() or bind(), MSocketDevice detects the
    protocol family (IPv4, IPv6) automatically. Passing the protocol
    family to MSocketDevice's constructor or to setSocket() forces
    creation of a socket device of a specific protocol. If not set, the
    protocol will be detected at the first call to connect() or bind().

    \sa MSocket, QSocketNotifier, QHostAddress
*/


/*!
    \enum MSocketDevice::Protocol

    This enum type describes the protocol family of the socket. Possible values
    are:

    \value IPv4 The socket is an IPv4 socket.
    \value IPv6 The socket is an IPv6 socket.
    \value Unknown The protocol family of the socket is not known. This can
    happen if you use MSocketDevice with an already existing socket; it
    tries to determine the protocol family, but this can fail if the
    protocol family is not known to MSocketDevice.

    \sa protocol() setSocket()
*/

/*!
    \enum MSocketDevice::Error

    This enum type describes the error states of MSocketDevice.

    \value NoError  No error has occurred.

    \value AlreadyBound  The device is already bound, according to bind().

    \value Inaccessible  The operating system or firewall prohibited
   the action.

    \value NoResources  The operating system ran out of a resource.

    \value InternalError  An internal error occurred in MSocketDevice.

    \value Impossible  An attempt was made to do something which makes
    no sense. For example:
    \snippet doc/src/snippets/code/src.qt3support.network.q3socketdevice.cpp 0
    The libc ::close() closes the socket, but MSocketDevice is not aware
    of this. So when you call writeBlock(), the impossible happens.

    \value NoFiles  The operating system will not let MSocketDevice open
    another file.

    \value ConnectionRefused  A connection attempt was rejected by the
    peer.

    \value NetworkFailure  There is a network failure.

    \value UnknownError  The operating system did something
    unexpected.

    \omitvalue Bug
*/

/*!
    \enum MSocketDevice::Type

    This enum type describes the type of the socket:
    \value Stream  a stream socket (TCP, usually)
    \value Datagram  a datagram socket (UDP, usually)
*/


/*!
    Creates a MSocketDevice object for the existing socket \a socket.

    The \a type argument must match the actual socket type; use \c
    MSocketDevice::Stream for a reliable, connection-oriented TCP
    socket, or MSocketDevice::Datagram for an unreliable,
    connectionless UDP socket.
*/
MSocketDevice::MSocketDevice(int socket, Type type)
        : fd(socket), t(type), p(0), pp(0), e(NoError),
        d(new MSocketDevicePrivate(Unknown))
{
#if defined(MSOCKETDEVICE_DEBUG)
    qDebug("MSocketDevice: Created MSocketDevice %p (socket %x, type %d)",
           this, socket, type);
#endif
    init();
    setSocket(socket, type);
}

/*!
    Creates a MSocketDevice object for a stream or datagram socket.

    The \a type argument must be either MSocketDevice::Stream for a
    reliable, connection-oriented TCP socket, or \c
    MSocketDevice::Datagram for an unreliable UDP socket.
    The socket protocol type is defaulting to unknown leaving it to
    connect() to determine if an IPv6 or IPv4 type is required.

    \sa blocking() protocol()
*/
MSocketDevice::MSocketDevice(Type type)
        : fd(-1), t(type), p(0), pp(0), e(NoError),
        d(new MSocketDevicePrivate(Unknown))
{
#if defined(MSOCKETDEVICE_DEBUG)
    qDebug("MSocketDevice: Created MSocketDevice object %p, type %d",
           this, type);
#endif
    init();
    setSocket(createNewSocket(), type);
}

/*!
    Creates a MSocketDevice object for a stream or datagram socket.

    The \a type argument must be either MSocketDevice::Stream for a
    reliable, connection-oriented TCP socket, or \c
    MSocketDevice::Datagram for an unreliable UDP socket.

    The \a protocol indicates whether the socket should be of type IPv4
    or IPv6. Passing \c Unknown is not meaningful in this context and you
    should avoid using (it creates an IPv4 socket, but your code is not easily
    readable).

    The argument \a dummy is necessary for compatibility with some
    compilers.

    \sa blocking() protocol()
*/
MSocketDevice::MSocketDevice(Type type, Protocol protocol, int)
        : fd(-1), t(type), p(0), pp(0), e(NoError),
        d(new MSocketDevicePrivate(protocol))
{
#if defined(MSOCKETDEVICE_DEBUG)
    qDebug("MSocketDevice: Created MSocketDevice object %p, type %d",
           this, type);
#endif
    init();
    setSocket(createNewSocket(), type);
}

/*!
    Destroys the socket device and closes the socket if it is open.
*/
MSocketDevice::~MSocketDevice()
{
    close();
    delete d;
    d = 0;
#if defined(MSOCKETDEVICE_DEBUG)
    qDebug("MSocketDevice: Destroyed MSocketDevice %p", this);
#endif
}


/*!
    Returns true if this is a valid socket; otherwise returns false.

    \sa socket()
*/
bool MSocketDevice::isValid() const
{
    return fd != -1;
}


/*!
    \fn Type MSocketDevice::type() const

    Returns the socket type which is either MSocketDevice::Stream
    or MSocketDevice::Datagram.

    \sa socket()
*/
MSocketDevice::Type MSocketDevice::type() const
{
    return t;
}

/*!
    Returns the socket's protocol family, which is one of \c Unknown, \c IPv4,
    or \c IPv6.

    MSocketDevice either creates a socket with a well known protocol family or
    it uses an already existing socket. In the first case, this function
    returns the protocol family it was constructed with. In the second case, it
    tries to determine the protocol family of the socket; if this fails, it
    returns \c Unknown.

    \sa Protocol setSocket()
*/
MSocketDevice::Protocol MSocketDevice::protocol() const
{
    if (d->protocol == Unknown)
        d->protocol = getProtocol();

    return d->protocol;
}

void MSocketDevice::setProtocol(Protocol protocol)
{
    d->protocol = protocol;
}

/*!
    Returns the socket number, or -1 if it is an invalid socket.

    \sa isValid(), type()
*/
int MSocketDevice::socket() const
{
    return fd;
}


/*!
    Sets the socket device to operate on the existing socket \a
    socket.

    The \a type argument must match the actual socket type; use \c
    MSocketDevice::Stream for a reliable, connection-oriented TCP
    socket, or MSocketDevice::Datagram for an unreliable,
    connectionless UDP socket.

    Any existing socket is closed.

    \sa isValid(), close()
*/
void MSocketDevice::setSocket(int socket, Type type)
{
    if (fd != -1)     // close any open socket
        close();

#if defined(MSOCKETDEVICE_DEBUG)
    qDebug("MSocketDevice::setSocket: socket %x, type %d", socket, type);

#endif
    t = type;

    fd = socket;

    d->protocol = Unknown;

    e = NoError;

    //resetStatus();
    open(ReadWrite);

    fetchConnectionParameters();
}


/*!
    Opens the socket using the specified QIODevice file \a mode. This
    function is called from the MSocketDevice constructors and from
    the setSocket() function. You should not call it yourself.

    \sa close()
*/
bool MSocketDevice::open(OpenMode mode)
{
    if (isOpen() || !isValid())
        return false;

#if defined(MSOCKETDEVICE_DEBUG)
    qDebug("MSocketDevice::open: mode %x", mode);

#endif
    setOpenMode((mode & ReadWrite) | Unbuffered);

    return true;
}

/*!
    \fn bool MSocketDevice::open(int mode)
    \overload
*/
/*!
    The current MSocketDevice implementation does not buffer at all,
    so this is a no-op. This function always returns true.
*/
bool MSocketDevice::flush()
{
    return true;
}


/*!
    \reimp

    The size is meaningless for a socket, therefore this function returns 0.
*/
qint64 MSocketDevice::size() const
{
    return 0;
}


/*!
    The read/write index is meaningless for a socket, therefore this
    function returns 0.
*/
qint64 MSocketDevice::pos() const
{
    return 0;
}


/*!
    The read/write index is meaningless for a socket, therefore this
    function does nothing and returns true.

    The \a offset parameter is ignored.
*/
bool MSocketDevice::seek(qint64 /* pos */)
{
    return true;
}


/*!
    \reimp

    Returns true if no data is currently available at the socket;
    otherwise returns false.
*/
bool MSocketDevice::atEnd() const
{
    return bytesAvailable() <= 0;
}

/*!
    Returns true if this socket has the broadcast option set.

    \sa setBroadcast()
*/
bool MSocketDevice::broadcast() const
{
    return option(Broadcast);
}

/*!
    Sets the broadcast option for this socket.

    \sa broadcast()
*/
void MSocketDevice::setBroadcast(bool enable)
{
    setOption(Broadcast, enable);
}

/*!
    Returns true if the address of this socket can be used by other
    sockets at the same time, and false if this socket claims
    exclusive ownership.

    \sa setAddressReusable()
*/
bool MSocketDevice::addressReusable() const
{
    return option(ReuseAddress);
}


/*!
    Sets the address of this socket to be usable by other sockets too
    if \a enable is true, and to be used exclusively by this socket if
    \a enable is false.

    When a socket is reusable, other sockets can use the same port
    number (and IP address), which is generally useful. Of course
    other sockets cannot use the same
    (address,port,peer-address,peer-port) 4-tuple as this socket, so
    there is no risk of confusing the two TCP connections.

    \sa addressReusable()
*/
void MSocketDevice::setAddressReusable(bool enable)
{
    setOption(ReuseAddress, enable);
}


/*!
    Returns true if this socket has the keepalive option set.

    \sa setKeepalive()
*/
bool MSocketDevice::keepalive() const
{
    return option(Keepalive);
}

/*!
    Sets the keepalive option for this socket.

    \sa keepalive()
*/
void MSocketDevice::setKeepalive(bool enable)
{
    setOption(Keepalive, enable);
}


/*!
    Returns the size of the operating system receive buffer.

    \sa setReceiveBufferSize()
*/
int MSocketDevice::receiveBufferSize() const
{
    return option(ReceiveBuffer);
}


/*!
    Sets the size of the operating system receive buffer to \a size.

    The operating system receive buffer size effectively limits two
    things: how much data can be in transit at any one moment, and how
    much data can be received in one iteration of the main event loop.

    The default is operating system-dependent. A socket that receives
    large amounts of data is probably best with a buffer size of
    49152.
*/
void MSocketDevice::setReceiveBufferSize(uint size)
{
    setOption(ReceiveBuffer, size);
}


/*!
    Returns the size of the operating system send buffer.

    \sa setSendBufferSize()
*/
int MSocketDevice::sendBufferSize() const
{
    return option(SendBuffer);
}


/*!
    Sets the size of the operating system send buffer to \a size.

    The operating system send buffer size effectively limits how much
    data can be in transit at any one moment.

    The default is operating system-dependent. A socket that sends
    large amounts of data is probably best with a buffer size of
    49152.
*/
void MSocketDevice::setSendBufferSize(uint size)
{
    setOption(SendBuffer, size);
}


/*!
    Returns the port number of this socket device. This may be 0 for a
    while, but is set to something sensible as soon as a sensible
    value is available.

    Note that Qt always uses native byte order, i.e. 67 is 67 in Qt;
    there is no need to call htons().
*/
quint16 MSocketDevice::port() const
{
    return p;
}


/*!
    Returns the address of this socket device. This may be 0.0.0.0 for
    a while, but is set to something sensible as soon as a sensible
    value is available.
*/
QHostAddress MSocketDevice::address() const
{

    QString ipaddress;

    if (a.toString().startsWith("0:0:0:0:0:FFFF:"))
    {
        Q_IPV6ADDR addr = a.toIPv6Address();
        // addr contains 16 unsigned characters

        ipaddress = QString("%1.%2.%3.%4").arg(addr[12]).arg(addr[13]).arg(addr[14]).arg(addr[15]);
    }
    else
        ipaddress = a.toString();

    return QHostAddress(ipaddress);
}


/*!
    Returns the first error seen.
*/
MSocketDevice::Error MSocketDevice::error() const
{
    return e;
}


/*!
    Allows subclasses to set the error state to \a err.
*/
void MSocketDevice::setError(Error err)
{
    e = err;
}

/*! \fn MSocketDevice::readBlock(char *data, Q_ULONG maxlen)

    Reads \a maxlen bytes from the socket into \a data and returns the
    number of bytes read. Returns -1 if an error occurred. Returning 0
    is not an error. For Stream sockets, 0 is returned when the remote
    host closes the connection. For Datagram sockets, 0 is a valid
    datagram size.
*/

/*! \fn MSocketDevice::writeBlock(const char *data, Q_ULONG len)

    Writes \a len bytes to the socket from \a data and returns the
    number of bytes written. Returns -1 if an error occurred.

    This is used for MSocketDevice::Stream sockets.
*/

/*!
    \fn bool MSocketDevice::isSequential() const
    \internal
*/

