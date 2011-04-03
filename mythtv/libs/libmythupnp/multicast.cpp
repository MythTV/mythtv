//////////////////////////////////////////////////////////////////////////////
// Program Name: multicast.cpp
// Created     : Mar. 3, 2011
//
// Purpose     : Multicast QSocketDevice Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
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

#include "multicast.h"

// Qt headers
#include <QByteArray>

// MythTV headers
#include "mythverbose.h"

#include <errno.h>

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


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// QMulticastSocket Class Definition/Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

QMulticastSocket::QMulticastSocket( QString sAddress, quint16 nPort, quint8 ttl )
                 : MSocketDevice( MSocketDevice::Datagram )
{
    m_address.setAddress( sAddress );
    m_port = nPort;

    //  ttl = UPnp::GetConfiguration()_pConfig->GetValue( "UPnP/TTL", 4 );

    if (ttl == 0)
        ttl = 4;

    // ----------------------------------------------------------------------
    // Set the numer of subnets to traverse (TTL)
    // ----------------------------------------------------------------------

    setsockopt( socket(), IPPROTO_IP, IP_MULTICAST_TTL, (char *)(&ttl), sizeof(ttl) );

    setAddressReusable( true );

    // ----------------------------------------------------------------------
    // bind to the local address/port (win32 requires QHostAddress::Any be used
    // ----------------------------------------------------------------------

    if (bind( QHostAddress::Any, m_port ) == false)
       VERBOSE(VB_IMPORTANT, QString( "QMulticastSocket: bind failed (%1)" ).arg(  GET_SOCKET_ERROR ));

    // ----------------------------------------------------------------------
    // Now join the Multicast group
    // ----------------------------------------------------------------------

    QByteArray addr = sAddress.toLatin1();

    struct ip_mreq  imr;

    imr.imr_multiaddr.s_addr = inet_addr( addr.constData() );
    imr.imr_interface.s_addr = htonl(INADDR_ANY);       

    if ( setsockopt( socket(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)(&imr), sizeof( imr )) != 0) 
        VERBOSE(VB_IMPORTANT, QString( "QMulticastSocket: setsockopt - IP_ADD_MEMBERSHIP Error (%1)" ).arg( GET_SOCKET_ERROR ));
}

QMulticastSocket::~QMulticastSocket()
{
    struct ip_mreq  imr;
    QByteArray addr = m_address.toString().toLatin1();

    imr.imr_multiaddr.s_addr = inet_addr( addr.constData() );
    imr.imr_interface.s_addr = htonl(INADDR_ANY);       

    setsockopt( socket(), IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)(&imr), sizeof(imr));
}
