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

#include <chrono>

#include <QIODevice>
#include <QHostAddress> // int->QHostAddress conversion

#include "upnpexp.h"

using namespace std::chrono_literals;

class MSocketDevicePrivate;

class UPNP_PUBLIC MSocketDevice: public QIODevice
{

public:
    enum Type : std::uint8_t { Stream, Datagram };
    enum Protocol : std::uint8_t { IPv4, IPv6, Unknown };

    explicit MSocketDevice(Type type = Stream);
    MSocketDevice(Type type, Protocol protocol, int dummy);
    MSocketDevice(int socket, Type type);
    ~MSocketDevice() override;

    /*!
      Returns true if this is a valid socket; otherwise returns false.

      \sa socket()
    */
    bool  isValid() const
    {
        return m_fd != -1;
    }
    Type  type() const;
    Protocol  protocol() const;

    void setProtocol(Protocol protocol);

    int   socket() const;
    virtual void setSocket(int socket, Type type);

    bool  open(OpenMode mode) override; // QIODevice
    bool  open(int mode)
    {
        return MSocketDevice::open((OpenMode)mode);
    }

    void  close() override; // QIODevice
    static bool  flush();

    // Implementation of QIODevice abstract virtual functions
    qint64  size() const override; // QIODevice
    qint64  pos() const override; // QIODevice
    bool  seek(qint64 /*pos*/) override; // QIODevice
    bool  atEnd() const override; // QIODevice

    bool  blocking() const;
    virtual void setBlocking(bool enable);

    bool  broadcast() const;
    virtual void setBroadcast(bool enable);

    bool  addressReusable() const;
    virtual void setAddressReusable(bool enable);

    int   receiveBufferSize() const;
    virtual void setReceiveBufferSize(uint size);
    int   sendBufferSize() const;
    virtual void setSendBufferSize(uint size);

    bool  keepalive() const;
    virtual void setKeepalive(bool enable);

    virtual bool connect(const QHostAddress &addr, quint16 port);

    virtual bool bind(const QHostAddress &address, quint16 port);
    virtual bool listen(int backlog);
    virtual int  accept();

    qint64  bytesAvailable() const override; // QIODevice
    qint64  waitForMore(std::chrono::milliseconds msecs, bool *timeout = nullptr) const;
    virtual qint64 writeBlock(const char *data, quint64 len,
                              const QHostAddress & host, quint16 port);
    qint64  writeBlock(const char *data, quint64 len)
    {
        return write(data, len);
    }

    qint64 readBlock(char *data, quint64 maxlen)
    {
        // read() does not correctly handle zero-byte udp packets
        // so call readData() directly which does
        return maxlen ? read(data, maxlen) : readData(data, maxlen);
    }

    virtual quint16  port() const;
    virtual quint16  peerPort() const;
    virtual QHostAddress address() const;
    virtual QHostAddress peerAddress() const;

    enum Error : std::uint8_t
    {
        NoError = 0,
        AlreadyBound = 1,
        Inaccessible = 2,
        NoResources = 3,
        InternalError = 4,
        Bug = InternalError, // ### remove in 4.0?
        Impossible = 5,
        NoFiles = 6,
        ConnectionRefused = 7,
        NetworkFailure = 8,
        UnknownError = 9
    };
    Error  error() const;

    bool isSequential() const override // QIODevice
    {
        return true;
    }

    int      createNewSocket();

protected:
    void setError(Error err);
    qint64 readData(char *data, qint64 maxlen) override; // QIODevice
    qint64 writeData(const char *data, qint64 len) override; // QIODevice

private:
    int                   m_fd {-1};
    Type                  m_t;
    quint16               m_p  {0};
    QHostAddress          m_a;
    quint16               m_pp {0};
    QHostAddress          m_pa;
    MSocketDevice::Error  m_e {NoError};
    MSocketDevicePrivate *d{nullptr}; // NOLINT(readability-identifier-naming)

    enum Option : std::uint8_t { Broadcast, ReceiveBuffer, ReuseAddress, SendBuffer, Keepalive };

    int   option(Option opt) const;
    virtual void setOption(Option opt, int v);

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
