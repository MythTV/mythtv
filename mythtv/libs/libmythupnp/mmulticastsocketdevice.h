//////////////////////////////////////////////////////////////////////////////
// Program Name: mmulticastsocketdevice.h
// Created     : Oct. 1, 2005
//
// Purpose     : Multicast QSocketDevice Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MULTICAST_SOCKET_DEVICE_H
#define MULTICAST_SOCKET_DEVICE_H

#ifdef __FreeBSD__
#  include <sys/types.h>
#endif

#ifdef _WIN32
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
#endif

// Qt headers
#include <QNetworkInterface>
#include <QHostAddress>
#include <QByteArray>
#include <QString>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/mythlogging.h"

#include "msocketdevice.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// MMulticastSocketDevice Class Definition/Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class MMulticastSocketDevice : public MSocketDevice
{
  public:
    MMulticastSocketDevice() :
        MSocketDevice(MSocketDevice::Datagram),
        m_localAddresses(QNetworkInterface::allAddresses()) {}
    MMulticastSocketDevice(const QString& sAddress, quint16 nPort, u_char ttl = 0);

    ~MMulticastSocketDevice() override;

    qint64 writeBlock(
        const char *data, quint64 len,
        const QHostAddress & host, quint16 port) override; // MSocketDevice

    QHostAddress address() const override // MSocketDevice
        { return m_address; }
    quint16 port() const override // MSocketDevice
        { return m_port; }

  private:
    QList<QHostAddress> m_localAddresses;
    QHostAddress        m_address;
    quint16             m_port {0};
    struct ip_mreq      m_imr {};
};

#endif // MULTICAST_SOCKET_DEVICE_H
