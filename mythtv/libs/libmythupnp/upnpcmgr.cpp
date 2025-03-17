//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCMGR.cpp
// Created     : Dec. 28, 2006
//
// Purpose     : uPnp Connection Manager Service 
//                                                                            
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnpcmgr.h"

#include "libmythbase/configuration.h"
#include "libmythbase/mythlogging.h"

#include "upnp.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCMGR::UPnpCMGR ( UPnpDevice *pDevice, 
                     const QString &sSharePath,
                     const QString &sSourceProtocols, 
                     const QString &sSinkProtocols ) 
         : Eventing( "UPnpCMGR", "CMGR_Event", sSharePath)
         , m_sControlUrl("/CMGR_Control")
{
    AddVariable( new StateVariable< QString >( "SourceProtocolInfo"  , true ) );
    AddVariable( new StateVariable< QString >( "SinkProtocolInfo"    , true ) );
    AddVariable( new StateVariable< QString >( "CurrentConnectionIDs", true ) );
    AddVariable( new StateVariable< QString >( "FeatureList"         , true ) );

    SetValue< QString >( "CurrentConnectionIDs", "0" );
    SetValue< QString >( "SourceProtocolInfo"  , sSourceProtocols );
    SetValue< QString >( "SinkProtocolInfo"    , sSinkProtocols   );
    SetValue< QString >( "FeatureList"         , "" );

    QString sUPnpDescPath = XmlConfiguration().GetValue( "UPnP/DescXmlPath", m_sSharePath);
    m_sServiceDescFileName = sUPnpDescPath + "CMGR_scpd.xml";

    // Add our Service Definition to the device.

    RegisterService( pDevice );

    // ConnectionManager uses a different schema definition for the FeatureList
    // to the ContentDirectoryService
    m_features.AddAttribute(NameValue( "xmlns",
                                       "urn:schemas-upnp-org:av:cm-featureList" ));
    m_features.AddAttribute(NameValue( "xmlns:xsi",
                                       "http://www.w3.org/2001/XMLSchema-instance" ));
    m_features.AddAttribute(NameValue( "xsi:schemaLocation",
                                       "urn:schemas-upnp-org:av:cm-featureList "
                                       "http://www.upnp.org/schemas/av/cm-featureList.xsd" ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::AddSourceProtocol( const QString &sProtocol )
{
    auto sValue = GetValue< QString >( "SourceProtocolInfo" );
    
    if (sValue.length() > 0 )
        sValue += ',';

    sValue += sProtocol;

    SetValue< QString >( "SourceProtocolInfo", sValue );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::AddSinkProtocol( const QString &sProtocol )
{
    auto sValue = GetValue< QString >( "SinkProtocolInfo" );
    
    if (sValue.length() > 0 )
        sValue += ',';

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
    if (sURI == "GetFeatureList"           ) return CMGRM_GetFeatureList ;

    return CMGRM_Unknown;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList UPnpCMGR::GetBasePaths() 
{ 
    return Eventing::GetBasePaths() << m_sControlUrl; 
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCMGR::ProcessRequest( HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if (Eventing::ProcessRequest( pRequest ))
            return true;

        if ( pRequest->m_sBaseUrl != m_sControlUrl )
        {
#if 0
            LOG(VB_UPNP, LOG_DEBUG,
                QString("UPnpCMGR::ProcessRequest - BaseUrl (%1) not ours...")
                    .arg(pRequest->m_sBaseUrl));
#endif
            return false;
        }

        LOG(VB_UPNP, LOG_INFO, 
            QString("UPnpCMGR::ProcessRequest - Method (%1)")
                .arg(pRequest->m_sMethod));

        switch( GetMethod( pRequest->m_sMethod ) )
        {
            case CMGRM_GetServiceDescription   :
                pRequest->FormatFileResponse( m_sServiceDescFileName );
                break;
            case CMGRM_GetProtocolInfo         :
                HandleGetProtocolInfo( pRequest );
                break;
            case CMGRM_GetCurrentConnectionInfo:
                HandleGetCurrentConnectionInfo( pRequest );
                break;
            case CMGRM_GetCurrentConnectionIDs :
                HandleGetCurrentConnectionIDs ( pRequest );
                break;
            case CMGRM_GetFeatureList :
                HandleGetFeatureList( pRequest );
                break;
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
    NameValues list;

    list.push_back(
        NameValue("Source", GetValue<QString>("SourceProtocolInfo")));
    list.push_back(
        NameValue("Sink",   GetValue<QString>("SinkProtocolInfo")));

    pRequest->FormatActionResponse(list);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::HandleGetCurrentConnectionInfo( HTTPRequest *pRequest )
{
    unsigned short nId = pRequest->m_mapParams[ "ConnectionID" ].toUShort();

    if ( nId != 0)
    {
        UPnp::FormatErrorResponse( pRequest,
                                   UPnPResult_CMGR_InvalidConnectionRef );
        return;
    }

    NameValues list;

    list.push_back(NameValue( "RcsID"                , "-1"             , true));
    list.push_back(NameValue( "AVTransportID"        , "-1"             , true));
    list.push_back(NameValue( "ProtocolInfo"         , "http-get:*:*:*" , true));
    list.push_back(NameValue( "PeerConnectionManager", "/"              , true));
    list.push_back(NameValue( "PeerConnectionID"     , "-1"             , true));
    list.push_back(NameValue( "Direction"            , "Output"         , true));
    list.push_back(NameValue( "Status"               , "Unknown"        , true));
    
    pRequest->FormatActionResponse(list);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCMGR::HandleGetCurrentConnectionIDs ( HTTPRequest *pRequest )
{
    NameValues list;

    list.push_back(
        NameValue("ConnectionIDs", GetValue<QString>("CurrentConnectionIDs")));

    pRequest->FormatActionResponse(list);
}

void UPnpCMGR::HandleGetFeatureList(HTTPRequest* pRequest)
{
    NameValues list;

    QString sResults = m_features.toXML();

    list.push_back(NameValue("FeatureList", sResults));

    pRequest->FormatActionResponse(list);
}
