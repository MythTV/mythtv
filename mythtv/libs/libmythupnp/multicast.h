//////////////////////////////////////////////////////////////////////////////
// Program Name: multicast.h
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

#ifndef __MULTICAST_H__
#define __MULTICAST_H__

// Qt headers
#include <QString>
#include <QHostAddress>

// MythTV headers
#include "msocketdevice.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// QMulticastSocket Class Definition/Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// -=>TODO: Need to add support for Multi-Homed machines.

class QMulticastSocket : public MSocketDevice
{
    public:

        QHostAddress    m_address;
        quint16         m_port;
 
    public:

        QMulticastSocket( QString sAddress, quint16 nPort, quint8 ttl = 0 );

        virtual ~QMulticastSocket();
};

#endif
