//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdp.cpp
// Created     : Oct. 1, 2005
//
// Purpose     : SSDP Discovery Service Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
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

#include "upnp.h"
#include "mythverbose.h"
#include "mythlogging.h"

#include "upnptasksearch.h"
#include "upnptaskcache.h"

#include "multicast.h"
#include "broadcast.h"

#include <QRegExp>
#include <QStringList>

#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDP Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// We're creating this class immediately so it will always be available.

SSDP* SSDP::g_pSSDP = NULL;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP* SSDP::Instance()
{
    return g_pSSDP ? g_pSSDP : (g_pSSDP = new SSDP());
}
 
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP::SSDP() :
    m_procReqLineExp       ("[ \r\n][ \r\n]*"),
    m_nPort                ( SSDP_PORT ),
    m_nSearchPort          ( SSDP_SEARCHPORT ),
    m_nServicePort         ( 0 ),
    m_pNotifyTask          ( NULL ),
    m_bAnnouncementsEnabled( false ),
    m_bTermRequested       ( false ),
    m_lock                 ( QMutex::NonRecursive )
{
    VERBOSE(VB_UPNP, "SSDP::ctor - Starting up SSDP Thread..." );

    Configuration *pConfig = UPnp::GetConfiguration();

    m_nPort       = pConfig->GetValue( "UPnP/SSDP/Port"      , SSDP_PORT       );
    m_nSearchPort = pConfig->GetValue( "UPnP/SSDP/SearchPort", SSDP_SEARCHPORT );

    m_Sockets[ SocketIdx_Search    ] = new MSocketDevice( MSocketDevice::Datagram );
    m_Sockets[ SocketIdx_Multicast ] = new QMulticastSocket( SSDP_GROUP, m_nPort );
    m_Sockets[ SocketIdx_Broadcast ] = new QBroadcastSocket( "255.255.255.255", m_nPort );

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

    VERBOSE(VB_UPNP, "SSDP::ctor - SSDP Thread Started." );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP::~SSDP()
{
    VERBOSE(VB_UPNP, "SSDP::Destructor - Shutting Down SSDP Thread..." );

    DisableNotifications();

    m_bTermRequested = true;
    wait();

    if (m_pNotifyTask != NULL)
        m_pNotifyTask->Release();

    for (int nIdx = 0; nIdx < (int)NumberOfSockets; nIdx++ )
    {
        if (m_Sockets[ nIdx ] != NULL )
        {
            delete m_Sockets[ nIdx ];
        }
    }

    g_pSSDP = NULL;

    VERBOSE(VB_UPNP, "SSDP::Destructor - SSDP Thread Terminated." );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::EnableNotifications( int nServicePort )
{
    if ( m_pNotifyTask == NULL )
    {
        m_nServicePort = nServicePort;

        VERBOSE(VB_UPNP, "SSDP::EnableNotifications() - creating new task");
        m_pNotifyTask = new UPnpNotifyTask( m_nServicePort ); 

        // ------------------------------------------------------------------
        // Let's make sure to hold on to a reference of the NotifyTask.
        // ------------------------------------------------------------------

        m_pNotifyTask->AddRef();

        // ------------------------------------------------------------------
        // First Send out Notification that we are leaving the network.
        // ------------------------------------------------------------------

        VERBOSE(VB_UPNP, "SSDP::EnableNotifications() - sending NTS_byebye");
        m_pNotifyTask->SetNTS( NTS_byebye );
        m_pNotifyTask->Execute( NULL );

        m_bAnnouncementsEnabled = true;
    }

    // ------------------------------------------------------------------
    // Add Announcement Task to the Queue
    // ------------------------------------------------------------------

    VERBOSE(VB_UPNP, "SSDP::EnableNotifications() - sending NTS_alive");

    m_pNotifyTask->SetNTS( NTS_alive );

    TaskQueue::Instance()->AddTask( m_pNotifyTask  );

    VERBOSE(VB_UPNP, "SSDP::EnableNotifications() - Task added to UPnP queue");
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

void SSDP::PerformSearch( const QString &sST )
{
    QString rRequest = QString( "M-SEARCH * HTTP/1.1\r\n"
                                "HOST: 239.255.255.250:1900\r\n"
                                "MAN: \"ssdp:discover\"\r\n"
                                "MX: 2\r\n"
                                "ST: %1\r\n"
                                "\r\n" ).arg( sST );
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
        cerr << "SSDP::PerformSearch - did not write entire buffer." << endl;

    usleep( rand() % 250000 );

    if ( pSocket->writeBlock( sRequest.data(),
                              sRequest.size(), address, SSDP_PORT ) != nSize)
        cerr << "SSDP::PerformSearch - did not write entire buffer." << endl;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::run()
{
    fd_set          read_set;
    struct timeval  timeout;

    threadRegister("SSDP");
    VERBOSE(VB_UPNP, "SSDP::Run - SSDP Thread Started." );

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
	            cout << "Found Extra data before select: " << nIdx << endl;
                    ProcessData( m_Sockets[ nIdx ] );
                }
#endif

            }
        }
        
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        if ( select( nMaxSocket + 1, &read_set, NULL, NULL, &timeout ) != -1)
        {
            for (int nIdx = 0; nIdx < (int)NumberOfSockets; nIdx++ )
            {
                if (m_Sockets[nIdx] != NULL && m_Sockets[nIdx]->socket() >= 0)
                {
                    if (FD_ISSET( m_Sockets[ nIdx ]->socket(), &read_set ))
                    {
                        // cout << "FD_ISSET( " << nIdx << " ) " << endl;

                        ProcessData( m_Sockets[ nIdx ] );
                    }
                }
            }
        }
    }
    threadDeregister();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::ProcessData( MSocketDevice *pSocket )
{
    long nBytes = 0;
    long nRead  = 0;

    while ((nBytes = pSocket->bytesAvailable()) > 0)
    {
        QByteArray buffer;
        buffer.resize(nBytes);

        nRead = pSocket->readBlock( buffer.data(), nBytes );

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

        VERBOSE(VB_UPNP+VB_EXTRA,
                QString( "SSDP::ProcessData - requestLine: %1" )
                .arg( sRequestLine ));

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

//        pSocket->SetDestAddress( peerAddress, peerPort );

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
                VERBOSE(VB_UPNP, "SSPD::ProcessData - Unknown request Type.");
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

    VERBOSE( VB_UPNP+VB_EXTRA,
             QString( "SSDP::ProcessSearchrequest : [%1] MX=%2" )
             .arg( sST ).arg( sMX ));

    // ----------------------------------------------------------------------
    // Validate Header Values...
    // ----------------------------------------------------------------------

//    if ( pRequest->m_sMethod   != "*"                 ) return false;
//    if ( pRequest->m_sProtocol != "HTTP"              ) return false;
//    if ( pRequest->m_nMajor    != 1                   ) return false;
    if ( sMAN                  != "\"ssdp:discover\"" ) return false;
    if ( sST.length()          == 0                   ) return false;
    if ( sMX.length()          == 0                   ) return false;
    if ((nMX = sMX.toInt())    == 0                   ) return false;
    if ( nMX                    < 0                   ) return false;

    // ----------------------------------------------------------------------
    // Adjust timeout to be a random interval between 0 and MX (max of 120)
    // ----------------------------------------------------------------------

    nMX = (nMX > 120) ? 120 : nMX;

    int nNewMX = (int)(0 + ((unsigned short)rand() % nMX)) * 1000;

    // ----------------------------------------------------------------------
    // See what they are looking for...
    // ----------------------------------------------------------------------

    if ((sST == "ssdp:all") || (sST == "upnp:rootdevice"))
    {
        UPnpSearchTask *pTask = new UPnpSearchTask( m_nServicePort, 
            peerAddress, peerPort, sST, 
            UPnp::g_UPnpDeviceDesc.m_rootDevice.GetUDN());

        // Excute task now for fastest response, queue for time-delayed response
        // -=>TODO: To be trully uPnp compliant, this Execute should be removed.
        //pTask->Execute( NULL );

        TaskQueue::Instance()->AddTask( nNewMX, pTask );

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

    VERBOSE( VB_UPNP+VB_EXTRA,
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

    VERBOSE( VB_UPNP+VB_EXTRA,
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

bool SSDPExtension::ProcessRequest( HttpWorkerThread *, HTTPRequest *pRequest )
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

    VERBOSE( VB_UPNP, "SSDPExtension::GetDeviceDesc - "
                       + QString( "Host=%1 Port=%2 UserAgent=%3" )
                         .arg( pRequest->GetHostAddress() )
                         .arg( m_nServicePort )
                         .arg( sUserAgent ));

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
        VERBOSE( VB_UPNP, QString( "SSDPExtension::GetFile( %1 ) - Exists" )
                             .arg( pRequest->m_sFileName ));

        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        pRequest->m_mapRespHeaders[ "Cache-Control" ]
            = "no-cache=\"Ext\", max-age = 5000";
    }
    else
    {
        VERBOSE( VB_UPNP, QString( "SSDPExtension::GetFile( %1 ) - Not Found" )
                             .arg( pRequest->m_sFileName ));
    }

}

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

void SSDPExtension::GetDeviceList( HTTPRequest *pRequest )
{
    SSDPCache*    pCache  = SSDPCache::Instance();
    int           nCount = 0;
    NameValues    list;

    VERBOSE( VB_UPNP, "SSDPExtension::GetDeviceList" );

    pCache->Lock();

    QString     sXML = "";
    QTextStream os( &sXML, QIODevice::WriteOnly );

    for (SSDPCacheEntriesMap::Iterator it  = pCache->Begin();
                                       it != pCache->End();
                                     ++it )
    {
        SSDPCacheEntries *pEntries = *it;

        if (pEntries != NULL)
        {
            os << "<Device uri='" << it.key() << "'>" << endl;

            pEntries->Lock();

            EntryMap *pMap = pEntries->GetEntryMap();

            for (EntryMap::Iterator itEntry  = pMap->begin();
                                    itEntry != pMap->end();
                                  ++itEntry )
            {

                DeviceLocation *pEntry = *itEntry;

                if (pEntry != NULL)
                {
                    nCount++;

                    pEntry->AddRef();

                    os << "<Service usn='" << pEntry->m_sUSN 
                       << "' expiresInSecs='" << pEntry->ExpiresInSecs()
                       << "' url='" << pEntry->m_sLocation << "' />" << endl;

                    pEntry->Release();
                }
            }

            os << "</Device>" << endl;

            pEntries->Unlock();
        }
    }
    os << flush;

    list.push_back( NameValue("DeviceCount"          , pCache->Count()               ));
    list.push_back( NameValue("DevicesAllocated"     , SSDPCacheEntries::g_nAllocated));
    list.push_back( NameValue("CacheEntriesFound"    , nCount                        ));
    list.push_back( NameValue("CacheEntriesAllocated", DeviceLocation::g_nAllocated  ));
    list.push_back( NameValue("DeviceList"           , sXML                          ));

    pCache->Unlock();

    pRequest->FormatActionResponse( list );

    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 200;
}
