//////////////////////////////////////////////////////////////////////////////
// Program Name: mmulticastsocketdevice.h
// Created     : Oct. 1, 2005
//
// Purpose     : Multicast QSocketDevice Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _MULTICAST_SOCKET_DEVICE_H_
#define _MULTICAST_SOCKET_DEVICE_H_

#ifdef _WIN32
# ifndef _MSC_VER
#  include <ws2tcpip.h>
# endif
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
