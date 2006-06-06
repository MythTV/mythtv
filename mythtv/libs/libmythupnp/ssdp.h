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

#define SSDP_GROUP  "239.255.255.250"
#define SSDP_PORT   1900

typedef enum 
{
    SSDPM_Unknown         = 0,
    SSDPM_GetDeviceDesc   = 1,
    SSDPM_GetCDSDesc      = 2,
    SSDPM_GetCMGRDesc     = 3

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
/////////////////////////////////////////////////////////////////////////////
//
// SSDPThread Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SSDP : public QThread
{
    private:

        bool    m_bTermRequested;
        QMutex  m_lock;

    protected:

        bool    ProcessSearchRequest( HTTPRequest *pRequest,
                                      QHostAddress  peerAddress,
                                      Q_UINT16      peerPort );
        bool    IsTermRequested     ();

    public:

                     SSDP   ();
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

    public:
                 SSDPExtension( );
        virtual ~SSDPExtension( );

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
