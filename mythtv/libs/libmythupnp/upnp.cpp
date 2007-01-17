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

#include <sys/utsname.h> 

//////////////////////////////////////////////////////////////////////////////
// Global/Class Static variables
//////////////////////////////////////////////////////////////////////////////

QString          UPnp::g_sPlatform;
UPnpDeviceDesc   UPnp::g_UPnpDeviceDesc;
TaskQueue       *UPnp::g_pTaskQueue     = NULL;
SSDP            *UPnp::g_pSSDP          = NULL;
SSDP            *UPnp::g_pSSDPBroadcast = NULL;
SSDPCache        UPnp::g_SSDPCache;

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

UPnp::UPnp( HttpServer *pHttpServer ) 
{
    VERBOSE(VB_UPNP, QString("UPnp::Begin"));

    if ((m_pHttpServer = pHttpServer) == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString( "UPnp::UPnp:Invalid Parameter (pHttpServer == NULL)" ));
        return;
    }

    // ----------------------------------------------------------------------
    // Get Settings
    // ----------------------------------------------------------------------

    bool bBroadcastSupport = (bool)gContext->GetNumSetting("upnpSSDPBroadcast", 0 );

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

    VERBOSE(VB_UPNP, QString( "UPnp::Starting TaskQueue" ));

    g_pTaskQueue = new TaskQueue();
    g_pTaskQueue->start();

    // ----------------------------------------------------------------------
    // Register any HttpServerExtensions
    // ----------------------------------------------------------------------

    m_pHttpServer->RegisterExtension( new SSDPExtension());

    // ----------------------------------------------------------------------
    // Add Task to keep SSDPCache purged of stale entries.  
    // ----------------------------------------------------------------------

    UPnp::g_pTaskQueue->AddTask( new SSDPCacheTask() );

    // ----------------------------------------------------------------------
    // Create the SSDP (Upnp Discovery) Thread.
    // ----------------------------------------------------------------------

    VERBOSE(VB_UPNP, QString(  "UPnp::Creating SSDP Thread (Multicast)" ));

    g_pSSDP = new SSDP( new QMulticastSocket( SSDP_GROUP, SSDP_PORT ) );

    if (bBroadcastSupport)
    {
        VERBOSE(VB_UPNP, QString(  "UPnp::Creating SSDP Thread (Broadcast)" ));

        g_pSSDPBroadcast = new SSDP( new QBroadcastSocket( "255.255.255.255", SSDP_PORT ), false );
    }

    VERBOSE(VB_UPNP, QString( "UPnp::End" ));
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnp::~UPnp()
{
	CleanUp();
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
    }

    if (g_pSSDPBroadcast != NULL)
    {
        VERBOSE(VB_UPNP, QString(  "UPnp::UPnp:Starting SSDP Thread (Broadcast)" ));
        g_pSSDPBroadcast->start();
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::CleanUp( void )
{

    if (g_pSSDP)
    {
        g_pSSDP->RequestTerminate();

        delete g_pSSDP;
        g_pSSDP = NULL;
    }

    if (g_pSSDPBroadcast)
    {
        g_pSSDPBroadcast->RequestTerminate();

        delete g_pSSDPBroadcast;
        g_pSSDPBroadcast = NULL;
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

