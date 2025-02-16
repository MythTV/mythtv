//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasknotify.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to send Notification messages
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnptasknotify.h"

// ANSI C headers
#include <cstdlib>

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt headers
#include <QStringList>
#include <QUuid> 
#include <QFile>

// MythTV headers
#include "libmythbase/configuration.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"
#include "libmythbase/mythversion.h"

#include "httpserver.h"
#include "mmulticastsocketdevice.h"
#include "ssdp.h"
#include "upnp.h"

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

UPnpNotifyTask::UPnpNotifyTask( int nServicePort ) :
    Task("UPnpNotifyTask"),
    m_nServicePort(nServicePort)
{
    m_nMaxAge      = XmlConfiguration().GetDuration<std::chrono::seconds>("UPnP/SSDP/MaxAge" , 1h);
} 

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpNotifyTask::SendNotifyMsg( MSocketDevice *pSocket,
                                    const QString& sNT,
                                    const QString& sUDN )
{
    QString sUSN;

    if ( sUDN.length() > 0)
        sUSN = sUDN + "::" + sNT;
    else
        sUSN = sNT;

    QString sData = QString ( "Server: %1\r\n"
                              "NTS: %3\r\n"
                              "NT: %4\r\n"
                              "USN: %5\r\n"
                              "CACHE-CONTROL: max-age=%6\r\n"
                              "Content-Length: 0\r\n\r\n" )
                            .arg( HttpServer::GetServerVersion(),
                                  GetNTSString(),
                                  sNT,
                                  sUSN,
                                  QString::number(m_nMaxAge.count()));

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpNotifyTask::SendNotifyMsg : %1:%2 : %3 : %4")
            .arg(pSocket->address().toString(), QString::number(pSocket->port()),
                 sNT, sUSN));

    QMutexLocker qml(&m_mutex); // for addressList

    // -------------------------------------------------------------------
    // Refresh IP Address List in case of changes
    // -------------------------------------------------------------------

    QList<QHostAddress> addressList = UPnp::g_IPAddrList;

    for (const auto & addr : std::as_const(addressList))
    {
        if (addr.toString().isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR,
                "UPnpNotifyTask::SendNotifyMsg - NULL in address list");
            continue;
        }

        QHostAddress ip = addr;
        // Descope the Link Local address. The scope is only valid
        // on the server sending the announcement, not the clients
        // that receive it
        ip.setScopeId(QString());

        QString ipaddress = ip.toString();

        // If this looks like an IPv6 address, then enclose it in []'s
        if (ipaddress.contains(":"))
            ipaddress = "[" + ipaddress + "]";

        QString sHeader = QString("NOTIFY * HTTP/1.1\r\n"
                                  "HOST: %1:%2\r\n"    
                                  "LOCATION: http://%3:%4/getDeviceDesc\r\n")
                    .arg(pSocket->address().toString()) .arg(pSocket->port())
                    .arg(ipaddress) .arg(m_nServicePort);

        QString  sPacket  = sHeader + sData;
        QByteArray scPacket = sPacket.toUtf8();

        // ---------------------------------------------------------------
        // Send Packet to Socket (Send same packet twice)
        // ---------------------------------------------------------------

        pSocket->writeBlock( scPacket, scPacket.length(),
                             pSocket->address(), pSocket->port() );
        if (m_eNTS != NTS_byebye)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(MythRandom(0, 250)));

            pSocket->writeBlock( scPacket, scPacket.length(),
                                pSocket->address(), pSocket->port() );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpNotifyTask::Execute( TaskQueue *pQueue )
{
    MSocketDevice *pMulticast = new MMulticastSocketDevice(
        SSDP_GROUP, SSDP_PORT);

    // ----------------------------------------------------------------------
    // Must send rootdevice Notification for first device.
    // ----------------------------------------------------------------------

    UPnpDevice &device = UPnp::g_UPnpDeviceDesc.m_rootDevice;

    SendNotifyMsg( pMulticast, "upnp:rootdevice", device.GetUDN() );

    // ----------------------------------------------------------------------
    // Process rest of notifications
    // ----------------------------------------------------------------------

    ProcessDevice( pMulticast, &device );

    // ----------------------------------------------------------------------
    // Clean up and reshedule task if needed (timeout = m_nMaxAge / 2).
    // ----------------------------------------------------------------------

    delete pMulticast;

    pMulticast = nullptr;

    m_mutex.lock();

    if (m_eNTS == NTS_alive) 
        pQueue->AddTask( (m_nMaxAge / 2), (Task *)this  );

    m_mutex.unlock();

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpNotifyTask::ProcessDevice(
    MSocketDevice *pSocket, UPnpDevice *pDevice)
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

    for (auto sit = pDevice->m_listServices.cbegin(); sit != pDevice->m_listServices.cend(); ++sit)
        SendNotifyMsg( pSocket, (*sit)->m_sServiceType, pDevice->GetUDN() );

    // ----------------------------------------------------------------------
    // Process any Embedded Devices
    // ----------------------------------------------------------------------

    for (const auto & dev : std::as_const(pDevice->m_listDevices))
        ProcessDevice( pSocket, dev);
}
