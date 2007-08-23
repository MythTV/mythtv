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

#include <qsocketdevice.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// QMulticastSocket Class Definition/Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// -=>TODO: Need to add support for Multi-Homed machines.

class QMulticastSocket : public QSocketDevice
{
    public:

        QHostAddress    m_address;
        Q_UINT16        m_port;
        struct ip_mreq  m_imr;

    public:

        QMulticastSocket( QString sAddress, Q_UINT16 nPort, u_char ttl = 0 )
         : QSocketDevice( QSocketDevice::Datagram )
        {
            m_address.setAddress( sAddress );
            m_port = nPort;

            if (ttl == 0)
                ttl = 4;

//            ttl = UPnp::g_pConfig->GetValue( "UPnP/TTL", 4 );

            m_imr.imr_multiaddr.s_addr = inet_addr( sAddress );
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
