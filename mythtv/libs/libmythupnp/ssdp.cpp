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

SSDP::SSDP() : m_bTermRequested( false )
{
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
    QMulticastSocket     *pMulticastSocket = new QMulticastSocket( SSDP_GROUP, SSDP_PORT );
    BufferedSocketDevice *pSocket          = new BufferedSocketDevice( pMulticastSocket  );
    UPnpNotifyTask       *pNotifyTask      = new UPnpNotifyTask(); 
    HTTPRequest          *pRequest         = NULL;

    //-=>TODO: Should add checks for NULL pointers?

    // ----------------------------------------------------------------------
    // Let's make sure to hold on to a reference of the NotifyTask.
    // ----------------------------------------------------------------------

    pNotifyTask->AddRef();

    // ----------------------------------------------------------------------
    // Add Announcement Task to the Queue
    // ----------------------------------------------------------------------
                             
    UPnp::g_pTaskQueue->AddTask( pNotifyTask  );

    // ----------------------------------------------------------------------
    // Listen for new Mutlicast Requests
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
                        default: break;
                    }
                }

                delete pRequest;
                pRequest = NULL;
            }
        }
    }

    // Send Announcement that we are leaving.

    pNotifyTask->SetNTS( NTS_byebye );
    pNotifyTask->Execute( NULL );
    pNotifyTask->Release();

    if (pSocket != NULL) 
        delete pSocket;

    if (pMulticastSocket != NULL)
        delete pMulticastSocket;
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
    m_sUPnpDescPath = gContext->GetSetting( "upnpDescXmlPath", "./" );
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
    if (sURI == "getDeviceDesc") return( SSDPM_GetDeviceDesc );
    if (sURI == "getCMGRDesc"  ) return( SSDPM_GetCMGRDesc   );
    if (sURI == "getCDSDesc"   ) return( SSDPM_GetCDSDesc    );

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
            case SSDPM_GetDeviceDesc: GetDeviceDesc( pRequest ); return( true );
            case SSDPM_GetCDSDesc   : GetFile( pRequest, "CDS_scpd.xml" );  return( true );
            case SSDPM_GetCMGRDesc  : GetFile( pRequest, "CMGR_scpd.xml" ); return( true );
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

    UPnp::g_UPnpDeviceDesc.GetValidXML( pRequest->GetHostAddress(), pRequest->m_response );
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
