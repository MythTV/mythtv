//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdp.h
//                                                                            
// Purpose - SSDP Discovery Service Implmenetation
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __SSDP_H__
#define __SSDP_H__

#include <qthread.h>
#include <qsocketdevice.h>
#include <qfile.h>

#include "mythcontext.h"

#include "httpserver.h"
#include "taskqueue.h"

#include "ssdpcache.h"

#define SSDP_GROUP  "239.255.255.250"
#define SSDP_PORT   1900

typedef enum 
{
    SSDPM_Unknown         = 0,
    SSDPM_GetDeviceDesc   = 1,
    SSDPM_GetCDSDesc      = 2,
    SSDPM_GetCMGRDesc     = 3,
    SSDPM_GetMSRRDesc     = 4,
    SSDPM_GetMythProtoDesc= 5,
    SSDPM_GetDeviceList   = 6

} SSDPMethod;

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
                ttl = gContext->GetNumSetting( "upnpTTL", 4 );

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

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPThread Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SSDP : public QThread
{
    private:

        QSocketDevice  *m_pSSDPSocket; 
        bool            m_bEnableNotify;
        bool            m_bTermRequested;
        QMutex          m_lock;

    protected:

        bool    ProcessSearchRequest( HTTPRequest *pRequest,
                                      QHostAddress  peerAddress,
                                      Q_UINT16      peerPort );
        bool    ProcessNotify       ( HTTPRequest *pRequest );
        bool    IsTermRequested     ();

    public:

                     SSDP   ( QSocketDevice *pSocket, bool bEnableNotify = true);
        virtual void run    ();

                void RequestTerminate(void);

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPExtension Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SSDPExtension : public HttpServerExtension
{
    private:

        QString     m_sUPnpDescPath;

    private:

        SSDPMethod GetMethod( const QString &sURI );

        void       GetDeviceDesc( HTTPRequest *pRequest );
        void       GetFile      ( HTTPRequest *pRequest, QString sFileName );
        void       GetDeviceList( HTTPRequest *pRequest );

    public:
                 SSDPExtension( );
        virtual ~SSDPExtension( );

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
