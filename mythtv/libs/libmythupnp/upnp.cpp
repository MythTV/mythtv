//////////////////////////////////////////////////////////////////////////////
// Program Name: upnp.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Main Class
//
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
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

#include "upnptaskcache.h"
#include "multicast.h"
#include "broadcast.h"

//////////////////////////////////////////////////////////////////////////////
// Global/Class Static variables
//////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc   UPnp::g_UPnpDeviceDesc;
QStringList      UPnp::g_IPAddrList;

Configuration   *UPnp::g_pConfig        = NULL;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnp Class implementaion
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnp::UPnp()
    : m_pHttpServer(NULL), m_nServicePort(0)
{
    VERBOSE( VB_UPNP, "UPnp - Constructor" );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnp::~UPnp()
{
    VERBOSE( VB_UPNP, "UPnp - Destructor" );
    CleanUp();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::SetConfiguration( Configuration *pConfig )
{
    if (g_pConfig)
        delete g_pConfig;

    g_pConfig = pConfig;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

Configuration *UPnp::GetConfiguration()
{
    // If someone is asking for a config and it's NULL, create a 
    // new XmlConfiguration since we don't have database info yet.
    
    if (g_pConfig == NULL)
        g_pConfig = new XmlConfiguration( "config.xml" );

    return g_pConfig;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool UPnp::Initialize( int nServicePort, HttpServer *pHttpServer )
{
    QStringList sList;

    GetIPAddressList( sList );

    return Initialize( sList, nServicePort, pHttpServer );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool UPnp::Initialize( QStringList &sIPAddrList, int nServicePort, HttpServer *pHttpServer )
{
    VERBOSE(VB_UPNP, "UPnp::Initialize - Begin");

    if (g_pConfig == NULL)
    {
        VERBOSE(VB_IMPORTANT, "UPnp::Initialize - Must call SetConfiguration.");
        return false;
    }

    if ((m_pHttpServer = pHttpServer) == NULL)
    {
        VERBOSE(VB_IMPORTANT,
                "UPnp::Initialize - Invalid Parameter (pHttpServer == NULL)");
        return false;
    }

    g_IPAddrList   = sIPAddrList;
    m_nServicePort = nServicePort;

    // ----------------------------------------------------------------------
    // Register any HttpServerExtensions
    // ----------------------------------------------------------------------

    m_pHttpServer->RegisterExtension(
            new SSDPExtension( m_nServicePort, m_pHttpServer->m_sSharePath));

    VERBOSE(VB_UPNP, "UPnp::Initialize - End");

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Delay startup of Discovery Threads until all Extensions are registered.
//////////////////////////////////////////////////////////////////////////////

void UPnp::Start()
{
    VERBOSE(VB_UPNP, "UPnp::Start - Enabling SSDP Notifications");
    // ----------------------------------------------------------------------
    // Turn on Device Announcements 
    // (this will also create/startup SSDP if not already done)
    // ----------------------------------------------------------------------

    SSDP::Instance()->EnableNotifications( m_nServicePort );

    VERBOSE(VB_UPNP, "UPnp::Start - Returning");
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::CleanUp()
{
    VERBOSE(VB_UPNP, "UPnp::CleanUp() - disabling SSDP notifications");

    SSDP::Instance()->DisableNotifications();

    if (g_pConfig)
    {
        delete g_pConfig;
        g_pConfig = NULL;
    }

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc *UPnp::GetDeviceDesc( QString &sURL, bool bInQtThread )
{
    return UPnpDeviceDesc::Retrieve( sURL, bInQtThread );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString UPnp::GetResultDesc( UPnPResultCode eCode )
{
    switch( eCode )
    {
        case UPnPResult_Success                     : return "Success";
        case UPnPResult_InvalidAction               : return "Invalid Action";
        case UPnPResult_InvalidArgs                 : return "Invalid Args";
        case UPnPResult_ActionFailed                : return "Action Failed";
        case UPnPResult_ArgumentValueInvalid        : return "Argument Value Invalid";
        case UPnPResult_ArgumentValueOutOfRange     : return "Argument Value Out Of Range";
        case UPnPResult_OptionalActionNotImplemented: return "Optional Action Not Implemented";
        case UPnPResult_OutOfMemory                 : return "Out Of Memory";
        case UPnPResult_HumanInterventionRequired   : return "Human Intervention Required";
        case UPnPResult_StringArgumentTooLong       : return "String Argument Too Long";
        case UPnPResult_ActionNotAuthorized         : return "Action Not Authorized";
        case UPnPResult_SignatureFailure            : return "Signature Failure";
        case UPnPResult_SignatureMissing            : return "Signature Missing";
        case UPnPResult_NotEncrypted                : return "Not Encrypted";
        case UPnPResult_InvalidSequence             : return "Invalid Sequence";
        case UPnPResult_InvalidControlURL           : return "Invalid Control URL";
        case UPnPResult_NoSuchSession               : return "No Such Session";
        case UPnPResult_MS_AccessDenied             : return "Access Denied";

        case UPnPResult_CDS_NoSuchObject            : return "No Such Object";
        case UPnPResult_CDS_InvalidCurrentTagValue  : return "Invalid CurrentTagValue";
        case UPnPResult_CDS_InvalidNewTagValue      : return "Invalid NewTagValue";
        case UPnPResult_CDS_RequiredTag             : return "Required Tag";
        case UPnPResult_CDS_ReadOnlyTag             : return "Read Only Tag";
        case UPnPResult_CDS_ParameterMismatch       : return "Parameter Mismatch";
        case UPnPResult_CDS_InvalidSearchCriteria   : return "Invalid Search Criteria";
        case UPnPResult_CDS_InvalidSortCriteria     : return "Invalid Sort Criteria";
        case UPnPResult_CDS_NoSuchContainer         : return "No Such Container";
        case UPnPResult_CDS_RestrictedObject        : return "Restricted Object";
        case UPnPResult_CDS_BadMetadata             : return "Bad Metadata";
        case UPnPResult_CDS_ResrtictedParentObject  : return "Resrticted Parent Object";
        case UPnPResult_CDS_NoSuchSourceResource    : return "No Such Source Resource";
        case UPnPResult_CDS_ResourceAccessDenied    : return "Resource Access Denied";
        case UPnPResult_CDS_TransferBusy            : return "Transfer Busy";
        case UPnPResult_CDS_NoSuchFileTransfer      : return "No Such File Transfer";
        case UPnPResult_CDS_NoSuchDestRes           : return "No Such Destination Resource";
        case UPnPResult_CDS_DestResAccessDenied     : return "Destination Resource Access Denied";
        case UPnPResult_CDS_CannotProcessRequest    : return "Cannot Process The Request";

        //case UPnPResult_CMGR_IncompatibleProtocol     = 701,
        //case UPnPResult_CMGR_IncompatibleDirections   = 702,
        //case UPnPResult_CMGR_InsufficientNetResources = 703,
        //case UPnPResult_CMGR_LocalRestrictions        = 704,
        //case UPnPResult_CMGR_AccessDenied             = 705,
        //case UPnPResult_CMGR_InvalidConnectionRef     = 706,
        case UPnPResult_CMGR_NotInNetwork           : return "Not In Network";

    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::FormatErrorResponse( HTTPRequest   *pRequest,
                                UPnPResultCode  eCode,
                                const QString &msg )
{
    QString sMsg( msg );

    if (pRequest != NULL)
    {
        QString sDetails = "";

        if (pRequest->m_bSOAPRequest)
            sDetails = "<UPnPResult xmlns=\"urn:schemas-upnp-org:control-1-0\">";

        if (sMsg.length() == 0)
            sMsg = GetResultDesc( eCode );

        sDetails += QString( "<errorCode>%1</errorCode>"
                             "<errorDescription>%2</errorDescription>" )
                       .arg( eCode )
                       .arg( HTTPRequest::Encode( sMsg ) );

        if (pRequest->m_bSOAPRequest)
            sDetails += "</UPnPResult>";


        pRequest->FormatErrorResponse ( true,   // -=>TODO: Should make this dynamic
                                        "UPnPResult",
                                        sDetails );
    }
    else
        VERBOSE( VB_IMPORTANT, "UPnp::FormatErrorResponse : Response not created - pRequest == NULL" );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::FormatRedirectResponse( HTTPRequest   *pRequest,
                                   const QString &hostName )
{
    pRequest->m_eResponseType     = ResponseTypeOther;
    pRequest->m_nResponseStatus   = 301;

    QStringList sItems = pRequest->m_sRawRequest.split( ' ' );

    QString sUrl = "http://" + pRequest->m_mapHeaders[ "host" ] + sItems[1];

    QUrl url( sUrl );

    url.setHost( hostName );

    pRequest->m_mapRespHeaders[ "Location" ] = url.toString();

    VERBOSE( VB_UPNP, QString( "Sending http redirect to: %1").arg( url.toString() ) );

    pRequest->SendResponse();
}
