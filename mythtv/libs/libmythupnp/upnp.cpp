//////////////////////////////////////////////////////////////////////////////
// Program Name: upnp.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Main Class
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnp.h"

#include <QNetworkInterface>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/serverpool.h"

#include "ssdp.h"
#include "ssdpextension.h"

//////////////////////////////////////////////////////////////////////////////
// Global/Class Static variables
//////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc      UPnp::g_UPnpDeviceDesc;
QList<QHostAddress> UPnp::g_IPAddrList;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnp Class implementaion
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

UPnp::UPnp()
{
    LOG(VB_UPNP, LOG_DEBUG, "UPnp - Constructor");
    // N.B. Ask for 5 second delay to send Bye Bye twice
    // TODO Check whether we actually send Bye Bye twice:)
    m_power = MythPower::AcquireRelease(this, true, 5s);
    if (m_power)
    {
        // NB We only listen for WillXXX signals which should give us time to send notifications
        connect(m_power, &MythPower::WillRestart,  this, &UPnp::DisableNotifications);
        connect(m_power, &MythPower::WillSuspend,  this, &UPnp::DisableNotifications);
        connect(m_power, &MythPower::WillShutDown, this, &UPnp::DisableNotifications);
        connect(m_power, &MythPower::WokeUp,       this, &UPnp::EnableNotificatins);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnp::~UPnp()
{
    LOG(VB_UPNP, LOG_DEBUG, "UPnp - Destructor");
    CleanUp();
    if (m_power)
        MythPower::AcquireRelease(this, false);
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool UPnp::Initialize( int nServicePort, HttpServer *pHttpServer )
{
    QList<QHostAddress> sList = ServerPool::DefaultListen();
    if (sList.contains(QHostAddress(QHostAddress::AnyIPv4)))
    {
        sList.removeAll(QHostAddress(QHostAddress::AnyIPv4));
        sList.removeAll(QHostAddress(QHostAddress::AnyIPv6));
        sList.append(QNetworkInterface::allAddresses());
    }
    return Initialize( sList, nServicePort, pHttpServer );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool UPnp::Initialize( QList<QHostAddress> &sIPAddrList, int nServicePort, HttpServer *pHttpServer )
{
    LOG(VB_UPNP, LOG_DEBUG, "UPnp::Initialize - Begin");

    if (m_pHttpServer)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "UPnp::Initialize - Already initialized, programmer error.");
        return false;
    }

    m_pHttpServer = pHttpServer;
    if (m_pHttpServer == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "UPnp::Initialize - Invalid Parameter (pHttpServer == NULL)");
        return false;
    }

    g_IPAddrList   = sIPAddrList;
    bool ipv4 = gCoreContext->GetBoolSetting("IPv4Support",true);
    bool ipv6 = gCoreContext->GetBoolSetting("IPv6Support",true);

    for (int it = 0; it < g_IPAddrList.size(); ++it)
    {
        // If IPV4 support is disabled and this is an IPV4 address,
        // remove this address
        // If IPV6 support is disabled and this is an IPV6 address,
        // remove this address
        if ((g_IPAddrList[it].protocol() == QAbstractSocket::IPv4Protocol
                && ! ipv4)
          ||(g_IPAddrList[it].protocol() == QAbstractSocket::IPv6Protocol
                && ! ipv6))
            g_IPAddrList.removeAt(it--);
    }

    m_nServicePort = nServicePort;

    // ----------------------------------------------------------------------
    // Register any HttpServerExtensions
    // ----------------------------------------------------------------------

    m_pHttpServer->RegisterExtension(
        new SSDPExtension(m_nServicePort, m_pHttpServer->GetSharePath()));

    LOG(VB_UPNP, LOG_DEBUG, "UPnp::Initialize - End");

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Delay startup of Discovery Threads until all Extensions are registered.
//////////////////////////////////////////////////////////////////////////////

void UPnp::Start()
{
    LOG(VB_UPNP, LOG_DEBUG, "UPnp::Start - Enabling SSDP Notifications");
    // ----------------------------------------------------------------------
    // Turn on Device Announcements 
    // (this will also create/startup SSDP if not already done)
    // ----------------------------------------------------------------------

    SSDP::Instance()->EnableNotifications( m_nServicePort );

    LOG(VB_UPNP, LOG_DEBUG, "UPnp::Start - Returning");
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::CleanUp()
{
    LOG(VB_UPNP, LOG_INFO, "UPnp::CleanUp() - disabling SSDP notifications");

    SSDP::Instance()->DisableNotifications();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc *UPnp::GetDeviceDesc( QString &sURL )
{
    return UPnpDeviceDesc::Retrieve( sURL );
}

void UPnp::DisableNotifications(std::chrono::milliseconds /*unused*/)
{
    SSDP::Instance()->DisableNotifications();
}

void UPnp::EnableNotificatins(std::chrono::milliseconds /*unused*/) const
{
    SSDP::Instance()->EnableNotifications(m_nServicePort);
}
