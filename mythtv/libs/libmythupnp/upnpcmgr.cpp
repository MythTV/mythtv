//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCMGR.cpp
// Created     : Dec. 28, 2006
//
// Purpose     : uPnp Connection Manager Service 
//                                                                            
// Copyright (c) 2006 David Blain <mythtv@theblains.net>
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
#include "upnpcmgr.h"
#include "mythverbose.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCMGR::UPnpCMGR ( UPnpDevice *pDevice, 
                     const QString &sSharePath,
                     const QString &sSourceProtocols, 
                     const QString &sSinkProtocols ) 
         : Eventing( "UPnpCMGR", "CMGR_Event", sSharePath)
{
    AddVariable( new StateVariable< QString >( "SourceProtocolInfo"  , true ) );
    AddVariable( new StateVariable< QString >( "SinkProtocolInfo"    , true ) );
    AddVariable( new StateVariable< QString >( "CurrentConnectionIDs", true ) );

    SetValue< QString >( "CurrentConnectionIDs", "0" );
    SetValue< QString >( "SourceProtocolInfo"  , sSourceProtocols );
    SetValue< QString >( "SinkProtocolInfo"    , sSinkProtocols   );

    QString sUPnpDescPath = UPnp::g_pConfig->GetValue( "UPnP/DescXmlPath",
                                                       m_sSharePath );
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
        sValue += ',';

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
        UPnp::FormatErrorResponse( pRequest, UPnPResult_CMGR_InvalidConnectionRef );
        return;
    }

    NameValues list;

    list.push_back(NameValue( "RcsID"                , "-1"             ));
    list.push_back(NameValue( "AVTransportID"        , "-1"             ));
    list.push_back(NameValue( "ProtocolInfo"         , "http-get:*:*:*" ));
    list.push_back(NameValue( "PeerConnectionManager", "/"              ));
    list.push_back(NameValue( "PeerConnectionID"     , "-1"             ));
    list.push_back(NameValue( "Direction"            , "Output"         ));
    list.push_back(NameValue( "Status"               , "Unknown"        ));
    
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
