//////////////////////////////////////////////////////////////////////////////
// Program Name: broadcast.h
//                                                                            
// Purpose - Broadcast QSocketDevice Implmenetation
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __BROADCAST_H__
#define __BROADCAST_H__

#include <qsocketdevice.h>

/////////////////////////////////////////////////////////////////////////////
// Broadcast Socket is used for XBox (original) since Multicast is not supported
/////////////////////////////////////////////////////////////////////////////

class QBroadcastSocket : public QSocketDevice
{
    public:

        QHostAddress    m_address;
        Q_UINT16        m_port;
        struct ip_mreq  m_imr;

    public:

        QBroadcastSocket( QString sAddress, Q_UINT16 nPort )
         : QSocketDevice( QSocketDevice::Datagram )
        {
            m_address.setAddress( sAddress );
            m_port = nPort;

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
