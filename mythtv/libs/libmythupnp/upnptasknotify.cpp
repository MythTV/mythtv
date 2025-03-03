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

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt headers
#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QString>

// MythTV headers
#include "libmythbase/configuration.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"

#include "httpserver.h"
#include "ssdp.h"
#include "upnp.h"

UPnpNotifyTask::UPnpNotifyTask( int nServicePort ) :
    Task("UPnpNotifyTask"),
    m_nServicePort(nServicePort)
{
    m_nMaxAge      = XmlConfiguration().GetDuration<std::chrono::seconds>("UPnP/SSDP/MaxAge" , 1h);
} 

void UPnpNotifyTask::SendNotifyMsg(QUdpSocket& socket, const QString& sNT, const QString& sUDN)
{
    QString uniqueServiceName = sNT;
    if (sUDN.length() > 0)
        uniqueServiceName = sUDN + "::" + uniqueServiceName;

    QByteArray data = QString("Server: %1\r\n"
                              "NTS: %3\r\n"
                              "NT: %4\r\n"
                              "USN: %5\r\n"
                              "CACHE-CONTROL: max-age=%6\r\n"
                              "Content-Length: 0\r\n\r\n")
                        .arg(HttpServer::GetServerVersion(),
                             GetNTSString(),
                             sNT,
                             uniqueServiceName,
                             QString::number(m_nMaxAge.count())
                             ).toUtf8();

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpNotifyTask::SendNotifyMsg : %1:%2 : %3 : %4")
            .arg(SSDP_GROUP, QString::number(SSDP_PORT),
                 sNT, uniqueServiceName
                 )
        );

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

        QByteArray datagram =
            QString("NOTIFY * HTTP/1.1\r\n"
                    "HOST: %1:%2\r\n"
                    "LOCATION: http://%3:%4/getDeviceDesc\r\n")
                .arg(SSDP_GROUP, QString::number(SSDP_PORT),
                     ipaddress, QString::number(m_nServicePort)
                     ).toUtf8()
             + data;

        // Send Packet to Socket (Send same packet twice)
        socket.writeDatagram(datagram, QHostAddress(QString(SSDP_GROUP)), SSDP_PORT);
        if (m_eNTS != NTS_byebye)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(MythRandom(0, 250)));

            socket.writeDatagram(datagram, QHostAddress(QString(SSDP_GROUP)), SSDP_PORT);
        }
    }
}

void UPnpNotifyTask::Execute( TaskQueue *pQueue )
{
    QUdpSocket socket;
    socket.bind(QHostAddress(QHostAddress::AnyIPv4), 0); // required for setSocketOption()
    socket.setSocketOption(QAbstractSocket::MulticastTtlOption, 4);

    // Must send rootdevice Notification for first device.
    SendNotifyMsg(socket, "upnp:rootdevice", UPnp::g_UPnpDeviceDesc.m_rootDevice.GetUDN());
    // Process rest of notifications
    ProcessDevice(socket, UPnp::g_UPnpDeviceDesc.m_rootDevice);

    // reshedule task if needed (timeout = m_nMaxAge / 2).
    m_mutex.lock();
    if (m_eNTS == NTS_alive) 
        pQueue->AddTask( (m_nMaxAge / 2), (Task *)this  );

    m_mutex.unlock();
}

void UPnpNotifyTask::ProcessDevice(QUdpSocket& socket, const UPnpDevice& device)
{
    // Loop for each device and send the 2 required messages
    // -=>TODO: Need to add support to only notify 
    //          Version 1 of a service.
    SendNotifyMsg(socket, device.GetUDN(), "");
    SendNotifyMsg(socket, device.m_sDeviceType, device.GetUDN());
    // Loop for each service in this device and send the 1 required message
    for (const auto* service : std::as_const(device.m_listServices))
    {
        SendNotifyMsg(socket, service->m_sServiceType, device.GetUDN());
    }

    // Process any Embedded Devices
    for (const auto* embedded_device : std::as_const(device.m_listDevices))
    {
        ProcessDevice(socket, *embedded_device);
    }
}
