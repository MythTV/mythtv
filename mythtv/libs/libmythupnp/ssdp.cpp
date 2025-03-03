//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdp.cpp
// Created     : Oct. 1, 2005
//
// Purpose     : SSDP Discovery Service Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "ssdp.h"

#include <algorithm>
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

#include <QByteArray>
#include <QHostAddress>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkDatagram>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUdpSocket>

#include "libmythbase/configuration.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"

#include "ssdpcache.h"
#include "taskqueue.h"
#include "upnp.h"
#include "upnptasknotify.h"
#include "upnptasksearch.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDP Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// We're creating this class immediately so it will always be available.

static QMutex g_pSSDPCreationLock;
SSDP* SSDP::g_pSSDP = nullptr;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP* SSDP::Instance()
{
    QMutexLocker locker(&g_pSSDPCreationLock);
    return g_pSSDP ? g_pSSDP : (g_pSSDP = new SSDP());
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::Shutdown()
{
    QMutexLocker locker(&g_pSSDPCreationLock);
    delete g_pSSDP;
    g_pSSDP = nullptr;
}
 
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP::SSDP()
{
    LOG(VB_UPNP, LOG_NOTICE, "SSDP instance created." );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP::~SSDP()
{
    LOG(VB_UPNP, LOG_NOTICE, "Destroying SSDP instance." );

    DisableNotifications();
    if (m_pNotifyTask != nullptr)
    {
        m_pNotifyTask->DecrRef();
        m_pNotifyTask = nullptr;
    }

    LOG(VB_UPNP, LOG_INFO, "SSDP instance destroyed." );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::EnableNotifications( int nServicePort )
{
    if ( m_pNotifyTask == nullptr )
    {
        m_nServicePort = nServicePort;

        LOG(VB_UPNP, LOG_INFO,
            "SSDP::EnableNotifications() - creating new task");
        m_pNotifyTask = new UPnpNotifyTask( m_nServicePort ); 

        // ------------------------------------------------------------------
        // First Send out Notification that we are leaving the network.
        // ------------------------------------------------------------------

        LOG(VB_UPNP, LOG_INFO,
            "SSDP::EnableNotifications() - sending NTS_byebye");
        m_pNotifyTask->SetNTS( NTS_byebye );
        m_pNotifyTask->Execute( nullptr );

        m_bAnnouncementsEnabled = true;
    }

    // ------------------------------------------------------------------
    // Add Announcement Task to the Queue
    // ------------------------------------------------------------------

    LOG(VB_UPNP, LOG_INFO, "SSDP::EnableNotifications() - sending NTS_alive");

    m_pNotifyTask->SetNTS( NTS_alive );

    TaskQueue::Instance()->AddTask(m_pNotifyTask);

    LOG(VB_UPNP, LOG_INFO,
        "SSDP::EnableNotifications() - Task added to UPnP queue");
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::DisableNotifications()
{
    m_bAnnouncementsEnabled = false;

    if (m_pNotifyTask != nullptr)
    {
        // Send Announcement that we are leaving.

        m_pNotifyTask->SetNTS( NTS_byebye );
        m_pNotifyTask->Execute( nullptr );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
void SSDP::PerformSearch(const QString &sST, std::chrono::seconds timeout)
{
    timeout = std::clamp(timeout, 1s, 5s);
    QString rRequest = QString("M-SEARCH * HTTP/1.1\r\n"
                               "HOST: 239.255.255.250:1900\r\n"
                               "MAN: \"ssdp:discover\"\r\n"
                               "MX: %1\r\n"
                               "ST: %2\r\n"
                               "\r\n")
        .arg(timeout.count()).arg(sST);

    LOG(VB_UPNP, LOG_DEBUG, QString("\n\n%1\n").arg(rRequest));

    QByteArray sRequest = rRequest.toUtf8();
    int nSize = sRequest.size();

    QUdpSocket socket;
    socket.bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    if (socket.writeDatagram(sRequest, QHostAddress(QString(SSDP_GROUP)), SSDP_PORT) != nSize)
    {
        LOG(VB_GENERAL, LOG_INFO, "SSDP::PerformSearch - did not write entire buffer.");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(MythRandom(0, 250)));

    if (socket.writeDatagram(sRequest, QHostAddress(QString(SSDP_GROUP)), SSDP_PORT) != nSize)
    {
        LOG(VB_GENERAL, LOG_INFO, "SSDP::PerformSearch - did not write entire buffer.");
    }
}

static SSDPRequestType ProcessRequestLine(const QString &sLine)
{
    static const QRegularExpression k_whitespace {"\\s+"};
    QStringList tokens = sLine.split(k_whitespace, Qt::SkipEmptyParts);

    // ----------------------------------------------------------------------
    // if this is actually a response, then sLine's format will be:
    //      HTTP/m.n <response code> <response text>
    // otherwise:
    //      <method> <Resource URI> HTTP/m.n
    // ----------------------------------------------------------------------

    if ( sLine.startsWith( QString("HTTP/") ))
        return SSDP_MSearchResp;

    if (tokens.count() > 0)
    {
        if (tokens[0] == "M-SEARCH" ) return SSDP_MSearch;
        if (tokens[0] == "NOTIFY"   ) return SSDP_Notify;
    }

    return SSDP_Unknown;
}

static QString GetHeaderValue(const QMap<QString, QString> &headers,
                              const QString    &sKey, const QString &sDefault )
{
    QMap<QString, QString>::const_iterator it = headers.find(sKey.toLower());

    if ( it == headers.end())
        return( sDefault );

    return *it;
}

static bool ProcessSearchRequest(const QMap<QString, QString> &sHeaders,
                                 const QHostAddress& peerAddress,
                                 quint16 peerPort,
                                 int servicePort)
{
    // Don't respond if notifications are not enabled.
    if (servicePort == 0)
    {
        return true;
    }

    QString sMAN = GetHeaderValue( sHeaders, "MAN", "" );
    QString sST  = GetHeaderValue( sHeaders, "ST" , "" );
    QString sMX  = GetHeaderValue( sHeaders, "MX" , "" );
    std::chrono::seconds nMX  = 0s;

    LOG(VB_UPNP, LOG_DEBUG, QString("SSDP::ProcessSearchrequest : [%1] MX=%2")
             .arg(sST, sMX));

    // ----------------------------------------------------------------------
    // Validate Header Values...
    // ----------------------------------------------------------------------

#if 0
    if ( pRequest->m_sMethod   != "*"                 ) return false;
    if ( pRequest->m_sProtocol != "HTTP"              ) return false;
    if ( pRequest->m_nMajor    != 1                   ) return false;
#endif
    if ( sMAN                  != "\"ssdp:discover\"" ) return false;
    if ( sST.length()          == 0                   ) return false;
    if ( sMX.length()          == 0                   ) return false;
    nMX = std::chrono::seconds(sMX.toInt());
    if ( nMX                   <= 0s                  ) return false;

    // ----------------------------------------------------------------------
    // Adjust timeout to be a random interval between 0 and MX (max of 120)
    // ----------------------------------------------------------------------

    nMX = std::clamp(nMX, 0s, 120s);

    auto nNewMX = std::chrono::milliseconds(MythRandom(0, (duration_cast<std::chrono::milliseconds>(nMX)).count()));

    // ----------------------------------------------------------------------
    // See what they are looking for...
    // ----------------------------------------------------------------------

    if ((sST == "ssdp:all") || (sST == "upnp:rootdevice"))
    {
        auto *pTask = new UPnpSearchTask(servicePort,
            peerAddress, peerPort, sST, 
            UPnp::g_UPnpDeviceDesc.m_rootDevice.GetUDN());

#if 0
        // Excute task now for fastest response, queue for time-delayed response
        // -=>TODO: To be trully uPnp compliant, this Execute should be removed.
        pTask->Execute( nullptr );
#endif

        TaskQueue::Instance()->AddTask( nNewMX, pTask );

        pTask->DecrRef();

        return true;
    }

    // ----------------------------------------------------------------------
    // Look for a specific device/service
    // ----------------------------------------------------------------------

    QString sUDN = UPnp::g_UPnpDeviceDesc.FindDeviceUDN(
        &(UPnp::g_UPnpDeviceDesc.m_rootDevice), sST );

    if (sUDN.length() > 0)
    {
        auto *pTask = new UPnpSearchTask(servicePort, peerAddress,
                                          peerPort, sST, sUDN );

        // Excute task now for fastest response, queue for time-delayed response
        // -=>TODO: To be trully uPnp compliant, this Execute should be removed.
        pTask->Execute( nullptr );

        TaskQueue::Instance()->AddTask( nNewMX, pTask );

        pTask->DecrRef();

        return true;
    }

    return false;
}

static bool ProcessSearchResponse(const QMap<QString, QString> &headers)
{
    QString sDescURL = GetHeaderValue( headers, "LOCATION"      , "" );
    QString sST      = GetHeaderValue( headers, "ST"            , "" );
    QString sUSN     = GetHeaderValue( headers, "USN"           , "" );
    QString sCache   = GetHeaderValue( headers, "CACHE-CONTROL" , "" );

    LOG(VB_UPNP, LOG_DEBUG,
        QString( "SSDP::ProcessSearchResponse ...\n"
                 "DescURL=%1\n"
                 "ST     =%2\n"
                 "USN    =%3\n"
                 "Cache  =%4")
             .arg(sDescURL, sST, sUSN, sCache));

    int nPos = sCache.indexOf("max-age", 0, Qt::CaseInsensitive);

    if (nPos < 0)
        return false;

    nPos = sCache.indexOf("=", nPos);
    if (nPos < 0)
        return false;

    auto nSecs = std::chrono::seconds(sCache.mid( nPos+1 ).toInt());

    SSDPCache::Instance()->Add( sST, sUSN, sDescURL, nSecs );

    return true;
}

static bool ProcessNotify(const QMap<QString, QString> &headers)
{
    QString sDescURL = GetHeaderValue( headers, "LOCATION"      , "" );
    QString sNTS     = GetHeaderValue( headers, "NTS"           , "" );
    QString sNT      = GetHeaderValue( headers, "NT"            , "" );
    QString sUSN     = GetHeaderValue( headers, "USN"           , "" );
    QString sCache   = GetHeaderValue( headers, "CACHE-CONTROL" , "" );

    LOG(VB_UPNP, LOG_DEBUG,
        QString( "SSDP::ProcessNotify ...\n"
                 "DescURL=%1\n"
                 "NTS    =%2\n"
                 "NT     =%3\n"
                 "USN    =%4\n"
                 "Cache  =%5" )
            .arg(sDescURL, sNTS, sNT, sUSN, sCache));

    if (sNTS.contains( "ssdp:alive"))
    {
        int nPos = sCache.indexOf("max-age", 0, Qt::CaseInsensitive);

        if (nPos < 0)
            return false;

        nPos = sCache.indexOf("=", nPos);
        if (nPos < 0)
            return false;

        auto nSecs = std::chrono::seconds(sCache.mid( nPos+1 ).toInt());

        SSDPCache::Instance()->Add( sNT, sUSN, sDescURL, nSecs );

        return true;
    }


    if ( sNTS.contains( "ssdp:byebye" ) )
    {
        SSDPCache::Instance()->Remove( sNT, sUSN );

        return true;
    }

    return false;
}

SSDPReceiver::SSDPReceiver() :
    m_port(XmlConfiguration().GetValue("UPnP/SSDP/Port", SSDP_PORT))
{
    m_socket.bind(QHostAddress::AnyIPv4, m_port, QUdpSocket::ShareAddress);
    m_socket.joinMulticastGroup(m_groupAddress);

    connect(&m_socket, &QUdpSocket::readyRead, this, &SSDPReceiver::processPendingDatagrams);
}

void SSDPReceiver::processPendingDatagrams()
{
    while (m_socket.hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_socket.receiveDatagram();
        QString     str          = QString::fromUtf8(datagram.data());
        QStringList lines        = str.split("\r\n", Qt::SkipEmptyParts);
        QString     sRequestLine = !lines.empty() ? lines[0] : "";

        if (!lines.isEmpty())
            lines.pop_front();

        // Parse request Type
        LOG(VB_UPNP, LOG_DEBUG, QString("SSDP::ProcessData - requestLine: %1")
                .arg(sRequestLine));
        SSDPRequestType eType = ProcessRequestLine( sRequestLine );

        // Read Headers into map
        QMap<QString, QString>  headers;
        for (const auto& sLine : std::as_const(lines))
        {
            QString sName  = sLine.section(':', 0, 0).trimmed();
            QString sValue = sLine.section(':', 1);

            sValue.truncate(sValue.length());  //-2

            if ((sName.length() != 0) && (sValue.length() != 0))
            {
                headers.insert(sName.toLower(), sValue.trimmed());
            }
        }

        // See if this is a valid request
        switch (eType)
        {
            case SSDP_MSearch:
            {
                ProcessSearchRequest(headers, datagram.senderAddress(),
                    datagram.senderPort(), SSDP::Instance()->getNotificationPort());

                break;
            }

            case SSDP_MSearchResp:
                ProcessSearchResponse(headers);
                break;

            case SSDP_Notify:
                ProcessNotify(headers);
                break;

            case SSDP_Unknown:
            default:
                LOG(VB_UPNP, LOG_ERR, "SSPD::ProcessData - Unknown request Type.");
                break;
        }
    }
}
