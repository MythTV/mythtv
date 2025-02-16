//////////////////////////////////////////////////////////////////////////////
// Program Name: upnp.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Main Class
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnp.h"

#include <QNetworkInterface>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/serverpool.h"

#include "ssdp.h"

//////////////////////////////////////////////////////////////////////////////
// Global/Class Static variables
//////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc      UPnp::g_UPnpDeviceDesc;
QList<QHostAddress> UPnp::g_IPAddrList;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnp Class implementaion
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

UPnp::UPnp()
{
    LOG(VB_UPNP, LOG_DEBUG, "UPnp - Constructor");
    // N.B. Ask for 5 second delay to send Bye Bye twice
    // TODO Check whether we actually send Bye Bye twice:)
    m_power = MythPower::AcquireRelease(this, true, 5s);
    if (m_power)
    {
        // NB We only listen for WillXXX signals which should give us time to send notifications
        connect(m_power, &MythPower::WillRestart,  this, &UPnp::DisableNotifications);
        connect(m_power, &MythPower::WillSuspend,  this, &UPnp::DisableNotifications);
        connect(m_power, &MythPower::WillShutDown, this, &UPnp::DisableNotifications);
        connect(m_power, &MythPower::WokeUp,       this, &UPnp::EnableNotificatins);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnp::~UPnp()
{
    LOG(VB_UPNP, LOG_DEBUG, "UPnp - Destructor");
    CleanUp();
    if (m_power)
        MythPower::AcquireRelease(this, false);
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool UPnp::Initialize( int nServicePort, HttpServer *pHttpServer )
{
    QList<QHostAddress> sList = ServerPool::DefaultListen();
    if (sList.contains(QHostAddress(QHostAddress::AnyIPv4)))
    {
        sList.removeAll(QHostAddress(QHostAddress::AnyIPv4));
        sList.removeAll(QHostAddress(QHostAddress::AnyIPv6));
        sList.append(QNetworkInterface::allAddresses());
    }
    return Initialize( sList, nServicePort, pHttpServer );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool UPnp::Initialize( QList<QHostAddress> &sIPAddrList, int nServicePort, HttpServer *pHttpServer )
{
    LOG(VB_UPNP, LOG_DEBUG, "UPnp::Initialize - Begin");

    if (m_pHttpServer)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "UPnp::Initialize - Already initialized, programmer error.");
        return false;
    }

    m_pHttpServer = pHttpServer;
    if (m_pHttpServer == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "UPnp::Initialize - Invalid Parameter (pHttpServer == NULL)");
        return false;
    }

    g_IPAddrList   = sIPAddrList;
    bool ipv4 = gCoreContext->GetBoolSetting("IPv4Support",true);
    bool ipv6 = gCoreContext->GetBoolSetting("IPv6Support",true);

    for (int it = 0; it < g_IPAddrList.size(); ++it)
    {
        // If IPV4 support is disabled and this is an IPV4 address,
        // remove this address
        // If IPV6 support is disabled and this is an IPV6 address,
        // remove this address
        if ((g_IPAddrList[it].protocol() == QAbstractSocket::IPv4Protocol
                && ! ipv4)
          ||(g_IPAddrList[it].protocol() == QAbstractSocket::IPv6Protocol
                && ! ipv6))
            g_IPAddrList.removeAt(it--);
    }

    m_nServicePort = nServicePort;

    // ----------------------------------------------------------------------
    // Register any HttpServerExtensions
    // ----------------------------------------------------------------------

    m_pHttpServer->RegisterExtension(
        new SSDPExtension(m_nServicePort, m_pHttpServer->GetSharePath()));

    LOG(VB_UPNP, LOG_DEBUG, "UPnp::Initialize - End");

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Delay startup of Discovery Threads until all Extensions are registered.
//////////////////////////////////////////////////////////////////////////////

void UPnp::Start()
{
    LOG(VB_UPNP, LOG_DEBUG, "UPnp::Start - Enabling SSDP Notifications");
    // ----------------------------------------------------------------------
    // Turn on Device Announcements 
    // (this will also create/startup SSDP if not already done)
    // ----------------------------------------------------------------------

    SSDP::Instance()->EnableNotifications( m_nServicePort );

    LOG(VB_UPNP, LOG_DEBUG, "UPnp::Start - Returning");
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void UPnp::CleanUp()
{
    LOG(VB_UPNP, LOG_INFO, "UPnp::CleanUp() - disabling SSDP notifications");

    SSDP::Instance()->DisableNotifications();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc *UPnp::GetDeviceDesc( QString &sURL )
{
    return UPnpDeviceDesc::Retrieve( sURL );
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
        case UPnPResult_MythTV_NoNamespaceGiven:      return "No Namespace Given";
        case UPnPResult_MythTV_XmlParseError        : return "XML Parse Error";
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

    if (pRequest != nullptr)
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
    {
        LOG(VB_GENERAL, LOG_ERR, "Response not created - pRequest == NULL" );
    }
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
    QString sUrl = "http://" + pRequest->GetLastHeader( "host" ) + sItems[1];
    QUrl url( sUrl );
    QString ipAddress = gCoreContext->GetSettingOnHost
                            ("BackendServerAddr",hostName,hostName);
    url.setHost( ipAddress );

    pRequest->m_mapRespHeaders[ "Location" ] = url.toString();

    LOG(VB_UPNP, LOG_INFO, QString("Sending http redirect to: %1")
                               .arg(url.toString()));

    pRequest->SendResponse();
}

void UPnp::DisableNotifications(std::chrono::milliseconds /*unused*/)
{
    SSDP::Instance()->DisableNotifications();
}

void UPnp::EnableNotificatins(std::chrono::milliseconds /*unused*/) const
{
    SSDP::Instance()->EnableNotifications(m_nServicePort);
}
