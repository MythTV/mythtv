//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasksearch.cpp
//                                                                            
// Purpose - UPnp Task to handle Discovery Responses
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpdevice.h"
#include "ssdp.h"
#include "upnptasksearch.h"

#include <unistd.h>
#include <qstringlist.h>
#include <quuid.h> 
#include <qdom.h> 
#include <qfile.h>
#include <sys/time.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpSearchTask Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpSearchTask::UPnpSearchTask( QHostAddress peerAddress,
                                int          nPeerPort,  
                                QString      sST, 
                                QString      sUDN )
{
    m_PeerAddress = peerAddress;
    m_nPeerPort   = nPeerPort;
    m_sST         = sST;
    m_sUDN        = sUDN;
} 

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpSearchTask::~UPnpSearchTask()  
{ 
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpSearchTask::SendMsg( QSocketDevice  *pSocket, 
                              QString         sST,
                              QString         sUDN )
{
    QString sUSN;

    if ( sUDN.length() > 0)
        sUSN = sUDN + "::" + sST;
    else
        sUSN = sST;

    QString sDate = QDateTime::currentDateTime().toString( "d MMM yyyy hh:mm:ss" );  

    QString sData = QString ( "CACHE-CONTROL: max-age=%1\r\n"
                              "DATE: %2\r\n"
                              "EXT:\r\n"
                              "SERVER: %3, UPnP/1.0, MythTv %4\r\n"
                              "ST: %5\r\n"
                              "USN: %6\r\n"
                              "Content-Length: 0\r\n\r\n" )
                              .arg( m_nMaxAge    )
                              .arg( sDate )
                              .arg( UPnp::g_sPlatform )
                              .arg( MYTH_BINARY_VERSION )
                              .arg( sST )
                              .arg( sUSN );

    for ( QStringList::Iterator it  = m_addressList.begin(); 
                                it != m_addressList.end(); 
                              ++it ) 
    {
        QString sHeader = QString ( "HTTP/1.1 200 OK\r\n"
                                    "LOCATION: http://%1:%2/getDeviceDesc\r\n" )
                            .arg( *it )
                            .arg( m_nStatusPort);


        QString  sPacket  = sHeader + sData;
        QCString scPacket = sPacket.utf8();

        // ------------------------------------------------------------------
        // Send Packet to UDP Socket (Send same packet twice)
        // ------------------------------------------------------------------

        pSocket->writeBlock( scPacket, scPacket.length(), m_PeerAddress, m_nPeerPort );
        usleep( 500000 );
        pSocket->writeBlock( scPacket, scPacket.length(), m_PeerAddress, m_nPeerPort );
    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpSearchTask::Execute( TaskQueue * /*pQueue*/ )
{
    // ----------------------------------------------------------------------
    // Reload in case they have changed.
    // ----------------------------------------------------------------------

    m_nStatusPort = gContext->GetNumSetting("BackendStatusPort", 6544 );
    m_nMaxAge     = gContext->GetNumSetting("upnpMaxAge"       , 3600 );

    QSocketDevice *pSocket = new QSocketDevice( QSocketDevice::Datagram );

    // ----------------------------------------------------------------------
    // Refresh IP Address List in case of changes
    // ----------------------------------------------------------------------

    GetIPAddressList( m_addressList );

    // ----------------------------------------------------------------------
    // Check to see if this is a rootdevice or all request.
    // ----------------------------------------------------------------------

    UPnpDevice &device = UPnp::g_UPnpDeviceDesc.m_rootDevice;

    if ((m_sST == "upnp:rootdevice") || (m_sST == "ssdp:all" ))
    {
        SendMsg( pSocket, "upnp:rootdevice", device.GetUDN() );

        if (m_sST == "ssdp:all")
            ProcessDevice( pSocket, &device );
    }
    else
    {
        // ------------------------------------------------------------------
        // Send Device/Service specific response.
        // ------------------------------------------------------------------

        SendMsg( pSocket, m_sST, m_sUDN );
    }

    delete pSocket;
    pSocket = NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpSearchTask::ProcessDevice( QSocketDevice *pSocket, UPnpDevice *pDevice )
{
    // ----------------------------------------------------------------------
    // Loop for each device and send the 2 required messages
    //
    // -=>TODO: We need to add support to only notify 
    //          Version 1 of a service.
    // ----------------------------------------------------------------------

    SendMsg( pSocket, pDevice->GetUDN(), "" );
    SendMsg( pSocket, pDevice->m_sDeviceType, pDevice->GetUDN() );
        
    // ------------------------------------------------------------------
    // Loop for each service in this device and send the 1 required message
    // ------------------------------------------------------------------

    for ( UPnpService *pService  = pDevice->m_listServices.first(); 
                       pService != NULL;
                       pService  = pDevice->m_listServices.next() )
    {
        SendMsg( pSocket, pService->m_sServiceType, pDevice->GetUDN() );
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

