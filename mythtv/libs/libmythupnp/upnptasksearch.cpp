//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasksearch.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to handle Discovery Responses
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <compat.h>
#include <stdlib.h>

#include <QStringList>
#include <QFile>
#include <QDateTime>

#include "upnp.h"
#include "upnptasksearch.h"
#include "mythversion.h"
#include "compat.h"
#include "mythdate.h"

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

UPnpSearchTask::UPnpSearchTask( int          nServicePort, 
                                QHostAddress peerAddress,
                                int          nPeerPort,  
                                QString      sST, 
                                QString      sUDN ) :
    Task("UPnpSearchTask")
{
    m_PeerAddress = peerAddress;
    m_nPeerPort   = nPeerPort;
    m_sST         = sST;
    m_sUDN        = sUDN;
    m_nServicePort= nServicePort;
    m_nMaxAge     = UPnp::GetConfiguration()->GetValue( "UPnP/SSDP/MaxAge" , 3600 );

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

void UPnpSearchTask::SendMsg( MSocketDevice  *pSocket,
                              QString         sST,
                              QString         sUDN )
{
    QString sUSN;

    if (( sUDN.length() > 0) && ( sUDN != sST ))
        sUSN = sUDN + "::" + sST;
    else
        sUSN = sST;

    QString sDate = MythDate::current().toString( "d MMM yyyy hh:mm:ss" );

    QString sData = QString ( "CACHE-CONTROL: max-age=%1\r\n"
                              "DATE: %2\r\n"
                              "EXT:\r\n"
                              "Server: %3\r\n"
                              "ST: %4\r\n"
                              "USN: %5\r\n"
                              "Content-Length: 0\r\n\r\n" )
                              .arg( m_nMaxAge    )
                              .arg( sDate )
                              .arg( HttpServer::GetServerVersion() )
                              .arg( sST )
                              .arg( sUSN );

#if 0
    LOG(VB_UPNP, LOG_DEBUG, QString("UPnpSearchTask::SendMsg : %1 : %2 ")
                        .arg(sST) .arg(sUSN));

    LOG(VB_UPNP, LOG_DEBUG,
        QString("UPnpSearchTask::SendMsg    m_PeerAddress = %1 Port=%2")
                        .arg(m_PeerAddress.toString()) .arg(m_nPeerPort));
#endif

    for ( QStringList::Iterator it  = m_addressList.begin(); 
                                it != m_addressList.end(); 
                              ++it ) 
    {
        QString ipaddress = *it;

        // If this looks like an IPv6 address, then enclose it in []'s
        if (ipaddress.contains(":"))
            ipaddress = "[" + ipaddress + "]";

        QString sHeader = QString ( "HTTP/1.1 200 OK\r\n"
                                    "LOCATION: http://%1:%2/getDeviceDesc\r\n" )
                            .arg( ipaddress )
                            .arg( m_nServicePort);


        QString  sPacket  = sHeader + sData;
        QByteArray scPacket = sPacket.toUtf8();

        // ------------------------------------------------------------------
        // Send Packet to UDP Socket (Send same packet twice)
        // ------------------------------------------------------------------

        pSocket->writeBlock( scPacket, scPacket.length(), m_PeerAddress,
                             m_nPeerPort );
        usleep( random() % 250000 );
        pSocket->writeBlock( scPacket, scPacket.length(), m_PeerAddress,
                             m_nPeerPort );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpSearchTask::Execute( TaskQueue * /*pQueue*/ )
{
    MSocketDevice *pSocket = new MSocketDevice( MSocketDevice::Datagram );

    // ----------------------------------------------------------------------
    // Refresh IP Address List in case of changes
    // ----------------------------------------------------------------------

    m_addressList = UPnp::g_IPAddrList;

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

void UPnpSearchTask::ProcessDevice(
    MSocketDevice *pSocket, UPnpDevice *pDevice)
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

    UPnpServiceList::const_iterator sit = pDevice->m_listServices.begin();
    for (; sit != pDevice->m_listServices.end(); ++sit)
        SendMsg(pSocket, (*sit)->m_sServiceType, pDevice->GetUDN());

    // ----------------------------------------------------------------------
    // Process any Embedded Devices
    // ----------------------------------------------------------------------

    UPnpDeviceList::const_iterator dit = pDevice->m_listDevices.begin();
    for (; dit != pDevice->m_listDevices.end(); ++dit)
        ProcessDevice( pSocket, *dit);
}

