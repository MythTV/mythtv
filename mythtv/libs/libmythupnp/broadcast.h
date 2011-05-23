//////////////////////////////////////////////////////////////////////////////
// Program Name: broadcast.h
// Created     : Oct. 1, 2005
//
// Purpose     : Broadcast QSocketDevice Implmenetation
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
                                                                       
#ifndef __BROADCAST_H__
#define __BROADCAST_H__

#include <QString>

#include "msocketdevice.h"

/////////////////////////////////////////////////////////////////////////////
// Broadcast Socket is used for XBox (original) since Multicast is not supported
/////////////////////////////////////////////////////////////////////////////

class QBroadcastSocket : public MSocketDevice
{
    public:

        QHostAddress    m_address;
        quint16         m_port;
        struct ip_mreq  m_imr;

    public:

        QBroadcastSocket( QString sAddress, quint16 nPort ) 
         : MSocketDevice( MSocketDevice::Datagram )
        {
            m_address.setAddress( sAddress );
            m_port = nPort;

            QByteArray addr = sAddress.toLatin1();
            setProtocol(IPv4);
            setSocket(createNewSocket(), MSocketDevice::Datagram);

            int one = 1;

            if ( setsockopt( socket(), SOL_SOCKET, SO_BROADCAST, &one, sizeof( one )) < 0) 
            {
                VERBOSE(VB_IMPORTANT, QString( "QBroadcastSocket: setsockopt - SO_BROADCAST Error" ));
            }

            setAddressReusable( true );

            bind( m_address, m_port ); 
        }

        virtual ~QBroadcastSocket()
        {
            int zero = 0;

            setsockopt( socket(), SOL_SOCKET, SO_BROADCAST, &zero, sizeof( zero ));
        }
};

#endif
