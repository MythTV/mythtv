//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdp.cpp
// Created     : Oct. 1, 2005
//
// Purpose     : SSDP Discovery Service Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <algorithm>

#include "upnp.h"
#include "mythlogging.h"

#include "upnptasksearch.h"
#include "upnptaskcache.h"

#include "mmulticastsocketdevice.h"
#include "mbroadcastsocketdevice.h"

#include <QRegExp>
#include <QStringList>

#include <stdlib.h>
#include <errno.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDP Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// We're creating this class immediately so it will always be available.

static QMutex g_pSSDPCreationLock;
SSDP* SSDP::g_pSSDP = NULL;

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
    g_pSSDP = NULL;
}
 
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP::SSDP() :
    MThread                ("SSDP" ),
    m_procReqLineExp       ("[ \r\n][ \r\n]*"),
    m_nPort                ( SSDP_PORT ),
    m_nSearchPort          ( SSDP_SEARCHPORT ),
    m_nServicePort         ( 0 ),
    m_pNotifyTask          ( NULL ),
    m_bAnnouncementsEnabled( false ),
    m_bTermRequested       ( false ),
    m_lock                 ( QMutex::NonRecursive )
{
    LOG(VB_UPNP, LOG_NOTICE, "Starting up SSDP Thread..." );

    Configuration *pConfig = UPnp::GetConfiguration();

    m_nPort       = pConfig->GetValue("UPnP/SSDP/Port"      , SSDP_PORT      );
    m_nSearchPort = pConfig->GetValue("UPnP/SSDP/SearchPort", SSDP_SEARCHPORT);

    m_Sockets[ SocketIdx_Search    ] =
        new MMulticastSocketDevice();
    m_Sockets[ SocketIdx_Multicast ] =
        new MMulticastSocketDevice(SSDP_GROUP, m_nPort);
    m_Sockets[ SocketIdx_Broadcast ] =
        new MBroadcastSocketDevice("255.255.255.255", m_nPort);

    m_Sockets[ SocketIdx_Search    ]->setBlocking( false );
    m_Sockets[ SocketIdx_Multicast ]->setBlocking( false );
    m_Sockets[ SocketIdx_Broadcast ]->setBlocking( false );

    // Setup SearchSocket
    QHostAddress ip4addr( QHostAddress::Any );

    m_Sockets[ SocketIdx_Search ]->bind( ip4addr          , m_nSearchPort );
    m_Sockets[ SocketIdx_Search ]->bind( QHostAddress::Any, m_nSearchPort );

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

    m_bTermRequested = true;
    wait();

    if (m_pNotifyTask != NULL)
    {
        m_pNotifyTask->DecrRef();
        m_pNotifyTask = NULL;
    }

    for (int nIdx = 0; nIdx < (int)NumberOfSockets; nIdx++ )
    {
        if (m_Sockets[ nIdx ] != NULL )
        {
            delete m_Sockets[ nIdx ];
        }
    }

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
    if ( m_pNotifyTask == NULL )
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
        m_pNotifyTask->Execute( NULL );

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

    if (m_pNotifyTask != NULL)
    {
        // Send Announcement that we are leaving.

        m_pNotifyTask->SetNTS( NTS_byebye );
        m_pNotifyTask->Execute( NULL );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
void SSDP::PerformSearch(const QString &sST, uint timeout_secs)
{
    timeout_secs = std::max(std::min(timeout_secs, 5U), 1U);
    QString rRequest = QString("M-SEARCH * HTTP/1.1\r\n"
                               "HOST: 239.255.255.250:1900\r\n"
                               "MAN: \"ssdp:discover\"\r\n"
                               "MX: %1\r\n"
                               "ST: %2\r\n"
                               "\r\n")
        .arg(timeout_secs).arg(sST);

    LOG(VB_UPNP, LOG_DEBUG, QString("\n\n%1\n").arg(rRequest));

    QByteArray sRequest = rRequest.toUtf8();

    MSocketDevice *pSocket = m_Sockets[ SocketIdx_Search ];
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

    usleep( random() % 250000 );

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
    struct timeval  timeout;

    LOG(VB_UPNP, LOG_INFO, "SSDP::Run - SSDP Thread Started." );

    // ----------------------------------------------------------------------
    // Listen for new Requests
    // ----------------------------------------------------------------------

    while ( ! m_bTermRequested )
    {
        int nMaxSocket = 0;

        FD_ZERO( &read_set );

        for (uint nIdx = 0; nIdx < NumberOfSockets; nIdx++ )
        {
            if (m_Sockets[nIdx] != NULL && m_Sockets[nIdx]->socket() >= 0)
            {
                FD_SET( m_Sockets[ nIdx ]->socket(), &read_set );
                nMaxSocket = max( m_Sockets[ nIdx ]->socket(), nMaxSocket );

#if 0
                if (m_Sockets[ nIdx ]->bytesAvailable() > 0)
                {
                    LOG(VB_GENERAL, LOG_DEBUG,
                        QString("Found Extra data before select: %1")
                        .arg(nIdx));
                    ProcessData( m_Sockets[ nIdx ] );
                }
#endif
            }
        }
        
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        int count;
        count = select(nMaxSocket + 1, &read_set, NULL, NULL, &timeout);
        for (int nIdx = 0; count && nIdx < (int)NumberOfSockets; nIdx++ )
        {
            if (m_Sockets[nIdx] != NULL && m_Sockets[nIdx]->socket() >= 0 &&
                FD_ISSET(m_Sockets[nIdx]->socket(), &read_set))
            {
#if 0
                LOG(VB_GENERAL, LOG_DEBUG, QString("FD_ISSET( %1 )").arg(nIdx));
#endif
                ProcessData(m_Sockets[nIdx]);
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
    long nBytes = 0;
    int retries = 0;

    while ((nBytes = pSocket->bytesAvailable()) > 0)
    {
        buffer.resize(nBytes);

        long nRead = 0;
        do
        {
            long ret = pSocket->readBlock( buffer.data() + nRead, nBytes - nRead );
            if (ret < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    if (retries == 3)
                    {
                        nBytes = nRead;
                        buffer.resize(nBytes);
                        break;
                    }
                    retries++;
                    usleep(10000);
                    continue;
                }
                LOG(VB_GENERAL, LOG_ERR, QString("Socket readBlock error %1")
                    .arg(pSocket->error()));
                buffer.clear();
                break;
            }
            retries = 0;

            nRead += ret;

            if (0 == ret)
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
        while (nRead < nBytes);

        if (buffer.isEmpty())
            continue;

        QHostAddress  peerAddress = pSocket->peerAddress();
        quint16       peerPort    = pSocket->peerPort   ();

        // ------------------------------------------------------------------
        QString     str          = QString(buffer.constData());
        QStringList lines        = str.split("\r\n", QString::SkipEmptyParts);
        QString     sRequestLine = lines.size() ? lines[0] : "";

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

        for ( QStringList::Iterator it = lines.begin();
                                    it != lines.end(); ++it ) 
        {
            QString sLine  = *it;
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

                if (m_pNotifyTask != NULL)
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
    QStringList tokens = sLine.split(m_procReqLineExp, QString::SkipEmptyParts);

    // ----------------------------------------------------------------------
    // if this is actually a response, then sLine's format will be:
    //      HTTP/m.n <response code> <response text>
    // otherwise:
    //      <method> <Resource URI> HTTP/m.n
    // ----------------------------------------------------------------------

    if ( sLine.startsWith( QString("HTTP/") ))
        return SSDP_MSearchResp;
    else
    {
        if (tokens.count() > 0)
        {
            if (tokens[0] == "M-SEARCH" ) return SSDP_MSearch;
            if (tokens[0] == "NOTIFY"   ) return SSDP_Notify;
        }
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
                                 QHostAddress      peerAddress,
                                 quint16           peerPort )
{
    QString sMAN = GetHeaderValue( sHeaders, "MAN", "" );
    QString sST  = GetHeaderValue( sHeaders, "ST" , "" );
    QString sMX  = GetHeaderValue( sHeaders, "MX" , "" );
    int     nMX  = 0;

    LOG(VB_UPNP, LOG_DEBUG, QString("SSDP::ProcessSearchrequest : [%1] MX=%2")
             .arg(sST).arg(sMX));

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
    if ((nMX = sMX.toInt())    == 0                   ) return false;
    if ( nMX                    < 0                   ) return false;

    // ----------------------------------------------------------------------
    // Adjust timeout to be a random interval between 0 and MX (max of 120)
    // ----------------------------------------------------------------------

    nMX = (nMX > 120) ? 120 : nMX;

    int nNewMX = (int)(0 + ((unsigned short)random() % nMX)) * 1000;

    // ----------------------------------------------------------------------
    // See what they are looking for...
    // ----------------------------------------------------------------------

    if ((sST == "ssdp:all") || (sST == "upnp:rootdevice"))
    {
        UPnpSearchTask *pTask = new UPnpSearchTask( m_nServicePort, 
            peerAddress, peerPort, sST, 
            UPnp::g_UPnpDeviceDesc.m_rootDevice.GetUDN());

#if 0
        // Excute task now for fastest response, queue for time-delayed response
        // -=>TODO: To be trully uPnp compliant, this Execute should be removed.
        pTask->Execute( NULL );
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
        UPnpSearchTask *pTask = new UPnpSearchTask( m_nServicePort,
                                                    peerAddress,
                                                    peerPort,
                                                    sST, 
                                                    sUDN );

        // Excute task now for fastest response, queue for time-delayed response
        // -=>TODO: To be trully uPnp compliant, this Execute should be removed.
        pTask->Execute( NULL );

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
             .arg(sDescURL).arg(sST).arg(sUSN).arg(sCache));

    int nPos = sCache.indexOf("max-age", 0, Qt::CaseInsensitive);

    if (nPos < 0)
        return false;

    if ((nPos = sCache.indexOf("=", nPos)) < 0)
        return false;

    int nSecs = sCache.mid( nPos+1 ).toInt();

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
            .arg(sDescURL).arg(sNTS).arg(sNT).arg(sUSN).arg(sCache));

    if (sNTS.contains( "ssdp:alive"))
    {
        int nPos = sCache.indexOf("max-age", 0, Qt::CaseInsensitive);

        if (nPos < 0)
            return false;

        if ((nPos = sCache.indexOf("=", nPos)) < 0)
            return false;

        int nSecs = sCache.mid( nPos+1 ).toInt();

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

SSDPExtension::SSDPExtension( int nServicePort , const QString sSharePath)
  : HttpServerExtension( "SSDP" , sSharePath),
    m_nServicePort(nServicePort)
{
    m_sUPnpDescPath = UPnp::GetConfiguration()->GetValue( "UPnP/DescXmlPath",
                                                 m_sSharePath );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPExtension::~SSDPExtension( )
{
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

void SSDPExtension::GetDeviceDesc( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType = ResponseTypeXML;

    QString sUserAgent = pRequest->GetHeaderValue( "User-Agent", "" );

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

void SSDPExtension::GetFile( HTTPRequest *pRequest, QString sFileName )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;
    pRequest->m_nResponseStatus = 404;

    pRequest->m_sFileName = m_sUPnpDescPath + sFileName;

    if (QFile::exists( pRequest->m_sFileName ))
    {
        LOG(VB_UPNP, LOG_DEBUG,
            QString("SSDPExtension::GetFile( %1 ) - Exists")
                .arg(pRequest->m_sFileName));

        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        pRequest->m_mapRespHeaders[ "Cache-Control" ]
            = "no-cache=\"Ext\", max-age = 5000";
    }
    else
    {
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

    uint nDevCount, nEntryCount;
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
