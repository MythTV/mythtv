//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasksearch.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to handle Discovery Responses
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <compat.h>
#include <stdlib.h>

#include <QStringList>
#include <QFile>
#include <QDateTime>

#include "upnp.h"
#include "upnptasksearch.h"
#include "compat.h"

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
                                QString      sUDN )
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

    QString sDate = QDateTime::currentDateTime().toString( "d MMM yyyy hh:mm:ss" );  

    QString sData = QString ( "CACHE-CONTROL: max-age=%1\r\n"
                              "DATE: %2\r\n"
                              "EXT:\r\n"
                              "Server: %3, UPnP/1.0, MythTV %4\r\n"
                              "ST: %5\r\n"
                              "USN: %6\r\n"
                              "Content-Length: 0\r\n\r\n" )
                              .arg( m_nMaxAge    )
                              .arg( sDate )
                              .arg( HttpServer::g_sPlatform )
                              .arg( MYTH_BINARY_VERSION )
                              .arg( sST )
                              .arg( sUSN );

//    VERBOSE(VB_UPNP, QString("UPnpSearchTask::SendMsg : %1 : %2 ")
//                        .arg( sST  )
//                        .arg( sUSN ));

//cout << "UPnpSearchTask::SendMsg    m_PeerAddress = " <<  m_PeerAddress.toString() << " Port=" << m_nPeerPort << endl;

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

        pSocket->writeBlock( scPacket, scPacket.length(), m_PeerAddress, m_nPeerPort );
        usleep( rand() % 250000 );
        pSocket->writeBlock( scPacket, scPacket.length(), m_PeerAddress, m_nPeerPort );
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

