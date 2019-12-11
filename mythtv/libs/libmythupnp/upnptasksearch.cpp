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

#include <chrono> // for milliseconds
#include <cstdlib>
#include <thread> // for sleep_for

#include <QDateTime>
#include <QFile>
#include <QStringList>
#include <utility>

#include "upnp.h"
#include "upnptasksearch.h"
#include "mythversion.h"
#include "compat.h"
#include "mythdate.h"

static QPair<QHostAddress, int> kLinkLocal6 =
                            QHostAddress::parseSubnet("fe80::/10");

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
    m_peerAddress = std::move(peerAddress);
    m_nPeerPort   = nPeerPort;
    m_sST         = std::move(sST);
    m_sUDN        = std::move(sUDN);
    m_nServicePort= nServicePort;
    m_nMaxAge     = UPnp::GetConfiguration()->GetValue( "UPnP/SSDP/MaxAge" , 3600 );

} 

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpSearchTask::SendMsg( MSocketDevice  *pSocket,
                              const QString&  sST,
                              const QString&  sUDN )
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
        QString("UPnpSearchTask::SendMsg    m_peerAddress = %1 Port=%2")
                        .arg(m_peerAddress.toString()) .arg(m_nPeerPort));
#endif

    for ( QList<QHostAddress>::Iterator it  = m_addressList.begin();
                                it != m_addressList.end(); 
                              ++it ) 
    {
        QString ipaddress;

        // Avoid announcing the localhost address
        if (*it == QHostAddress::LocalHost ||
            *it == QHostAddress::LocalHostIPv6 ||
            *it == QHostAddress::AnyIPv4 ||
            *it == QHostAddress::AnyIPv6)
            continue;

        QHostAddress ip = *it;
        // Descope the Link Local address. The scope is only valid
        // on the server sending the announcement, not the clients
        // that receive it
        ip.setScopeId(QString());

        // If this looks like an IPv6 address, then enclose it in []'s
        if (ip.protocol() == QAbstractSocket::IPv6Protocol)
            ipaddress = "[" + ip.toString() + "]";
        else
            ipaddress = ip.toString();

        QString sHeader = QString ( "HTTP/1.1 200 OK\r\n"
                                    "LOCATION: http://%1:%2/getDeviceDesc\r\n" )
                            .arg( ipaddress )
                            .arg( m_nServicePort);


        QString  sPacket  = sHeader + sData;
        QByteArray scPacket = sPacket.toUtf8();

        // ------------------------------------------------------------------
        // Send Packet to UDP Socket (Send same packet twice)
        // ------------------------------------------------------------------

        pSocket->writeBlock( scPacket, scPacket.length(), m_peerAddress,
                             m_nPeerPort );
        std::this_thread::sleep_for( std::chrono::milliseconds( random() % 250 ));
        pSocket->writeBlock( scPacket, scPacket.length(), m_peerAddress,
                             m_nPeerPort );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpSearchTask::Execute( TaskQueue * /*pQueue*/ )
{
    auto *pSocket = new MSocketDevice( MSocketDevice::Datagram );

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
    pSocket = nullptr;
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

