//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCMGR.cpp
//                                                                            
// Purpose - uPnp Connection Manager Service 
//                                                                            
// Created By  : David Blain                    Created On : Dec. 28, 2006
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnp.h"
#include "upnpcmgr.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCMGR::UPnpCMGR ( UPnpDevice *pDevice, 
                     const QString &sSourceProtocols, 
                     const QString &sSinkProtocols ) 
         : Eventing( "UPnpCMGR", "CMGR_Event" )
{
    AddVariable( new StateVariable< QString >( "SourceProtocolInfo"  , true ) );
    AddVariable( new StateVariable< QString >( "SinkProtocolInfo"    , true ) );
    AddVariable( new StateVariable< QString >( "CurrentConnectionIDs", true ) );

    SetValue< QString >( "CurrentConnectionIDs", "0" );
    SetValue< QString >( "SourceProtocolInfo"  , sSourceProtocols );
    SetValue< QString >( "SinkProtocolInfo"    , sSinkProtocols   );

    QString sUPnpDescPath = UPnp::g_pConfig->GetValue( "UPnP/DescXmlPath", m_sSharePath );

    m_sServiceDescFileName = sUPnpDescPath + "CMGR_scpd.xml";
    m_sControlUrl          = "/CMGR_Control";

    // Add our Service Definition to the device.

    RegisterService( pDevice );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCMGR::~UPnpCMGR()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::AddSourceProtocol( const QString &sProtocol )
{
    QString sValue = GetValue< QString >( "SourceProtocolInfo" );
    
    if (sValue.length() > 0 )
        sValue += ",";

    sValue += sProtocol;

    SetValue< QString >( "SourceProtocolInfo", sValue );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::AddSinkProtocol( const QString &sProtocol )
{
    QString sValue = GetValue< QString >( "SinkProtocolInfo" );
    
    if (sValue.length() > 0 )
        sValue += ",";

    sValue += sProtocol;

    SetValue< QString >( "SinkProtocolInfo", sValue );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCMGRMethod UPnpCMGR::GetMethod( const QString &sURI )
{                        
    if (sURI == "GetServDesc"              ) return CMGRM_GetServiceDescription   ;
    if (sURI == "GetProtocolInfo"          ) return CMGRM_GetProtocolInfo         ;
    if (sURI == "GetCurrentConnectionInfo" ) return CMGRM_GetCurrentConnectionInfo;              
    if (sURI == "GetCurrentConnectionIDs"  ) return CMGRM_GetCurrentConnectionIDs ; 

    return CMGRM_Unknown;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCMGR::ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if (Eventing::ProcessRequest( pThread, pRequest ))
            return true;

        if ( pRequest->m_sBaseUrl != m_sControlUrl )
        {
//            VERBOSE( VB_UPNP, QString("UPnpCMGR::ProcessRequest - BaseUrl (%1) not ours...").arg(pRequest->m_sBaseUrl ));
            return false;
        }

        VERBOSE( VB_UPNP, QString("UPnpCMGR::ProcessRequest - Method (%1)").arg(pRequest->m_sMethod ));

        switch( GetMethod( pRequest->m_sMethod ) )
        {
            case CMGRM_GetServiceDescription   : pRequest->FormatFileResponse  ( m_sServiceDescFileName ); break;
            case CMGRM_GetProtocolInfo         : HandleGetProtocolInfo         ( pRequest ); break;
            case CMGRM_GetCurrentConnectionInfo: HandleGetCurrentConnectionInfo( pRequest ); break;
            case CMGRM_GetCurrentConnectionIDs : HandleGetCurrentConnectionIDs ( pRequest ); break;
            default:
                UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );
                break;
        }       
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::HandleGetProtocolInfo( HTTPRequest *pRequest )
{
    NameValueList list;

    list.append( new NameValue( "Source", GetValue< QString >( "SourceProtocolInfo")));
    list.append( new NameValue( "Sink"  , GetValue< QString >( "SinkProtocolInfo"  )));

    pRequest->FormatActionResponse( &list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::HandleGetCurrentConnectionInfo( HTTPRequest *pRequest )
{
    unsigned short nId = pRequest->m_mapParams[ "ConnectionID" ].toUShort();

    if ( nId != 0)
    {
        UPnp::FormatErrorResponse( pRequest, UPnPResult_CMGR_InvalidConnectionRef );
        return;
    }

    NameValueList list;

    list.append( new NameValue( "RcsID"                , "-1"             ));
    list.append( new NameValue( "AVTransportID"        , "-1"             ));
    list.append( new NameValue( "ProtocolInfo"         , "http-get:*:*:*" ));
    list.append( new NameValue( "PeerConnectionManager", "/"              ));
    list.append( new NameValue( "PeerConnectionID"     , "-1"             ));
    list.append( new NameValue( "Direction"            , "Output"         ));
    list.append( new NameValue( "Status"               , "Unknown"        ));
    
    pRequest->FormatActionResponse( &list );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::HandleGetCurrentConnectionIDs ( HTTPRequest *pRequest )
{
    NameValueList list;

    list.append( new NameValue( "ConnectionIDs", GetValue< QString >( "CurrentConnectionIDs" )));

    pRequest->FormatActionResponse( &list );

}
