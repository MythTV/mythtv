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

#include "upnptasknotify.h"
#include "upnptasksearch.h"
#include "upnptaskcache.h"

#include "mythcontext.h"

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

SSDP::SSDP( QSocketDevice *pSocket, bool bEnableNotify ) : m_bTermRequested( false )
{
    m_pSSDPSocket   = pSocket;
    m_bEnableNotify = bEnableNotify;
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

void SSDP::run()
{
    BufferedSocketDevice *pSocket          = new BufferedSocketDevice( m_pSSDPSocket );
    UPnpNotifyTask       *pNotifyTask      = NULL;
    HTTPRequest          *pRequest         = NULL;

    //-=>TODO: Should add checks for NULL pointers?

    if (m_bEnableNotify)
    {
        pNotifyTask = new UPnpNotifyTask(); 

        // ------------------------------------------------------------------
        // Let's make sure to hold on to a reference of the NotifyTask.
        // ------------------------------------------------------------------

        pNotifyTask->AddRef();

        // ------------------------------------------------------------------
        // First Send out Notification that we are leaving the network.
        // ------------------------------------------------------------------

        pNotifyTask->SetNTS( NTS_byebye );
        pNotifyTask->Execute( NULL );

        // ------------------------------------------------------------------
        // Add Announcement Task to the Queue
        // ------------------------------------------------------------------

        pNotifyTask->SetNTS( NTS_alive );
        UPnp::g_pTaskQueue->AddTask( pNotifyTask  );

    }

    // ----------------------------------------------------------------------
    // Listen for new Requests
    // ----------------------------------------------------------------------

    while (!IsTermRequested())
    {
        if ( pSocket->WaitForMore( 500 ) > 0)
        {
            QHostAddress  peerAddress = pSocket->PeerAddress();
            Q_UINT16      peerPort    = pSocket->PeerPort   ();

            pSocket->SetDestAddress( peerAddress, peerPort );

            // --------------------------------------------------------------
            // See if this is a valid request
            // --------------------------------------------------------------
            if ((pRequest = new BufferedSocketDeviceRequest( pSocket )) != NULL)
            {

                if ( pRequest->ParseRequest() )
                {
                    switch( pRequest->m_eType )
                    {
                        case  RequestTypeMSearch:  ProcessSearchRequest( pRequest, peerAddress, peerPort ); break;
                        case  RequestTypeNotify :  ProcessNotify       ( pRequest                        ); break;
                        default: break;
                    }
                }
                else 
                {
                    VERBOSE(VB_UPNP, "No info from ParseRequest... Clearing");
                    pSocket->ClearReadBuffer();
                }  

                delete pRequest;
                pRequest = NULL;
            }
        }
    }

    if (pNotifyTask != NULL)
    {
        // Send Announcement that we are leaving.

        pNotifyTask->SetNTS( NTS_byebye );
        pNotifyTask->Execute( NULL );
        pNotifyTask->Release();
    }

    if (pSocket != NULL) 
        delete pSocket;

    if (m_pSSDPSocket != NULL)
    {
        delete m_pSSDPSocket;
        m_pSSDPSocket = NULL;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SSDP::ProcessSearchRequest( HTTPRequest  *pRequest,
                                 QHostAddress  peerAddress,
                                 Q_UINT16      peerPort )
{
    QString sMAN = pRequest->GetHeaderValue( "MAN", "" );
    QString sST  = pRequest->GetHeaderValue(  "ST", "" );
    QString sMX  = pRequest->GetHeaderValue(  "MX", "" );
    int     nMX  = 0;

    //cout << "*** SSDP ProcessSearchrequest : [" << sST << "] MX = " << nMX << endl;

    // ----------------------------------------------------------------------
    // Validate Header Values...
    // ----------------------------------------------------------------------

    if ( UPnp::g_pTaskQueue    == NULL                ) return false;
    if ( pRequest->m_sMethod   != "*"                 ) return false;
    if ( pRequest->m_sProtocol != "HTTP"              ) return false;
    if ( pRequest->m_nMajor    != 1                   ) return false;
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
        UPnpSearchTask *pTask = new UPnpSearchTask( peerAddress,
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
        UPnpSearchTask *pTask = new UPnpSearchTask( peerAddress,
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

bool SSDP::ProcessNotify( HTTPRequest  *pRequest )
{
    QString sDescURL = pRequest->GetHeaderValue( "LOCATION"     , "" );
    QString sNTS     = pRequest->GetHeaderValue( "NTS"          , "" );
    QString sNT      = pRequest->GetHeaderValue( "NT"           , "" );
    QString sUSN     = pRequest->GetHeaderValue( "USN"          , "" );
    QString sCache   = pRequest->GetHeaderValue( "CACHE-CONTROL", "" );

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

SSDPExtension::SSDPExtension( ) : HttpServerExtension( "SSDP" )
{
    QString sSharePath = gContext->GetShareDir();

    m_sUPnpDescPath = gContext->GetSetting("upnpDescXmlPath", sSharePath);
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
    if (sURI == "getCMGRDesc"       ) return( SSDPM_GetCMGRDesc      );
    if (sURI == "getCDSDesc"        ) return( SSDPM_GetCDSDesc       );
    if (sURI == "getMSRRDesc"       ) return( SSDPM_GetMSRRDesc      );
    if (sURI == "getMythProtoDesc"  ) return( SSDPM_GetMythProtoDesc );
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
            case SSDPM_GetCDSDesc       : GetFile( pRequest, "CDS_scpd.xml"  ); return( true );
            case SSDPM_GetCMGRDesc      : GetFile( pRequest, "CMGR_scpd.xml" ); return( true );
            case SSDPM_GetMSRRDesc      : GetFile( pRequest, "MSRR_scpd.xml" ); return( true );
            case SSDPM_GetMythProtoDesc : GetFile( pRequest, "MXML_scpd.xml" ); return( true );
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
// Tempory Method to help debug/test the SSDPCache.
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

                SSDPCacheEntry *pEntry = (SSDPCacheEntry *)itEntry.data();

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
    list.append( new NameValue( "CacheEntriesAllocated", QString::number( SSDPCacheEntry::g_nAllocated )));

    list.append( new NameValue( "DeviceList"           , sXML));

    cache.Unlock();

    pRequest->FormatActionReponse( &list );

    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 200;

}
