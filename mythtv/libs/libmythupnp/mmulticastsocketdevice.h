//////////////////////////////////////////////////////////////////////////////
// Program Name: mmulticastsocketdevice.h
// Created     : Oct. 1, 2005
//
// Purpose     : Multicast QSocketDevice Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _MULTICAST_SOCKET_DEVICE_H_
#define _MULTICAST_SOCKET_DEVICE_H_

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
#include "msocketdevice.h"
#include "mythlogging.h"
#include "compat.h"

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
        m_local_addresses(QNetworkInterface::allAddresses()),
        m_port(0)
    {
        memset(&m_imr, 0, sizeof(struct ip_mreq));
    }

    MMulticastSocketDevice(QString sAddress, quint16 nPort, u_char ttl = 0);

    virtual ~MMulticastSocketDevice();

    virtual qint64 writeBlock(
        const char *data, quint64 len,
        const QHostAddress & host, quint16 port);

    virtual QHostAddress address() const { return m_address; }
    virtual quint16 port() const { return m_port; }

  private:
    QList<QHostAddress> m_local_addresses;
    QHostAddress        m_address;
    quint16             m_port;
    struct ip_mreq      m_imr;
};

#endif // _MULTICAST_SOCKET_DEVICE_H_
