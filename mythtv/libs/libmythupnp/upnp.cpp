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
#include "upnptaskcache.h"
#include "multicast.h"
#include "broadcast.h"

#include <sys/utsname.h> 

//////////////////////////////////////////////////////////////////////////////
// Global/Class Static variables
//////////////////////////////////////////////////////////////////////////////

QString          UPnp::g_sPlatform;
UPnpDeviceDesc   UPnp::g_UPnpDeviceDesc;
TaskQueue       *UPnp::g_pTaskQueue     = NULL;
SSDP            *UPnp::g_pSSDP          = NULL;
SSDPCache        UPnp::g_SSDPCache;

Configuration   *UPnp::g_pConfig        = NULL;

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

UPnp::UPnp()
{
    VERBOSE( VB_UPNP, "UPnp - Constructor" );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnp::~UPnp()
{
    VERBOSE( VB_UPNP, "UPnp - Destructor" );
    CleanUp();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::SetConfiguration( Configuration *pConfig )
{
    if (g_pConfig)
        delete g_pConfig;

    g_pConfig = pConfig;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool UPnp::Initialize( int nServicePort, HttpServer *pHttpServer )
{
    VERBOSE(VB_UPNP, QString("UPnp::Initialize - Begin"));

    if (g_pConfig == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString( "UPnp::Initialize - Must call SetConfiguration." ));
        return false;
    }

    if ((m_pHttpServer = pHttpServer) == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString( "UPnp::Initialize - Invalid Parameter (pHttpServer == NULL)" ));
        return false;
    }

    m_nServicePort = nServicePort;

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

    VERBOSE(VB_UPNP, QString( "UPnp::Initialize - Starting TaskQueue" ));

    g_pTaskQueue = new TaskQueue();
    g_pTaskQueue->start();

    // ----------------------------------------------------------------------
    // Register any HttpServerExtensions
    // ----------------------------------------------------------------------

    m_pHttpServer->RegisterExtension( new SSDPExtension( m_nServicePort ));

    // ----------------------------------------------------------------------
    // Add Task to keep SSDPCache purged of stale entries.  
    // ----------------------------------------------------------------------

    UPnp::g_pTaskQueue->AddTask( new SSDPCacheTask() );

    // ----------------------------------------------------------------------
    // Create the SSDP (Upnp Discovery) Thread.
    // ----------------------------------------------------------------------

    VERBOSE(VB_UPNP, QString(  "UPnp::Initialize - Creating SSDP Thread" ));

    g_pSSDP = new SSDP( m_nServicePort );

    VERBOSE(VB_UPNP, QString( "UPnp::Initialize - End" ));

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Delay startup of Discovery Threads until all Extensions are registered.
//////////////////////////////////////////////////////////////////////////////

void UPnp::Start()
{
    if (g_pSSDP != NULL)
    {
        VERBOSE(VB_UPNP, QString(  "UPnp::UPnp:Starting SSDP Thread (Multicast)" ));
        g_pSSDP->start();
        g_pSSDP->EnableNotifications();
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::CleanUp( void )
{

    if (g_pSSDP)
    {
        g_pSSDP->DisableNotifications();
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

    if (g_pConfig)
    {
        delete g_pConfig;
        g_pConfig = NULL;
    }

} 

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

SSDPCacheEntries *UPnp::Find( const QString &sURI )
{
    return g_SSDPCache.Find( sURI );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
DeviceLocation *UPnp::Find( const QString &sURI, const QString &sUSN )
{
    return g_SSDPCache.Find( sURI, sUSN );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc *UPnp::GetDeviceDesc( QString &sURL, bool bInQtThread )
{
    return UPnpDeviceDesc::Retrieve( sURL, bInQtThread );
}

