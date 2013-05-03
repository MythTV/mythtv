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

#ifndef MSOCKETDEVICE_H
#define MSOCKETDEVICE_H

#include <QIODevice>
#include <QHostAddress> // int->QHostAddress conversion

#include "upnpexp.h"

class MSocketDevicePrivate;

class UPNP_PUBLIC MSocketDevice: public QIODevice
{

public:
    enum Type { Stream, Datagram };
    enum Protocol { IPv4, IPv6, Unknown };

    MSocketDevice(Type type = Stream);
    MSocketDevice(Type type, Protocol protocol, int dummy);
    MSocketDevice(int socket, Type type);
    virtual ~MSocketDevice();

    bool  isValid() const;
    Type  type() const;
    Protocol  protocol() const;

    void setProtocol(Protocol protocol);

    int   socket() const;
    virtual void setSocket(int socket, Type type);

    bool  open(OpenMode mode);
    bool  open(int mode)
    {
        return open((OpenMode)mode);
    }

    void  close();
    bool  flush();

    // Implementation of QIODevice abstract virtual functions
    qint64  size() const;
    qint64  pos() const;
    bool  seek(qint64);
    bool  atEnd() const;

    bool  blocking() const;
    virtual void setBlocking(bool);

    bool  broadcast() const;
    virtual void setBroadcast(bool);

    bool  addressReusable() const;
    virtual void setAddressReusable(bool);

    int   receiveBufferSize() const;
    virtual void setReceiveBufferSize(uint);
    int   sendBufferSize() const;
    virtual void setSendBufferSize(uint);

    bool  keepalive() const;
    virtual void setKeepalive(bool);

    virtual bool connect(const QHostAddress &, quint16);

    virtual bool bind(const QHostAddress &, quint16);
    virtual bool listen(int backlog);
    virtual int  accept();

    qint64  bytesAvailable() const;
    qint64  waitForMore(int msecs, bool *timeout = 0) const;
    virtual qint64 writeBlock(const char *data, quint64 len,
                              const QHostAddress & host, quint16 port);
    inline qint64  writeBlock(const char *data, quint64 len)
    {
        return qint64(write(data, qint64(len)));
    }

    inline qint64 readBlock(char *data, quint64 maxlen)
    {
        return qint64(read(data, qint64(maxlen)));
    }

    virtual quint16  port() const;
    virtual quint16  peerPort() const;
    virtual QHostAddress address() const;
    virtual QHostAddress peerAddress() const;

    enum Error
    {
        NoError,
        AlreadyBound,
        Inaccessible,
        NoResources,
        InternalError,
        Bug = InternalError, // ### remove in 4.0?
        Impossible,
        NoFiles,
        ConnectionRefused,
        NetworkFailure,
        UnknownError
    };
    Error  error() const;

    inline bool isSequential() const
    {
        return true;
    }

    int      createNewSocket();

protected:
    void setError(Error err);
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);

private:
    int fd;
    Type t;
    quint16 p;
    QHostAddress a;
    quint16 pp;
    QHostAddress pa;
    MSocketDevice::Error e;
    MSocketDevicePrivate * d;

    enum Option { Broadcast, ReceiveBuffer, ReuseAddress, SendBuffer, Keepalive };

    int   option(Option) const;
    virtual void setOption(Option, int);

    void  fetchConnectionParameters();
#if defined(Q_OS_WIN32) || defined(Q_OS_WINCE)
    void  fetchPeerConnectionParameters();
#endif

    static void  init();
    Protocol  getProtocol() const;

private: // Disabled copy constructor and operator=
#if defined(Q_DISABLE_COPY)
    MSocketDevice(const MSocketDevice &);
    MSocketDevice &operator=(const MSocketDevice &);
#endif
};

#endif // MSOCKETDEVICE_H
