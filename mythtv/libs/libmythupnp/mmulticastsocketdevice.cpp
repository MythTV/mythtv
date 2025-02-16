//////////////////////////////////////////////////////////////////////////////
// Program Name: mmulticastsocketdevice.cpp
// Created     : Oct. 1, 2005
//
// Purpose     : Multicast QSocketDevice Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "mmulticastsocketdevice.h"

#include <cerrno>
#include "libmythbase/mythconfig.h"

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# define GET_SOCKET_ERROR    WSAGetLastError()
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# define GET_SOCKET_ERROR    errno
#endif

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt headers
#include <QStringList>

// MythTV headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"

#define LOC      QString("MMulticastSocketDevice(%1:%2): ") \
                     .arg(m_address.toString()).arg(socket())

MMulticastSocketDevice::MMulticastSocketDevice(
    const QString& sAddress, quint16 nPort, u_char ttl) :
    MSocketDevice(MSocketDevice::Datagram),
    m_address(sAddress), m_port(nPort)
{
#if 0
    ttl = XmlConfiguration().GetValue( "UPnP/TTL", 4 );
#endif

    if (ttl == 0)
        ttl = 4;

    setProtocol(IPv4);
    setSocket(createNewSocket(), MSocketDevice::Datagram);

    m_imr.imr_multiaddr.s_addr = inet_addr(sAddress.toLatin1().constData());
    m_imr.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socket(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   (const char *)&m_imr, sizeof( m_imr )) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "setsockopt - IP_ADD_MEMBERSHIP " + ENO);
    }

    if (setsockopt(socket(), IPPROTO_IP, IP_MULTICAST_TTL,
                   (const char *)&ttl, sizeof(ttl)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "setsockopt - IP_MULTICAST_TTL " + ENO);
    }

    setAddressReusable(true);

    if (!bind(m_address, m_port))
        LOG(VB_GENERAL, LOG_ERR, LOC + "bind failed");
}

MMulticastSocketDevice::~MMulticastSocketDevice()
{
    if (!m_address.isNull() &&
        (setsockopt(socket(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
                    (char*)(&m_imr), sizeof(m_imr)) < 0))
    {
        // This isn't really an error, we will drop out of
        // the group anyway when we close the socket.
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "setsockopt - IP_DROP_MEMBERSHIP " +
            ENO);
    }
}

qint64 MMulticastSocketDevice::writeBlock(
    const char *data, quint64 len,
    const QHostAddress & host, quint16 port)
{
#ifdef IP_MULTICAST_IF
    if (host.toString() == "239.255.255.250")
    {
        int retx = 0;
        for (const auto & address : std::as_const(m_localAddresses))
        {
            if (address.protocol() != QAbstractSocket::IPv4Protocol)
                continue; // skip IPv6 addresses

            QString addr = address.toString();
            if (addr == "127.0.0.1")
                continue; // skip localhost address

            uint32_t interface_addr = address.toIPv4Address();
            if (setsockopt(socket(), IPPROTO_IP, IP_MULTICAST_IF,
                           (const char *)&interface_addr,
                           sizeof(interface_addr)) < 0)
            {
                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    "setsockopt - IP_MULTICAST_IF " + ENO);
            }
            retx = MSocketDevice::writeBlock(data, len, host, port);
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("writeBlock on %1 %2")
                    .arg((*it).toString()).arg((retx==(int)len)?"ok":"err"));
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(MythRandom(5, 9)));
        }
        return retx;
    }
#endif

    return MSocketDevice::writeBlock(data, len, host, port);
}
