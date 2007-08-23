//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdp.cpp
//                                                                            
// Purpose - SSDP Discovery Service Implmenetation
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnp.h"

#include "upnptasksearch.h"
#include "upnptaskcache.h"

#include "multicast.h"
#include "broadcast.h"

#include <qregexp.h>

#include <unistd.h>
#include <qstringlist.h>
#include <sys/time.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDP Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP::SSDP( int nServicePort ) : m_bTermRequested( false )
{

    m_nServicePort = nServicePort;
    m_nPort        = UPnp::g_pConfig->GetValue( "UPnP/SSDP/Port"      , SSDP_PORT       );
    m_nSearchPort  = UPnp::g_pConfig->GetValue( "UPnP/SSDP/SearchPort", SSDP_SEARCHPORT );

    m_Sockets[ SocketIdx_Search    ] = new QSocketDevice( QSocketDevice::Datagram ); 
    m_Sockets[ SocketIdx_Multicast ] = new QMulticastSocket( SSDP_GROUP, m_nPort );
    m_Sockets[ SocketIdx_Broadcast ] = new QBroadcastSocket( "255.255.255.255", m_nPort );


    m_Sockets[ SocketIdx_Search    ]->setBlocking( FALSE );
    m_Sockets[ SocketIdx_Multicast ]->setBlocking( FALSE );
    m_Sockets[ SocketIdx_Broadcast ]->setBlocking( FALSE );

    // Setup SearchSocket

    m_Sockets[ SocketIdx_Search ]->bind( INADDR_ANY, m_nSearchPort ); 

    m_pNotifyTask   = NULL;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDP::~SSDP()
{
    DisableNotifications();

    if (m_pNotifyTask != NULL)
        m_pNotifyTask->Release();

    for (int nIdx = 0; nIdx < (int)NumberOfSockets; nIdx++ )
    {
        if (m_Sockets[ nIdx ] != NULL )
        {
            delete m_Sockets[ nIdx ];
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SSDP::IsTermRequested()
{
    m_lock.lock();
    bool bTermRequested = m_bTermRequested;
    m_lock.unlock();

    return( bTermRequested );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::RequestTerminate(void)
{
    m_lock.lock();  
    m_bTermRequested = true; 
    m_lock.unlock();

    // Call wait to give thread time to terminate.

    wait( 500 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::EnableNotifications()
{
    if ( m_pNotifyTask == NULL )
    {
        m_pNotifyTask = new UPnpNotifyTask( m_nServicePort ); 

        // ------------------------------------------------------------------
        // Let's make sure to hold on to a reference of the NotifyTask.
        // ------------------------------------------------------------------

        m_pNotifyTask->AddRef();

        // ------------------------------------------------------------------
        // First Send out Notification that we are leaving the network.
        // ------------------------------------------------------------------

        m_pNotifyTask->SetNTS( NTS_byebye );
        m_pNotifyTask->Execute( NULL );
    }

    // ------------------------------------------------------------------
    // Add Announcement Task to the Queue
    // ------------------------------------------------------------------

    m_pNotifyTask->SetNTS( NTS_alive );
    UPnp::g_pTaskQueue->AddTask( m_pNotifyTask  );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::DisableNotifications()
{
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
    QCString sRequest = QString( "M-SEARCH * HTTP/1.1\r\n"
                                 "HOST: 239.255.255.250:1900\r\n"
                                 "MAN: \"ssdp:discover\"\r\n"
                                 "MX: 2\r\n"
                                 "ST: %1\r\n"
                                 "\r\n" )
                           .arg( sST ).utf8();

    QSocketDevice *pSocket = m_Sockets[ SocketIdx_Search ];

    QHostAddress address;
    address.setAddress( SSDP_GROUP );

    int nSize = sRequest.size();

    if ( pSocket->writeBlock( sRequest.data(), sRequest.size(), address, SSDP_PORT ) != nSize)
        cerr << "SSDP::PerformSearch - did not write entire buffer." << endl;

    usleep( rand() % 250000 );

    if ( pSocket->writeBlock( sRequest.data(), sRequest.size(), address, SSDP_PORT ) != nSize)
        cerr << "SSDP::PerformSearch - did not write entire buffer." << endl;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::run()
{
    fd_set          read_set;
    struct timeval  timeout;

    // ----------------------------------------------------------------------
    // Listen for new Requests
    // ----------------------------------------------------------------------

    while (!IsTermRequested())
    {
        int nMaxSocket = 0;

        FD_ZERO( &read_set );

        for (int nIdx = 0; nIdx < (int)NumberOfSockets; nIdx++ )
        {
            if (m_Sockets[ nIdx ] != NULL)
            {
                FD_SET( m_Sockets[ nIdx ]->socket(), &read_set );
                nMaxSocket = max( m_Sockets[ nIdx ]->socket(), nMaxSocket );

/*
                if (m_Sockets[ nIdx ]->bytesAvailable() > 0)
                {
	            cout << "Found Extra data before select: " << nIdx << endl;
                    ProcessData( m_Sockets[ nIdx ] );
                }
*/

            }
        }
        
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        if ( select( nMaxSocket + 1, &read_set, NULL, NULL, &timeout ) != -1)
        {
            for (int nIdx = 0; nIdx < (int)NumberOfSockets; nIdx++ )
            {
                if (m_Sockets[ nIdx ] != NULL)
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
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDP::ProcessData( QSocketDevice *pSocket )
{
    long nBytes = 0;
    long nRead  = 0;

    while ((nBytes = pSocket->bytesAvailable()) > 0)
    {
        QCString buffer( nBytes + 1 );

        nRead = pSocket->readBlock( buffer.data(), nBytes );

        QHostAddress  peerAddress = pSocket->peerAddress();
        Q_UINT16      peerPort    = pSocket->peerPort   ();

        // ------------------------------------------------------------------

        QStringList lines        = QStringList::split( "\r\n", buffer );
        QString     sRequestLine = lines[0];

        lines.pop_front();

        // ------------------------------------------------------------------
        // Parse request Type
        // ------------------------------------------------------------------

        // cout << "SSDP::ProcessData - requestLine: " << sRequestLine << endl;

        SSDPRequestType eType = ProcessRequestLine( sRequestLine );

        // ------------------------------------------------------------------
        // Read Headers into map
        // ------------------------------------------------------------------

        QStringMap  headers;

        for ( QStringList::Iterator it = lines.begin(); it != lines.end(); ++it ) 
        {
            QString sLine  = *it;
            QString sName  = sLine.section( ':', 0, 0 ).stripWhiteSpace();
            QString sValue = sLine.section( ':', 1 );

            sValue.truncate( sValue.length() );  //-2

            if ((sName.length() != 0) && (sValue.length() !=0))
                headers.insert( sName.lower(), sValue.stripWhiteSpace() );
        }

//        pSocket->SetDestAddress( peerAddress, peerPort );

        // --------------------------------------------------------------
        // See if this is a valid request
        // --------------------------------------------------------------

        switch( eType )
        {
            case SSDP_MSearch    :  ProcessSearchRequest ( headers, peerAddress, peerPort ); break;
            case SSDP_MSearchResp:  ProcessSearchResponse( headers                        ); break;
            case SSDP_Notify     :  ProcessNotify        ( headers                        ); break;
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
    QStringList tokens = QStringList::split(QRegExp("[ \r\n][ \r\n]*"), sLine );

    // ----------------------------------------------------------------------
    // if this is actually a response, then sLine's format will be:
    //      HTTP/m.n <response code> <response text>
    // otherwise:
    //      <method> <Resource URI> HTTP/m.n
    // ----------------------------------------------------------------------

    if ( sLine.startsWith( "HTTP/" )) 
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

QString SSDP::GetHeaderValue( const QStringMap &headers, const QString &sKey, const QString &sDefault )
{
    QStringMap::const_iterator it = headers.find( sKey.lower() );

    if ( it == headers.end())
        return( sDefault );

    return( it.data() );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SSDP::ProcessSearchRequest( const QStringMap &sHeaders, 
                                 QHostAddress      peerAddress,
                                 Q_UINT16          peerPort )
{
    QString sMAN = GetHeaderValue( sHeaders, "MAN", "" );
    QString sST  = GetHeaderValue( sHeaders, "ST" , "" );
    QString sMX  = GetHeaderValue( sHeaders, "MX" , "" );
    int     nMX  = 0;

    //cout << "*** SSDP ProcessSearchrequest : [" << sST << "] MX = " << nMX << endl;

    // ----------------------------------------------------------------------
    // Validate Header Values...
    // ----------------------------------------------------------------------

    if ( UPnp::g_pTaskQueue    == NULL                ) return false;
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
                                                    peerAddress,
                                                    peerPort,
                                                    sST, 
                                   UPnp::g_UPnpDeviceDesc.m_rootDevice.GetUDN());

        // Excute task now for fastest response & queue for time-delayed response
        // -=>TODO: To be trully uPnp compliant, this Execute should be removed.

        pTask->Execute( NULL );

        UPnp::g_pTaskQueue->AddTask( nNewMX, pTask );

        return true;
    }

    // ----------------------------------------------------------------------
    // Look for a specific device/service
    // ----------------------------------------------------------------------

    QString sUDN = UPnp::g_UPnpDeviceDesc.FindDeviceUDN( &(UPnp::g_UPnpDeviceDesc.m_rootDevice), sST );

    if (sUDN.length() > 0)
    {
        UPnpSearchTask *pTask = new UPnpSearchTask( m_nServicePort,
                                                    peerAddress,
                                                    peerPort,
                                                    sST, 
                                                    sUDN );

        // Excute task now for fastest response & queue for time-delayed response
        // -=>TODO: To be trully uPnp compliant, this Execute should be removed.

        pTask->Execute( NULL );

        UPnp::g_pTaskQueue->AddTask( nNewMX, pTask );

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

    int nPos = sCache.find( "max-age", 0, false );

    if (nPos < 0)
        return false;

    if ((nPos = sCache.find( "=", nPos, false )) < 0)
        return false;

    int nSecs = sCache.mid( nPos+1 ).toInt();

    UPnp::g_SSDPCache.Add( sST, sUSN, sDescURL, nSecs );

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

    if (sNTS.contains( "ssdp:alive"))
    {
        int nPos = sCache.find( "max-age", 0, false );

        if (nPos < 0)
            return false;

        if ((nPos = sCache.find( "=", nPos, false )) < 0)
            return false;

        int nSecs = sCache.mid( nPos+1 ).toInt();

        UPnp::g_SSDPCache.Add( sNT, sUSN, sDescURL, nSecs );

        return true;
    }


    if ( sNTS.contains( "ssdp:byebye" ) )
    {
        UPnp::g_SSDPCache.Remove( sNT, sUSN );

        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPExtension Implemenation
// 
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPExtension::SSDPExtension( int nServicePort ) : HttpServerExtension( "SSDP" )
{
    m_sUPnpDescPath = UPnp::g_pConfig->GetValue( "UPnP/DescXmlPath", m_sSharePath );
    m_nServicePort  = nServicePort;
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

bool SSDPExtension::ProcessRequest( HttpWorkerThread *, HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if ( pRequest->m_sBaseUrl != "/")
            return( false );

        switch( GetMethod( pRequest->m_sMethod ))
        {
            case SSDPM_GetDeviceDesc    : GetDeviceDesc( pRequest ); return( true );
            case SSDPM_GetDeviceList    : GetDeviceList( pRequest ); return( true );

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

    UPnp::g_UPnpDeviceDesc.GetValidXML( pRequest->GetHostAddress(), 
                                        m_nServicePort,
                                        pRequest->m_response,
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

        pRequest->m_eResponseType                     = ResponseTypeFile;
        pRequest->m_nResponseStatus                   = 200;
        pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    }

}

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

void SSDPExtension::GetDeviceList( HTTPRequest *pRequest )
{
    SSDPCache    &cache  = UPnp::g_SSDPCache;
    int           nCount = 0;
    NameValueList list;

    cache.Lock();

    QString     sXML;
    QTextStream os( sXML, IO_WriteOnly );

    for (SSDPCacheEntriesMap::Iterator it  = cache.Begin();
                                       it != cache.End();
                                     ++it )
    {
        SSDPCacheEntries *pEntries = (SSDPCacheEntries *)it.data();

        if (pEntries != NULL)
        {
            os << "<Device uri='" << it.key() << "'>" << endl;

            pEntries->Lock();

            EntryMap *pMap = pEntries->GetEntryMap();

            for (EntryMap::Iterator itEntry  = pMap->begin();
                                    itEntry != pMap->end();
                                  ++itEntry )
            {

                DeviceLocation *pEntry = (DeviceLocation *)itEntry.data();

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

    list.append( new NameValue( "DeviceCount"          , QString::number( cache.Count() )));
    list.append( new NameValue( "DevicesAllocated"     , QString::number( SSDPCacheEntries::g_nAllocated )));

    list.append( new NameValue( "CacheEntriesFound"    , QString::number( nCount )));
    list.append( new NameValue( "CacheEntriesAllocated", QString::number( DeviceLocation::g_nAllocated )));

    list.append( new NameValue( "DeviceList"           , sXML));

    cache.Unlock();

    pRequest->FormatActionResponse( &list );

    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 200;

}
