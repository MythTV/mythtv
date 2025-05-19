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
#include <cerrno>
#include <chrono> // for milliseconds
#include <cstdlib>
#include <thread> // for sleep_for

#include "libmythbase/configuration.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"

#include "upnp.h"
#include "upnptasksearch.h"

#include "mmulticastsocketdevice.h"
#include "msocketdevice.h"

#include <QStringList>

#ifdef Q_OS_ANDROID
#include <sys/select.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// Broadcast Socket is used for XBox (original) since Multicast is not supported
/////////////////////////////////////////////////////////////////////////////

class MBroadcastSocketDevice : public MSocketDevice
{
  public:
    MBroadcastSocketDevice(const QString& sAddress, quint16 nPort) :
        MSocketDevice(MSocketDevice::Datagram),
        m_address(sAddress), m_port(nPort)
    {
        m_address.setAddress( sAddress );

        setProtocol(IPv4);
        setSocket(createNewSocket(), MSocketDevice::Datagram);

        int one = 1;
        if (setsockopt(socket(), SOL_SOCKET, SO_BROADCAST,
                       (const char *)&one, sizeof(one)) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "setsockopt - SO_BROADCAST Error" + ENO);
        }

        setAddressReusable(true);

        bind(m_address, m_port);
    }

    ~MBroadcastSocketDevice() override
    {
        int zero = 0;
        if (setsockopt(socket(), SOL_SOCKET, SO_BROADCAST, (const char *)&zero,
                       sizeof(zero)) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "setsockopt - SO_BROADCAST Error" + ENO);
        }
    }

    QHostAddress address() const override // MSocketDevice
        { return m_address; }
    quint16 port() const override // MSocketDevice
        { return m_port; }

  private:
    QHostAddress    m_address;
    quint16         m_port;
};

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

SSDP::SSDP() :
    MThread("SSDP")
{
    LOG(VB_UPNP, LOG_NOTICE, "Starting up SSDP Thread..." );

    {
        auto config = XmlConfiguration();

        m_nPort       = config.GetValue("UPnP/SSDP/Port"      , SSDP_PORT      );
        m_nSearchPort = config.GetValue("UPnP/SSDP/SearchPort", SSDP_SEARCHPORT);
    }

    m_sockets[ SocketIdx_Search    ] =
        new MMulticastSocketDevice();
    m_sockets[ SocketIdx_Multicast ] =
        new MMulticastSocketDevice(SSDP_GROUP, m_nPort);
    m_sockets[ SocketIdx_Broadcast ] =
        new MBroadcastSocketDevice("255.255.255.255", m_nPort);

    m_sockets[ SocketIdx_Search    ]->setBlocking( false );
    m_sockets[ SocketIdx_Multicast ]->setBlocking( false );
    m_sockets[ SocketIdx_Broadcast ]->setBlocking( false );

    // Setup SearchSocket
    QHostAddress ip4addr( QHostAddress::Any );

    m_sockets[ SocketIdx_Search ]->bind( ip4addr          , m_nSearchPort );
    m_sockets[ SocketIdx_Search ]->bind( QHostAddress::Any, m_nSearchPort );

    // ----------------------------------------------------------------------
    // Create the SSDP (Upnp Discovery) Thread.
    // ----------------------------------------------------------------------

    start();

    LOG(VB_UPNP, LOG_INFO, "SSDP Thread Starting soon" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP::~SSDP()
{
    LOG(VB_UPNP, LOG_NOTICE, "Shutting Down SSDP Thread..." );

    DisableNotifications();

    RequestTerminate();
    wait();

    if (m_pNotifyTask != nullptr)
    {
        m_pNotifyTask->DecrRef();
        m_pNotifyTask = nullptr;
    }

    for (auto & socket : m_sockets)
        delete socket;

    LOG(VB_UPNP, LOG_INFO, "SSDP Thread Terminated." );
}

void SSDP::RequestTerminate(void)
{
    m_bTermRequested = true;
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

    MSocketDevice *pSocket = m_sockets[ SocketIdx_Search ];
    if ( !pSocket->isValid() )
    {
        pSocket->setProtocol(MSocketDevice::IPv4);
        pSocket->setSocket(pSocket->createNewSocket(), MSocketDevice::Datagram);
    }

    QHostAddress address;
    address.setAddress( SSDP_GROUP );

    int nSize = sRequest.size();

    if ( pSocket->writeBlock( sRequest.data(),
                              sRequest.size(), address, SSDP_PORT ) != nSize)
        LOG(VB_GENERAL, LOG_INFO,
            "SSDP::PerformSearch - did not write entire buffer.");

    std::this_thread::sleep_for(std::chrono::milliseconds(MythRandom(0, 250)));

    if ( pSocket->writeBlock( sRequest.data(),
                              sRequest.size(), address, SSDP_PORT ) != nSize)
        LOG(VB_GENERAL, LOG_INFO,
            "SSDP::PerformSearch - did not write entire buffer.");
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::run()
{
    RunProlog();

    fd_set          read_set;
    struct timeval  timeout {};

    LOG(VB_UPNP, LOG_INFO, "SSDP::Run - SSDP Thread Started." );

    // ----------------------------------------------------------------------
    // Listen for new Requests
    // ----------------------------------------------------------------------

    while ( ! m_bTermRequested )
    {
        int nMaxSocket = 0;

        FD_ZERO( &read_set ); // NOLINT(readability-isolate-declaration)

        for (auto & socket : m_sockets)
        {
            if (socket != nullptr && socket->socket() >= 0)
            {
                FD_SET( socket->socket(), &read_set );
                nMaxSocket = std::max( socket->socket(), nMaxSocket );

#if 0
                if (socket->bytesAvailable() > 0)
                {
                    LOG(VB_GENERAL, LOG_DEBUG,
                        QString("Found Extra data before select: %1")
                        .arg(nIdx));
                    ProcessData( socket );
                }
#endif
            }
        }
        
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        int count = select(nMaxSocket + 1, &read_set, nullptr, nullptr, &timeout);

        for (int nIdx = 0; count && nIdx < kNumberOfSockets; nIdx++ )
        {
            bool cond1 = m_sockets[nIdx] != nullptr;
            bool cond2 = cond1 && m_sockets[nIdx]->socket() >= 0;
            bool cond3 = cond2 && FD_ISSET(m_sockets[nIdx]->socket(), &read_set);

            if (cond3)
            {
#if 0
                LOG(VB_GENERAL, LOG_DEBUG, QString("FD_ISSET( %1 )").arg(nIdx));
#endif

                ProcessData(m_sockets[nIdx]);
                count--;
            }
        }
    }

    RunEpilog();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::ProcessData( MSocketDevice *pSocket )
{
    QByteArray buffer;
    long nBytes = pSocket->bytesAvailable();
    int retries = 0;
    // Note: this function MUST do a read even if someone sends a zero byte UDP message
    // Otherwise the select() will continue to signal data ready, so to prevent using 100%
    // CPU, we need to call a recv function to make select() block again
    bool didDoRead = false;

    // UDP message of zero length? OK, "recv" it and move on
    if (nBytes == 0)
    {
        LOG(VB_UPNP, LOG_WARNING, QString("SSDP: Received 0 byte UDP message"));
    }

    while (((nBytes = pSocket->bytesAvailable()) > 0 || (nBytes == 0 && !didDoRead)) && !m_bTermRequested)
    {
        buffer.resize(nBytes);

        long nRead = 0;
        while ((nRead < nBytes) || (nBytes == 0 && !didDoRead))
        {
            long ret = pSocket->readBlock( buffer.data() + nRead, nBytes - nRead );
            didDoRead = true;
            if (ret < 0)
            {
                if (errno == EAGAIN
#if EAGAIN != EWOULDBLOCK
                    || errno == EWOULDBLOCK
#endif
                    )
                {
                    if (retries == 3)
                    {
                        nBytes = nRead;
                        buffer.resize(nBytes);
                        break;
                    }
                    retries++;
                    std::this_thread::sleep_for(10ms);
                    continue;
                }
                LOG(VB_GENERAL, LOG_ERR, QString("Socket readBlock error %1")
                    .arg(pSocket->error()));
                buffer.clear();
                break;
            }
            retries = 0;

            nRead += ret;

            if (0 == ret && nBytes != 0)
            {
                LOG(VB_SOCKET, LOG_WARNING,
                    QString("%1 bytes reported available, "
                            "but only %2 bytes read.")
                    .arg(nBytes).arg(nRead));
                nBytes = nRead;
                buffer.resize(nBytes);
                break;
            }
        }

        if (buffer.isEmpty())
            continue;

        QHostAddress  peerAddress = pSocket->peerAddress();
        quint16       peerPort    = pSocket->peerPort   ();
        
        // ------------------------------------------------------------------
        QString     str          = QString(buffer.constData());
        QStringList lines        = str.split("\r\n", Qt::SkipEmptyParts);
        QString     sRequestLine = !lines.empty() ? lines[0] : "";

        if (!lines.isEmpty())
            lines.pop_front();

        // ------------------------------------------------------------------
        // Parse request Type
        // ------------------------------------------------------------------

        LOG(VB_UPNP, LOG_DEBUG, QString("SSDP::ProcessData - requestLine: %1")
                .arg(sRequestLine));

        SSDPRequestType eType = ProcessRequestLine( sRequestLine );

        // ------------------------------------------------------------------
        // Read Headers into map
        // ------------------------------------------------------------------

        QStringMap  headers;

        for (const auto& sLine : std::as_const(lines))
        {
            QString sName  = sLine.section( ':', 0, 0 ).trimmed();
            QString sValue = sLine.section( ':', 1 );

            sValue.truncate( sValue.length() );  //-2

            if ((sName.length() != 0) && (sValue.length() !=0))
                headers.insert( sName.toLower(), sValue.trimmed() );
        }

#if 0
        pSocket->SetDestAddress( peerAddress, peerPort );
#endif

        // --------------------------------------------------------------
        // See if this is a valid request
        // --------------------------------------------------------------

        switch( eType )
        {
            case SSDP_MSearch:
            {
                // ----------------------------------------------------------
                // If we haven't enabled notifications yet, then we don't 
                // want to answer search requests.
                // ----------------------------------------------------------

                if (m_pNotifyTask != nullptr)
                    ProcessSearchRequest( headers, peerAddress, peerPort ); 

                break;
            }

            case SSDP_MSearchResp:
                ProcessSearchResponse( headers); 
                break;

            case SSDP_Notify:
                ProcessNotify( headers ); 
                break;

            case SSDP_Unknown:
            default:
                LOG(VB_UPNP, LOG_ERR,
                    "SSPD::ProcessData - Unknown request Type.");
                break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPRequestType SSDP::ProcessRequestLine( const QString &sLine )
{
    QStringList tokens = sLine.split(m_procReqLineExp, Qt::SkipEmptyParts);

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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString SSDP::GetHeaderValue( const QStringMap &headers,
                              const QString    &sKey, const QString &sDefault )
{
    QStringMap::const_iterator it = headers.find( sKey.toLower() );

    if ( it == headers.end())
        return( sDefault );

    return *it;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SSDP::ProcessSearchRequest( const QStringMap &sHeaders, 
                                 const QHostAddress& peerAddress,
                                 quint16           peerPort ) const
{
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
        auto *pTask = new UPnpSearchTask( m_nServicePort,
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
        auto *pTask = new UPnpSearchTask( m_nServicePort, peerAddress,
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SSDP::ProcessSearchResponse( const QStringMap &headers )
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SSDP::ProcessNotify( const QStringMap &headers )
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

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPExtension Implementation
// 
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPExtension::SSDPExtension( int nServicePort , const QString &sSharePath)
  : HttpServerExtension( "SSDP" , sSharePath),
    m_nServicePort(nServicePort)
{
    m_nSupportedMethods |= (RequestTypeMSearch | RequestTypeNotify);
    m_sUPnpDescPath = XmlConfiguration().GetValue("UPnP/DescXmlPath", m_sSharePath);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPMethod SSDPExtension::GetMethod( const QString &sURI )
{
    if (sURI == "getDeviceDesc"     ) return( SSDPM_GetDeviceDesc    );
    if (sURI == "getDeviceList"     ) return( SSDPM_GetDeviceList    );

    return( SSDPM_Unknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList SSDPExtension::GetBasePaths() 
{
    // -=>TODO: This is very inefficient... should look into making 
    //          it a unique path.

    return QStringList( "/" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SSDPExtension::ProcessRequest( HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if ( pRequest->m_sBaseUrl != "/")
            return( false );

        switch( GetMethod( pRequest->m_sMethod ))
        {
            case SSDPM_GetDeviceDesc: GetDeviceDesc( pRequest ); return( true );
            case SSDPM_GetDeviceList: GetDeviceList( pRequest ); return( true );

            default: break;
        }
    }

    return( false );
}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void SSDPExtension::GetDeviceDesc( HTTPRequest *pRequest ) const
{
    pRequest->m_eResponseType = ResponseTypeXML;

    QString sUserAgent = pRequest->GetRequestHeader( "User-Agent", "" );

    LOG(VB_UPNP, LOG_DEBUG, "SSDPExtension::GetDeviceDesc - " +
        QString( "Host=%1 Port=%2 UserAgent=%3" )
            .arg(pRequest->GetHostAddress()) .arg(m_nServicePort)
            .arg(sUserAgent));

    QTextStream stream( &(pRequest->m_response) );

    UPnp::g_UPnpDeviceDesc.GetValidXML( pRequest->GetHostAddress(), 
                                        m_nServicePort,
                                        stream,
                                        sUserAgent  );
}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void SSDPExtension::GetFile( HTTPRequest *pRequest, const QString& sFileName )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;

    pRequest->m_sFileName = m_sUPnpDescPath + sFileName;

    if (QFile::exists( pRequest->m_sFileName ))
    {
        LOG(VB_UPNP, LOG_DEBUG,
            QString("SSDPExtension::GetFile( %1 ) - Exists")
                .arg(pRequest->m_sFileName));

        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        pRequest->m_mapRespHeaders[ "Cache-Control" ]
            = "no-cache=\"Ext\", max-age = 7200"; // 2 hours
    }
    else
    {
        pRequest->m_nResponseStatus = 404;
        pRequest->m_response.write( pRequest->GetResponsePage() );
        LOG(VB_UPNP, LOG_ERR,
            QString("SSDPExtension::GetFile( %1 ) - Not Found")
                .arg(pRequest->m_sFileName));
    }

}

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

void SSDPExtension::GetDeviceList( HTTPRequest *pRequest )
{
    LOG(VB_UPNP, LOG_DEBUG, "SSDPExtension::GetDeviceList");

    QString     sXML;
    QTextStream os(&sXML, QIODevice::WriteOnly);

    uint nDevCount = 0;
    uint nEntryCount = 0;
    SSDPCache::Instance()->OutputXML(os, &nDevCount, &nEntryCount);

    NameValues list;
    list.push_back(
        NameValue("DeviceCount",           (int)nDevCount));
    list.push_back(
        NameValue("DevicesAllocated",      SSDPCacheEntries::g_nAllocated));
    list.push_back(
        NameValue("CacheEntriesFound",     (int)nEntryCount));
    list.push_back(
        NameValue("CacheEntriesAllocated", DeviceLocation::g_nAllocated));
    list.push_back(
        NameValue("DeviceList",            sXML));

    pRequest->FormatActionResponse(list);

    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 200;
}
