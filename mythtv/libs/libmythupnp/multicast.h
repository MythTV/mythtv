//////////////////////////////////////////////////////////////////////////////
// Program Name: multicast.h
//                                                                            
// Purpose - Multicast QSocketDevice Implmenetation
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __MULTICAST_H__
#define __MULTICAST_H__

// Qt headers
#include <QString>
#include <QByteArray>
#include <QHostAddress>

// MythTV headers
#include "msocketdevice.h"
#include "compat.h"
#include "mythverbose.h"

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
        struct ip_mreq  m_imr;

    public:

        QMulticastSocket( QString sAddress, quint16 nPort, u_char ttl = 0 )
         : MSocketDevice( MSocketDevice::Datagram )
        {
            m_address.setAddress( sAddress );
            m_port = nPort;

            if (ttl == 0)
                ttl = 4;

//            ttl = UPnp::g_pConfig->GetValue( "UPnP/TTL", 4 );
            QByteArray addr = sAddress.toLatin1();
            m_imr.imr_multiaddr.s_addr = inet_addr( addr.constData() );
            m_imr.imr_interface.s_addr = htonl(INADDR_ANY);       

            if ( setsockopt( socket(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &m_imr, sizeof( m_imr )) < 0) 
            {
                VERBOSE(VB_IMPORTANT, QString( "QMulticastSocket: setsockopt - IP_ADD_MEMBERSHIP Error" ));
            }

            setsockopt( socket(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl) );

            setAddressReusable( true );

            bind( m_address, m_port ); 
        }

        virtual ~QMulticastSocket()
        {
            setsockopt( socket(), IPPROTO_IP, IP_DROP_MEMBERSHIP, &m_imr, sizeof(m_imr));
        }
};

#endif
