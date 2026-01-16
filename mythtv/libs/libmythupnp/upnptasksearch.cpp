//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasksearch.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to handle Discovery Responses
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnptasksearch.h"

#include <chrono> // for milliseconds
using namespace std::chrono_literals;
#include <thread> // for sleep_for
#include <utility>

#include <QByteArray>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QPair>
#include <QString>
#include <QUdpSocket>

#include "libmythbase/configuration.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"

#include "libmythupnp/httpserver.h"
#include "libmythupnp/upnp.h"

UPnpSearchTask::UPnpSearchTask( int          nServicePort, 
                                const QHostAddress& peerAddress,
                                int          nPeerPort,  
                                QString      sST, 
                                QString      sUDN ) :
    Task("UPnpSearchTask"),
    m_nServicePort(nServicePort),
    m_nMaxAge(XmlConfiguration().GetDuration<std::chrono::seconds>("UPnP/SSDP/MaxAge" , 1h)),
    m_peerAddress(peerAddress),
    m_nPeerPort(nPeerPort),
    m_sST(std::move(sST)),
    m_sUDN(std::move(sUDN))
{
}

void UPnpSearchTask::SendMsg(QUdpSocket& socket, const QString& sST, const QString& sUDN)
{
    QString uniqueServiceName = sST;
    if (( sUDN.length() > 0) && ( sUDN != sST ))
        uniqueServiceName = sUDN + "::" + uniqueServiceName;

    QByteArray data = QString("CACHE-CONTROL: max-age=%1\r\n"
                              "DATE: %2\r\n"
                              "EXT:\r\n"
                              "Server: %3\r\n"
                              "ST: %4\r\n"
                              "USN: %5\r\n"
                              "Content-Length: 0\r\n\r\n")
                        .arg(QString::number(m_nMaxAge.count()),
                             MythDate::current().toString("d MMM yyyy hh:mm:ss"),
                             HttpServer::GetServerVersion(),
                             sST,
                             uniqueServiceName
                             ).toUtf8();

    LOG(VB_UPNP, LOG_DEBUG, QString("UPnpSearchTask::SendMsg ST: %1 USN: %2; m_peerAddress = %3 Port=%4")
                        .arg(sST, uniqueServiceName, m_peerAddress.toString(), QString::number(m_nPeerPort))
        );

    // TODO: When we add dynamic handling of interfaces the information
    // for each address on the system should be available from the
    // "central" location on request.
    //
    // loop through all available interfaces
    QList<QNetworkInterface> IFs = QNetworkInterface::allInterfaces();
    for (const auto & qni : std::as_const(IFs))
    {
        QList<QNetworkAddressEntry> netAddressList = qni.addressEntries();
        for (const auto & netAddr : std::as_const(netAddressList))
        {
            QString ip_subnet = QString("%1/%2").arg(netAddr.ip().toString()).arg(netAddr.prefixLength());
            QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(ip_subnet);
            if (m_peerAddress.isInSubnet(subnet)) {
                LOG(VB_UPNP, LOG_DEBUG, QString("UPnpSearchTask::SendMsg : IP: [%1], Found network [%2], relevant to peer [%3]")
                    .arg(netAddr.ip().toString(), subnet.first.toString(), m_peerAddress.toString()));

                QString ipaddress;
                QHostAddress ip = netAddr.ip();
                // Descope the Link Local address. The scope is only valid
                // on the server sending the announcement, not the clients
                // that receive it
                ip.setScopeId(QString());

                // If this looks like an IPv6 address, then enclose it in []'s
                if (ip.protocol() == QAbstractSocket::IPv6Protocol)
                    ipaddress = "[" + ip.toString() + "]";
                else
                    ipaddress = ip.toString();

                QByteArray datagram =
                    QString("HTTP/1.1 200 OK\r\n"
                            "LOCATION: http://%1:%2/getDeviceDesc\r\n")
                        .arg(ipaddress, QString::number(m_nServicePort)).toUtf8()
                    + data;

                LOG(VB_UPNP, LOG_DEBUG,
                    QString("Sending SSDP search reply datagram to %1:%2\n%3")
                    .arg(m_peerAddress.toString(), QString::number(m_nPeerPort),
                         QString::fromUtf8(datagram))
                    );
                // Send Packet to UDP Socket (Send same packet twice)
                socket.writeDatagram(datagram, m_peerAddress, m_nPeerPort);

                std::this_thread::sleep_for(std::chrono::milliseconds(MythRandom(0, 250)));

                socket.writeDatagram(datagram, m_peerAddress, m_nPeerPort);
            }
        }
    }
}

void UPnpSearchTask::Execute( TaskQueue * /*pQueue*/ )
{
    QUdpSocket socket;

    // Check to see if this is a rootdevice or all request.
    if ((m_sST == "upnp:rootdevice") || (m_sST == "ssdp:all" ))
    {
        SendMsg(socket, "upnp:rootdevice", UPnp::g_UPnpDeviceDesc.m_rootDevice.GetUDN());

        if (m_sST == "ssdp:all")
            ProcessDevice(socket, UPnp::g_UPnpDeviceDesc.m_rootDevice);
    }
    else
    {
        // Send Device/Service specific response.
        SendMsg(socket, m_sST, m_sUDN);
    }
}

void UPnpSearchTask::ProcessDevice(QUdpSocket& socket, const UPnpDevice& device)
{
    // Loop for each device and send the 2 required messages
    // -=>TODO: We need to add support to only notify 
    //          Version 1 of a service.
    SendMsg(socket, device.GetUDN(), "");
    SendMsg(socket, device.m_sDeviceType, device.GetUDN());
    // Loop for each service in this device and send the 1 required message
    for (const auto* service : std::as_const(device.m_listServices))
    {
        SendMsg(socket, service->m_sServiceType, device.GetUDN());
    }

    // Process any Embedded Devices
    for (const auto* embedded_device : std::as_const(device.m_listDevices))
    {
        ProcessDevice(socket, *embedded_device);
    }
}
