//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasknotify.cpp
//                                                                            
// Purpose - UPnp Task to send Notification messages
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpdevice.h"
#include "ssdp.h"
#include "upnptasknotify.h"

#include <unistd.h>
#include <qstringlist.h>
#include <quuid.h> 
#include <qdom.h> 
#include <qfile.h>
#include <sys/time.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpNotifyTask Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpNotifyTask::UPnpNotifyTask( ) : m_nStatusPort( 0 ),
                                    m_nMaxAge( 0 ), 
                                    m_eNTS( NTS_alive )
{
} 

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpNotifyTask::~UPnpNotifyTask()  
{ 
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpNotifyTask::SendNotifyMsg( QMulticastSocket *pSocket, 
                                    QString           sNT,
                                    QString           sUDN )
{
    QString sUSN;

    if ( sUDN.length() > 0)
        sUSN = sUDN + "::" + sNT;
    else
        sUSN = sNT;

    QString sData = QString ( "SERVER: %1, UPnP/1.0, MythTv %2\r\n"
                              "NTS: %3\r\n"
                              "NT: %4\r\n"
                              "USN: %5\r\n"
                              "CACHE-CONTROL: max-age=%6\r\n"
                              "Content-Length: 0\r\n\r\n" )
                            .arg( UPnp::g_sPlatform )
                            .arg( MYTH_BINARY_VERSION )
                            .arg( GetNTSString()    )
                            .arg( sNT          )
                            .arg( sUSN         )
                            .arg( m_nMaxAge    );

    for ( QStringList::Iterator it  = m_addressList.begin(); 
                                it != m_addressList.end(); 
                              ++it ) 
    {
        QString sHeader = QString ( "NOTIFY * HTTP/1.1\r\n"
                                    "HOST: 239.255.255.250:1900\r\n"
                                    "LOCATION: http://%1:%2/getDeviceDesc\r\n" )
                            .arg( *it )
                            .arg( m_nStatusPort);

        QString  sPacket  = sHeader + sData;
        QCString scPacket = sPacket.utf8();

        // ------------------------------------------------------------------
        // Send Packet to Multicast Socket (Send same packet twice)
        // ------------------------------------------------------------------

        pSocket->writeBlock( scPacket, scPacket.length(), pSocket->address(), pSocket->port() );
        usleep( 500000 );
        pSocket->writeBlock( scPacket, scPacket.length(), pSocket->address(), pSocket->port() );
    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpNotifyTask::Execute( TaskQueue *pQueue )
{
    // ----------------------------------------------------------------------
    // Reload in case they have changed.
    // ----------------------------------------------------------------------

    m_nStatusPort = gContext->GetNumSetting("BackendStatusPort", 6544 );
    m_nMaxAge     = gContext->GetNumSetting("upnpMaxAge"       , 3600 );

    QMulticastSocket *pSocket = new QMulticastSocket( SSDP_GROUP, SSDP_PORT );

    // ----------------------------------------------------------------------
    // Refresh IP Address List in case of changes
    // ----------------------------------------------------------------------

    GetIPAddressList( m_addressList );

    // ----------------------------------------------------------------------
    // Must send rootdevice Notification for first device.
    // ----------------------------------------------------------------------

    UPnpDevice &device = UPnp::g_UPnpDeviceDesc.m_rootDevice;

    SendNotifyMsg( pSocket, "upnp:rootdevice", device.GetUDN() );

    // ----------------------------------------------------------------------
    // Process rest of notifications
    // ----------------------------------------------------------------------

    ProcessDevice( pSocket, &device );

    // ----------------------------------------------------------------------
    // Clean up and reshedule task if needed (timeout = m_nMaxAge / 2).
    // ----------------------------------------------------------------------

    delete pSocket;
    pSocket = NULL;

    m_mutex.lock();

    if (m_eNTS == NTS_alive) 
        pQueue->AddTask( (m_nMaxAge / 2) * 1000, (Task *)this  );

    m_mutex.unlock();

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpNotifyTask::ProcessDevice( QMulticastSocket *pSocket, UPnpDevice *pDevice )
{
    // ----------------------------------------------------------------------
    // Loop for each device and send the 2 required messages
    //
    // -=>TODO: Need to add support to only notify 
    //          Version 1 of a service.
    // ----------------------------------------------------------------------

    SendNotifyMsg( pSocket, pDevice->GetUDN(), "" );
    SendNotifyMsg( pSocket, pDevice->m_sDeviceType, pDevice->GetUDN() );
        
    // ------------------------------------------------------------------
    // Loop for each service in this device and send the 1 required message
    // ------------------------------------------------------------------

    for ( UPnpService *pService  = pDevice->m_listServices.first(); 
                       pService != NULL;
                       pService  = pDevice->m_listServices.next() )
    {
        SendNotifyMsg( pSocket, pService->m_sServiceType, pDevice->GetUDN() );
    }

    // ----------------------------------------------------------------------
    // Process any Embedded Devices
    // ----------------------------------------------------------------------

    for ( UPnpDevice *pEmbeddedDevice  = pDevice->m_listDevices.first(); 
                      pEmbeddedDevice != NULL;
                      pEmbeddedDevice  = pDevice->m_listDevices.next() )
    {
        ProcessDevice( pSocket, pEmbeddedDevice );
    }
}
