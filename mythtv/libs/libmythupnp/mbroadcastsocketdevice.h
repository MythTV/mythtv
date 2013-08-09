//////////////////////////////////////////////////////////////////////////////
// Program Name: broadcast.h
// Created     : Oct. 1, 2005
//
// Purpose     : Broadcast QSocketDevice Implmenetation
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////
                                                                       
#ifndef _MBROADCAST_SOCKET_DEVICE_H_
#define _MBROADCAST_SOCKET_DEVICE_H_

#include <QString>

#include "msocketdevice.h"
#include "mythlogging.h"

/////////////////////////////////////////////////////////////////////////////
// Broadcast Socket is used for XBox (original) since Multicast is not supported
/////////////////////////////////////////////////////////////////////////////

class MBroadcastSocketDevice : public MSocketDevice
{
  public:
    MBroadcastSocketDevice(QString sAddress, quint16 nPort) :
        MSocketDevice(MSocketDevice::Datagram),
        m_address(sAddress), m_port(nPort)
    {
        m_address.setAddress( sAddress );
        m_port = nPort;

        QByteArray addr = sAddress.toLatin1();
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

    virtual ~MBroadcastSocketDevice()
    {
        int zero = 0;
        if (setsockopt(socket(), SOL_SOCKET, SO_BROADCAST, (const char *)&zero,
                       sizeof(zero)) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "setsockopt - SO_BROADCAST Error" + ENO);
        }
    }

    virtual QHostAddress address() const { return m_address; }
    virtual quint16 port() const { return m_port; }

  private:
    QHostAddress    m_address;
    quint16         m_port;
};

#endif // _MBROADCAST_SOCKET_DEVICE_H_
