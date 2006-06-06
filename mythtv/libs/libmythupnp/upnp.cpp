//////////////////////////////////////////////////////////////////////////////
// Program Name: upnp.cpp
//                                                                            
// Purpose - UPnp Main Class
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnp.h"

#include <quuid.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h> 
#include <sys/time.h>

#include "upnpcds.h"

//////////////////////////////////////////////////////////////////////////////
// Global/Class Static variables
//////////////////////////////////////////////////////////////////////////////

QString          UPnp::g_sPlatform;
UPnpDeviceDesc   UPnp::g_UPnpDeviceDesc;
TaskQueue       *UPnp::g_pTaskQueue;
SSDP            *UPnp::g_pSSDP;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnp Class implementaion
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnp::UPnp( bool /*bIsMaster */, HttpServer *pHttpServer ) 
{
    VERBOSE(VB_UPNP, QString("UPnp::UPnp:Begin"));

    if ((m_pHttpServer = pHttpServer) == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString( "UPnp::UPnp:Invalid Parameter (pHttpServer == NULL)" ));
        return;
    }

    // ----------------------------------------------------------------------
    // Build Platform String
    // ----------------------------------------------------------------------

    struct utsname uname_info;

    uname( &uname_info );

    g_sPlatform = QString( "%1 %2" ).arg( uname_info.sysname )
                                    .arg( uname_info.release );

    // ----------------------------------------------------------------------
    // Initialize & Start the global Task Queue Processing Thread
    // ----------------------------------------------------------------------

    VERBOSE(VB_UPNP, QString( "UPnp::UPnp:Starting TaskQueue" ));

    g_pTaskQueue = new TaskQueue();
    g_pTaskQueue->start();

    // ----------------------------------------------------------------------
    // Make sure our device Description is loaded.
    // ----------------------------------------------------------------------

    VERBOSE(VB_UPNP, QString( "UPnp::UPnp:Loading UPnp Description" ));

    g_UPnpDeviceDesc.Load();

    // ----------------------------------------------------------------------
    // Register any HttpServerExtensions
    // ----------------------------------------------------------------------

    m_pHttpServer->RegisterExtension(              new SSDPExtension());
    m_pHttpServer->RegisterExtension( m_pUPnpCDS = new UPnpCDS      ());

    // ----------------------------------------------------------------------
    // Start up the SSDP (Upnp Discovery) Thread.
    // ----------------------------------------------------------------------

    VERBOSE(VB_UPNP, QString( "UPnp::UPnp:Starting SSDP Thread" ));

    g_pSSDP = new SSDP();
    g_pSSDP->start();

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------

    VERBOSE(VB_UPNP, QString( "UPnp::UPnp:Adding Context Listener" ));

    gContext->addListener( this );

    VERBOSE(VB_UPNP, QString( "UPnp::UPnp:End" ));
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnp::~UPnp()
{
	CleanUp();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::CleanUp( void )
{

    // -=>TODO: Need to check to see if calling this more than once is ok.

    gContext->removeListener(this);

    if (g_pSSDP)
    {
        g_pSSDP->RequestTerminate();

        delete g_pSSDP;
        g_pSSDP = NULL;
    }

    // ----------------------------------------------------------------------
    // Clear the Task Queue & terminate Thread
    // ----------------------------------------------------------------------

    if (g_pTaskQueue)
    {
        g_pTaskQueue->Clear();
        g_pTaskQueue->RequestTerminate();

        delete g_pTaskQueue;
        g_pTaskQueue = NULL;
    }

} 
    
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::customEvent( QCustomEvent *e )
{
    if (MythEvent::Type(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        //-=>TODO: Need to handle events to notify clients of changes
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::RegisterExtension( UPnpCDSExtension *pExtension )
{
    m_pUPnpCDS->RegisterExtension( pExtension );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::UnregisterExtension( UPnpCDSExtension *pExtension )
{
    m_pUPnpCDS->UnregisterExtension( pExtension );
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// Global Helper Methods... 
// 
// -=>TODO: Should these functions go someplace else?
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString LookupUDN( QString sDeviceType )
{
    sDeviceType = "upnp:UDN:" + sDeviceType;
    
    QString sUDN = gContext->GetSetting( sDeviceType, "" );

    if ( sUDN.length() == 0) 
    {
        sUDN = QUuid::createUuid().toString();

        sUDN = sUDN.mid( 1, sUDN.length() - 2);

        gContext->SaveSetting   ( sDeviceType, sUDN );
    }

    return( sUDN );
}

/////////////////////////////////////////////////////////////////////////////

long GetIPAddressList( QStringList &sStrList )
{
    sStrList.clear();

    QSocketDevice socket( QSocketDevice::Datagram );

    struct ifreq  ifReqs[ 16 ];
    struct ifreq  ifReq;
    struct ifconf ifConf;

    // ----------------------------------------------------------------------
    // Get Configuration information...
    // ----------------------------------------------------------------------

    ifConf.ifc_len           = sizeof( struct ifreq ) * sizeof( ifReqs );
    ifConf.ifc_ifcu.ifcu_req = ifReqs;

    if ( ioctl( socket.socket(), SIOCGIFCONF, &ifConf ) < 0)
        return( 0 );

    long nCount = ifConf.ifc_len / sizeof( struct ifreq );

    // ----------------------------------------------------------------------
    // Loop through looking for IP addresses.
    // ----------------------------------------------------------------------

    for (long nIdx = 0; nIdx < nCount; nIdx++ )
    {
        // ------------------------------------------------------------------
        // Is this an interface we want?
        // ------------------------------------------------------------------

        strcpy ( ifReq.ifr_name, ifReqs[ nIdx ].ifr_name );

        if (ioctl ( socket.socket(), SIOCGIFFLAGS, &ifReq ) < 0)
            continue;

        // ------------------------------------------------------------------
        // Skip loopback and down interfaces
        // ------------------------------------------------------------------

        if ((ifReq.ifr_flags & IFF_LOOPBACK) || (!(ifReq.ifr_flags & IFF_UP)))
            continue;

        if ( ifReqs[ nIdx ].ifr_addr.sa_family == AF_INET)
        {
            struct sockaddr_in addr;

            // --------------------------------------------------------------
            // Get a pointer to the address
            // --------------------------------------------------------------

            memcpy (&addr, &(ifReqs[ nIdx ].ifr_addr), sizeof( ifReqs[ nIdx ].ifr_addr ));

            if (addr.sin_addr.s_addr != htonl( INADDR_LOOPBACK ))
            {
                QHostAddress address( htonl( addr.sin_addr.s_addr ));

                sStrList.append( address.toString() ); 
            }
        }
    }

    return( sStrList.count() );
}

/////////////////////////////////////////////////////////////////////////////
//           
/////////////////////////////////////////////////////////////////////////////

bool operator< ( TaskTime t1, TaskTime t2 )
{
    if ( (t1.tv_sec  < t2.tv_sec) or 
        ((t1.tv_sec == t2.tv_sec) && (t1.tv_usec < t2.tv_usec)))
    {
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//           
/////////////////////////////////////////////////////////////////////////////

bool operator== ( TaskTime t1, TaskTime t2 )
{
    if ((t1.tv_sec == t2.tv_sec) && (t1.tv_usec == t2.tv_usec))
        return true;

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//           
/////////////////////////////////////////////////////////////////////////////

void AddMicroSecToTaskTime( TaskTime &t, __suseconds_t uSecs )
{
    uSecs += t.tv_usec;

    t.tv_sec  += (uSecs / 1000000);
    t.tv_usec  = (uSecs % 1000000);
}

