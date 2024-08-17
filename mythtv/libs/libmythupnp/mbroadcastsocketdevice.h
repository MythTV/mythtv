//////////////////////////////////////////////////////////////////////////////
// Program Name: broadcast.h
// Created     : Oct. 1, 2005
//
// Purpose     : Broadcast QSocketDevice Implmenetation
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
                                                                       
#ifndef MBROADCAST_SOCKET_DEVICE_H
#define MBROADCAST_SOCKET_DEVICE_H

#include <QString>

#include "libmythbase/mythlogging.h"

#include "msocketdevice.h"

/////////////////////////////////////////////////////////////////////////////
// Broadcast Socket is used for XBox (original) since Multicast is not supported
/////////////////////////////////////////////////////////////////////////////

class MBroadcastSocketDevice : public MSocketDevice
{
  public:
    MBroadcastSocketDevice(const QString& sAddress, quint16 nPort) :
        MSocketDevice(MSocketDevice::Datagram),
        m_address(sAddress), m_port(nPort)
    {
        m_address.setAddress( sAddress );

        setProtocol(IPv4);
        setSocket(createNewSocket(), MSocketDevice::Datagram);

        int one = 1;
        if (setsockopt(socket(), SOL_SOCKET, SO_BROADCAST,
                       (const char *)&one, sizeof(one)) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "setsockopt - SO_BROADCAST Error" + ENO);
        }

        setAddressReusable(true);

        bind(m_address, m_port);
    }

    ~MBroadcastSocketDevice() override
    {
        int zero = 0;
        if (setsockopt(socket(), SOL_SOCKET, SO_BROADCAST, (const char *)&zero,
                       sizeof(zero)) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "setsockopt - SO_BROADCAST Error" + ENO);
        }
    }

    QHostAddress address() const override // MSocketDevice
        { return m_address; }
    quint16 port() const override // MSocketDevice
        { return m_port; }

  private:
    QHostAddress    m_address;
    quint16         m_port;
};

#endif // MBROADCAST_SOCKET_DEVICE_H
