//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcds.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : uPnp Content Directory Service
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnpcds.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "libmythbase/configuration.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"

#include "upnp.h"
#include "upnputil.h"

static constexpr const char* DIDL_LITE_BEGIN { R"(<DIDL-Lite xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/" xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/">)" };
static constexpr const char* DIDL_LITE_END   { "</DIDL-Lite>" };

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSExtensionResults::Add( CDSObject *pObject )
{
    if (pObject)
    {
        pObject->IncrRef();
        m_List.append( pObject );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSExtensionResults::Add( const CDSObjects& objects )
{
    for (auto *const object : std::as_const(objects))
    {
        object->IncrRef();
        m_List.append( object );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSExtensionResults::GetResultXML(FilterMap &filter,
                                              bool ignoreChildren)
{
    QString sXML;

    for (auto *item : std::as_const(m_List))
        sXML += item->toXml(filter, ignoreChildren);

    return sXML;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDS::UPnpCDS( UPnpDevice *pDevice, const QString &sSharePath )
  : Eventing( "UPnpCDS", "CDS_Event", sSharePath ),
    m_sControlUrl("/CDS_Control"),
    m_pShortCuts(new UPnPShortcutFeature())
{
    m_root.m_eType       = OT_Container;
    m_root.m_sId         = "0";
    m_root.m_sParentId   = "-1";
    m_root.m_sTitle      = "MythTV";
    m_root.m_sClass      = "object.container";
    m_root.m_bRestricted = true;
    m_root.m_bSearchable = true;

    AddVariable( new StateVariable< QString  >( "TransferIDs"       , true ) );
    AddVariable( new StateVariable< QString  >( "ContainerUpdateIDs", true ) );
    AddVariable( new StateVariable< uint16_t >( "SystemUpdateID"    , true ) );
    AddVariable( new StateVariable< QString  >( "ServiceResetToken" , true ) );

    SetValue< uint16_t >( "SystemUpdateID", 0 );
    // ServiceResetToken must be unique (never repeat) and it must change when
    // the backend restarts (all internal state is reset)
    //
    // The current date + time fits the criteria.
    SetValue< QString  >( "ServiceResetToken",
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODate) );

    QString sUPnpDescPath = XmlConfiguration().GetValue("UPnP/DescXmlPath", sSharePath);

    m_sServiceDescFileName = sUPnpDescPath + "CDS_scpd.xml";

    RegisterFeature(m_pShortCuts);

    // Add our Service Definition to the device.

    RegisterService( pDevice );

    // ContentDirectoryService uses a different schema definition for the FeatureList
    // to the ConnectionManager, although they appear to be identical
    m_features.AddAttribute(NameValue( "xmlns",
                                       "urn:schemas-upnp-org:av:avs" ));
    m_features.AddAttribute(NameValue( "xmlns:xsi",
                                       "http://www.w3.org/2001/XMLSchema-instance" ));
    m_features.AddAttribute(NameValue( "xsi:schemaLocation",
                                       "urn:schemas-upnp-org:av:avs "
                                       "http://www.upnp.org/schemas/av/avs.xsd" ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDS::~UPnpCDS()
{
    while (!m_extensions.isEmpty())
    {
        delete m_extensions.takeLast();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSMethod UPnpCDS::GetMethod( const QString &sURI )
{
    if (sURI == "GetServDesc"           ) return CDSM_GetServiceDescription;
    if (sURI == "Browse"                ) return CDSM_Browse               ;
    if (sURI == "Search"                ) return CDSM_Search               ;
    if (sURI == "GetSearchCapabilities" ) return CDSM_GetSearchCapabilities;
    if (sURI == "GetSortCapabilities"   ) return CDSM_GetSortCapabilities  ;
    if (sURI == "GetSystemUpdateID"     ) return CDSM_GetSystemUpdateID    ;
    if (sURI == "X_GetFeatureList" ||     // HACK: Samsung
        sURI == "GetFeatureList"        ) return CDSM_GetFeatureList       ;
    if (sURI == "GetServiceResetToken"  ) return CDSM_GetServiceResetToken ;

    return(  CDSM_Unknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSBrowseFlag UPnpCDS::GetBrowseFlag( const QString &sFlag )
{
    if (sFlag == "BrowseMetadata"       ) return( CDS_BrowseMetadata        );
    if (sFlag == "BrowseDirectChildren" ) return( CDS_BrowseDirectChildren  );

    return( CDS_BrowseUnknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::RegisterExtension  ( UPnpCDSExtension *pExtension )
{
    if (pExtension)
    {
        m_extensions.append( pExtension );

        CDSShortCutList shortcuts = pExtension->GetShortCuts();
        CDSShortCutList::iterator it;
        for (it = shortcuts.begin(); it != shortcuts.end(); ++it)
        {
            RegisterShortCut(it.key(), it.value());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::UnregisterExtension( UPnpCDSExtension *pExtension )
{
    if (pExtension)
    {
        m_extensions.removeAll(pExtension);
        delete pExtension;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::RegisterShortCut(UPnPShortcutFeature::ShortCutType type,
                               const QString& objectID)
{
    m_pShortCuts->AddShortCut(type, objectID);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::RegisterFeature(UPnPFeature* feature)
{
    m_features.AddFeature(feature); // m_features takes ownership
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList UPnpCDS::GetBasePaths()
{
    return Eventing::GetBasePaths() << m_sControlUrl;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDS::ProcessRequest( HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if (Eventing::ProcessRequest( pRequest ))
            return true;

        if ( pRequest->m_sBaseUrl != m_sControlUrl )
        {
#if 0
            LOG(VB_UPNP, LOG_DEBUG,
                QString("UPnpCDS::ProcessRequest - BaseUrl (%1) not ours...")
                    .arg(pRequest->m_sBaseUrl));
#endif
            return false;
        }

        switch( GetMethod( pRequest->m_sMethod ) )
        {
            case CDSM_GetServiceDescription :
                pRequest->FormatFileResponse( m_sServiceDescFileName );
                break;
            case CDSM_Browse                :
                HandleBrowse( pRequest );
                break;
            case CDSM_Search                :
                HandleSearch( pRequest );
                break;
            case CDSM_GetSearchCapabilities :
                HandleGetSearchCapabilities( pRequest );
                break;
            case CDSM_GetSortCapabilities   :
                HandleGetSortCapabilities( pRequest );
                break;
            case CDSM_GetSystemUpdateID     :
                HandleGetSystemUpdateID( pRequest );
                break;
            case CDSM_GetFeatureList        :
                HandleGetFeatureList( pRequest );
                break;
            case CDSM_GetServiceResetToken  :
                HandleGetServiceResetToken( pRequest );
                break;
            default:
                UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );
                break;
        }

        return true;
    }

    return false;
}

static const std::array<const UPnpCDSClientException,5> clientExceptions {{
    // Windows Media Player version 12
    { CDS_ClientWMP, 
      "User-Agent",
      "Windows-Media-Player/" },
    // Windows Media Player version < 12
    { CDS_ClientWMP,
      "User-Agent",
      "Mozilla/4.0 (compatible; UPnP/1.0; Windows 9x" },
    // XBMC
    { CDS_ClientXBMC,
      "User-Agent",
      "Platinum/" },
    // XBox 360
    { CDS_ClientXBox,
      "User-Agent",
      "Xbox" },
    // Sony Blu-ray players
    { CDS_ClientSonyDB,
      "X-AV-Client-Info",
      R"(cn="Sony Corporation"; mn="Blu-ray Disc Player")" },
}};

void UPnpCDS::DetermineClient( HTTPRequest *pRequest,
                               UPnpCDSRequest *pCDSRequest )
{
    pCDSRequest->m_eClient = CDS_ClientDefault;
    pCDSRequest->m_nClientVersion = 0;

    // Do we know this client string?
    for ( const auto & except : clientExceptions )
    {
        QString sHeaderValue = pRequest->GetRequestHeader(except.sHeaderKey, "");
        int idx = sHeaderValue.indexOf(except.sHeaderValue);
        if (idx == -1)
            continue;

        pCDSRequest->m_eClient = except.nClientType;

        idx += except.sHeaderValue.length();
 
        // If we have a / at the end of the string then we
        // increment the string to skip over it
        if ( sHeaderValue[idx] == '/')
        {
            idx++;
        }

        // Now find the version number
        QString version = sHeaderValue.mid(idx).trimmed();
        idx = version.indexOf( '.' );
        if (idx != -1)
        {
            idx = version.indexOf( '.', idx + 1 );
        }
        if (idx != -1)
        {
            version = version.left( idx );
        }
        idx = version.indexOf( ' ' );
        if (idx != -1)
        {
            version = version.left( idx );
        }

        pCDSRequest->m_nClientVersion = version.toDouble();

        LOG(VB_UPNP, LOG_INFO,
            QString("DetermineClient %1:%2 Identified as %3 version %4")
                .arg(except.sHeaderKey, sHeaderValue)
                .arg(pCDSRequest->m_eClient)
                .arg(pCDSRequest->m_nClientVersion));
        break;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleBrowse( HTTPRequest *pRequest )
{
    UPnpCDSExtensionResults *pResult  = nullptr;
    UPnpCDSRequest           request;

    DetermineClient( pRequest, &request );
    request.m_sObjectId         = pRequest->m_mapParams[ "objectid"      ];
    request.m_sParentId         = "0";
    request.m_eBrowseFlag       =
        GetBrowseFlag( pRequest->m_mapParams[ "browseflag"    ] );
    request.m_sFilter           = pRequest->m_mapParams[ "filter"        ];
    request.m_nStartingIndex    = std::max(pRequest->m_mapParams[ "startingindex" ].toUShort(),
                                      uint16_t(0));
    request.m_nRequestedCount   =
        pRequest->m_mapParams[ "requestedcount"].toUShort();
    if (request.m_nRequestedCount == 0)
        request.m_nRequestedCount = UINT16_MAX;
    request.m_sSortCriteria     = pRequest->m_mapParams[ "sortcriteria"  ];


    LOG(VB_UPNP, LOG_DEBUG, QString("UPnpCDS::ProcessRequest \n"
                                    ": url            = %1 \n"
                                    ": Method         = %2 \n"
                                    ": ObjectId       = %3 \n"
                                    ": BrowseFlag     = %4 \n"
                                    ": Filter         = %5 \n"
                                    ": StartingIndex  = %6 \n"
                                    ": RequestedCount = %7 \n"
                                    ": SortCriteria   = %8 " )
                       .arg( pRequest->m_sBaseUrl,
                             pRequest->m_sMethod,
                             request.m_sObjectId,
                             QString::number(request.m_eBrowseFlag),
                             request.m_sFilter,
                             QString::number(request.m_nStartingIndex),
                             QString::number(request.m_nRequestedCount),
                             request.m_sSortCriteria  ));

    UPnPResultCode eErrorCode      = UPnPResult_CDS_NoSuchObject;
    QString        sErrorDesc      = "";
    uint16_t       nNumberReturned = 0;
    uint16_t       nTotalMatches   = 0;
    uint16_t       nUpdateID       = 0;
    QString        sResultXML;
    FilterMap filter =  request.m_sFilter.split(',');

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::HandleBrowse ObjectID=%1")
            .arg(request.m_sObjectId));

    if (request.m_sObjectId == "0")
    {
        // ------------------------------------------------------------------
        // This is for the root object... lets handle it.
        // ------------------------------------------------------------------

        switch( request.m_eBrowseFlag )
        {
            case CDS_BrowseMetadata:
            {
                // -----------------------------------------------------------
                // Return Root Object Only
                // -----------------------------------------------------------

                eErrorCode      = UPnPResult_Success;
                nNumberReturned = 1;
                nTotalMatches   = 1;
                nUpdateID       = m_root.m_nUpdateId;

                m_root.SetChildCount( m_extensions.count() );
                m_root.SetChildContainerCount( m_extensions.count() );

                sResultXML      = m_root.toXml(filter);

                break;
            }

            case CDS_BrowseDirectChildren:
            {
                // Loop Through each extension and Build the Root Folders

                eErrorCode      = UPnPResult_Success;
                nTotalMatches   = m_extensions.count();
                nUpdateID       = m_root.m_nUpdateId;

                if (request.m_nRequestedCount == 0)
                    request.m_nRequestedCount = nTotalMatches;

                uint16_t nStart = std::max(request.m_nStartingIndex, uint16_t(0));
                uint16_t nCount = std::min(nTotalMatches, request.m_nRequestedCount);

                DetermineClient( pRequest, &request );

                for (uint i = nStart;
                     (i < (uint)m_extensions.size()) &&
                         (nNumberReturned < nCount);
                     i++)
                {
                    UPnpCDSExtension *pExtension = m_extensions[i];
                    CDSObject* pExtensionRoot = pExtension->GetRoot();
                    sResultXML += pExtensionRoot->toXml(filter, true); // Ignore Children
                    nNumberReturned ++;
                }

                break;
            }
            default: break;
        }
    }
    else
    {
        // ------------------------------------------------------------------
        // Look for a CDS Extension that knows how to handle this ObjectID
        // ------------------------------------------------------------------

        UPnpCDSExtensionList::iterator it = m_extensions.begin();
        for (; (it != m_extensions.end()) && !pResult; ++it)
        {
            LOG(VB_UPNP, LOG_INFO,
                QString("UPNP Browse : Searching for : %1  / ObjectID : %2")
                    .arg((*it)->m_sExtensionId, request.m_sObjectId));

            pResult = (*it)->Browse(&request);
        }

        if (pResult != nullptr)
        {
            eErrorCode  = pResult->m_eErrorCode;
            sErrorDesc  = pResult->m_sErrorDesc;

            if (eErrorCode == UPnPResult_Success)
            {
                while (pResult->m_List.size() > request.m_nRequestedCount)
                {
                    pResult->m_List.takeLast()->DecrRef();
                }

                nNumberReturned = pResult->m_List.count();
                nTotalMatches   = pResult->m_nTotalMatches;
                nUpdateID       = pResult->m_nUpdateID;
                if (request.m_eBrowseFlag == CDS_BrowseMetadata)
                    sResultXML      = pResult->GetResultXML(filter, true); // Ignore children
                else
                    sResultXML      = pResult->GetResultXML(filter);
            }

            delete pResult;
            pResult = nullptr;
        }
    }

    // ----------------------------------------------------------------------
    // Output Results of Browse Method
    // ----------------------------------------------------------------------

    if (eErrorCode == UPnPResult_Success)
    {
        NameValues list;

        QString sResults = DIDL_LITE_BEGIN;
        sResults += sResultXML;
        sResults += DIDL_LITE_END;

        list.push_back(NameValue("Result",         sResults));
        list.push_back(NameValue("NumberReturned", nNumberReturned));
        list.push_back(NameValue("TotalMatches",   nTotalMatches));
        list.push_back(NameValue("UpdateID",       nUpdateID));

        pRequest->FormatActionResponse(list);
    }
    else
    {
        UPnp::FormatErrorResponse ( pRequest, eErrorCode, sErrorDesc );
    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleSearch( HTTPRequest *pRequest )
{
    UPnpCDSExtensionResults *pResult  = nullptr;
    UPnpCDSRequest           request;

    UPnPResultCode eErrorCode      = UPnPResult_InvalidAction;
    QString       sErrorDesc      = "";
    uint16_t         nNumberReturned = 0;
    uint16_t         nTotalMatches   = 0;
    uint16_t         nUpdateID       = 0;
    QString       sResultXML;

    DetermineClient( pRequest, &request );
    request.m_sObjectId         = pRequest->m_mapParams[ "objectid"      ];
    request.m_sContainerID      = pRequest->m_mapParams[ "containerid"   ];
    request.m_sFilter           = pRequest->m_mapParams[ "filter"        ];
    request.m_nStartingIndex    =
        pRequest->m_mapParams[ "startingindex" ].toLong();
    request.m_nRequestedCount   =
        pRequest->m_mapParams[ "requestedcount"].toLong();
    request.m_sSortCriteria     = pRequest->m_mapParams[ "sortcriteria"  ];
    request.m_sSearchCriteria   = pRequest->m_mapParams[ "searchcriteria"];

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::HandleSearch ObjectID=%1, ContainerId=%2")
            .arg(request.m_sObjectId, request.m_sContainerID));

    // ----------------------------------------------------------------------
    // Break the SearchCriteria into it's parts
    // -=>TODO: This DOES NOT handle ('s or other complex expressions
    // ----------------------------------------------------------------------

    static const QRegularExpression re {"\\b(or|and)\\b", QRegularExpression::CaseInsensitiveOption};

    request.m_sSearchList  = request.m_sSearchCriteria.split(
        re, Qt::SkipEmptyParts);
    request.m_sSearchClass = "object";  // Default to all objects.

    // ----------------------------------------------------------------------
    // -=>TODO: Need to process all expressions in searchCriteria... for now,
    //          Just focus on the "upnp:class derivedfrom" expression
    // ----------------------------------------------------------------------

    for ( QStringList::Iterator it  = request.m_sSearchList.begin();
                                it != request.m_sSearchList.end();
                              ++it )
    {
        if ((*it).contains("upnp:class derivedfrom", Qt::CaseInsensitive))
        {
            QStringList sParts = (*it).split(' ', Qt::SkipEmptyParts);
            if (sParts.count() > 2)
            {
                request.m_sSearchClass = sParts[2].trimmed();
                request.m_sSearchClass.remove( '"' );

                break;
            }
        }
    }

    // ----------------------------------------------------------------------


    LOG(VB_UPNP, LOG_INFO, QString("UPnpCDS::ProcessRequest \n"
                                    ": url            = %1 \n"
                                    ": Method         = %2 \n"
                                    ": ObjectId       = %3 \n"
                                    ": SearchCriteria = %4 \n"
                                    ": Filter         = %5 \n"
                                    ": StartingIndex  = %6 \n"
                                    ": RequestedCount = %7 \n"
                                    ": SortCriteria   = %8 \n"
                                    ": SearchClass    = %9" )
                       .arg( pRequest->m_sBaseUrl,
                             pRequest->m_sMethod,
                             request.m_sObjectId,
                             request.m_sSearchCriteria,
                             request.m_sFilter,
                             QString::number(request.m_nStartingIndex),
                             QString::number(request.m_nRequestedCount),
                             request.m_sSortCriteria,
                             request.m_sSearchClass));

#if 0
    bool bSearchDone = false;
#endif

    UPnpCDSExtensionList::iterator it = m_extensions.begin();
    for (; (it != m_extensions.end()) && !pResult; ++it)
        pResult = (*it)->Search(&request);

    if (pResult != nullptr)
    {
        eErrorCode  = pResult->m_eErrorCode;
        sErrorDesc  = pResult->m_sErrorDesc;

        if (eErrorCode == UPnPResult_Success)
        {
            FilterMap filter = request.m_sFilter.split(',');
            nNumberReturned = pResult->m_List.count();
            nTotalMatches   = pResult->m_nTotalMatches;
            nUpdateID       = pResult->m_nUpdateID;
            sResultXML      = pResult->GetResultXML(filter);
#if 0
            bSearchDone = true;
#endif
        }

        delete pResult;
        pResult = nullptr;
    }

#if 0
    nUpdateID       = 0;
    LOG(VB_UPNP, LOG_DEBUG, sResultXML);
#endif

    if (eErrorCode == UPnPResult_Success)
    {
        NameValues list;
        QString sResults = DIDL_LITE_BEGIN;
        sResults += sResultXML;
        sResults += DIDL_LITE_END;

        list.push_back(NameValue("Result",         sResults));
        list.push_back(NameValue("NumberReturned", nNumberReturned));
        list.push_back(NameValue("TotalMatches",   nTotalMatches));
        list.push_back(NameValue("UpdateID",       nUpdateID));

        pRequest->FormatActionResponse(list);
    }
    else
    {
        UPnp::FormatErrorResponse( pRequest, eErrorCode, sErrorDesc );
    }
}

/**
 *  \brief Return the list of supported search fields
 *
 *  Currently no searching is supported.
 */

void UPnpCDS::HandleGetSearchCapabilities( HTTPRequest *pRequest )
{
    NameValues list;

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::ProcessRequest : %1 : %2")
            .arg(pRequest->m_sBaseUrl, pRequest->m_sMethod));

    // -=>TODO: Need to implement based on CDS Extension Capabilities

//     list.push_back(
//         NameValue("SearchCaps",
//                   "dc:title,dc:creator,dc:date,upnp:class,res@size,"
//                   "res@protocolInfo","@refID"));
    list.push_back(
        NameValue("SearchCaps","")); // We don't support any searching

    pRequest->FormatActionResponse(list);
}

/**
 *  \brief Return the list of supported sorting fields
 *
 *  Currently no sorting is supported.
 */

void UPnpCDS::HandleGetSortCapabilities( HTTPRequest *pRequest )
{
    NameValues list;

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::ProcessRequest : %1 : %2")
            .arg(pRequest->m_sBaseUrl, pRequest->m_sMethod));

    // -=>TODO: Need to implement based on CDS Extension Capabilities

//     list.push_back(
//         NameValue("SortCaps",
//                   "dc:title,dc:creator,dc:date,upnp:class,res@size,"
//                   "res@protocolInfo,@refID"));
    list.push_back(
        NameValue("SortCaps","")); // We don't support any sorting

    pRequest->FormatActionResponse(list);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSystemUpdateID( HTTPRequest *pRequest )
{
    NameValues list;

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::ProcessRequest : %1 : %2")
            .arg(pRequest->m_sBaseUrl, pRequest->m_sMethod));

    auto nId = GetValue<uint16_t>("SystemUpdateID");

    list.push_back(NameValue("Id", nId));

    pRequest->FormatActionResponse(list);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetFeatureList(HTTPRequest* pRequest)
{
    NameValues list;
    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::ProcessRequest : %1 : %2")
            .arg(pRequest->m_sBaseUrl, pRequest->m_sMethod));

    QString sResults = m_features.toXML();

    list.push_back(NameValue("FeatureList", sResults));

    pRequest->FormatActionResponse(list);
}

void UPnpCDS::HandleGetServiceResetToken(HTTPRequest* pRequest)
{
    NameValues list;

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::ProcessRequest : %1 : %2")
            .arg(pRequest->m_sBaseUrl, pRequest->m_sMethod));

    auto sToken = GetValue<QString>("ServiceResetToken");

    list.push_back(NameValue("ResetToken", sToken));

    pRequest->FormatActionResponse(list);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpCDSExtension Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtension::~UPnpCDSExtension()
{
    if (m_pRoot)
    {
        m_pRoot->DecrRef();
        m_pRoot = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSExtension::IsBrowseRequestForUs( UPnpCDSRequest *pRequest )
{
    if (!pRequest->m_sObjectId.startsWith(m_sExtensionId, Qt::CaseSensitive))
        return false;

    LOG(VB_UPNP, LOG_INFO, QString("%1: Browse request is for us.").arg(m_sExtensionId));

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSExtension::Browse( UPnpCDSRequest *pRequest )
{
    // -=>TODO: Need to add Filter & Sorting Support.

    if (!IsBrowseRequestForUs( pRequest ))
        return( nullptr );

    // ----------------------------------------------------------------------
    // Split the request ID into token key/value
    //
    //   Music/Artist=123/Album=15
    //   Music/Genre=32/Artist=616/Album=13/Track=2632
    // ----------------------------------------------------------------------
    IDTokenMap tokens = TokenizeIDString(pRequest->m_sObjectId);
    QString currentToken = GetCurrentToken(pRequest->m_sObjectId).first;

    LOG(VB_UPNP, LOG_DEBUG, QString("Browse (%1): Current Token '%2'")
                                .arg(m_sExtensionId, currentToken));

    // ----------------------------------------------------------------------
    // Process based on location in hierarchy
    // ----------------------------------------------------------------------

    auto *pResults = new UPnpCDSExtensionResults();

    if (pResults != nullptr)
    {
        switch( pRequest->m_eBrowseFlag )
        {
            case CDS_BrowseMetadata:
            {
                if (pRequest->m_nRequestedCount == 0)
                    pRequest->m_nRequestedCount = 1; // This should be the case anyway, but enforce it just in case

                pRequest->m_sParentId = "0"; // Root

                // Create parent ID by stripping the last token from the object ID
                if (pRequest->m_sObjectId.contains("/"))
                    pRequest->m_sParentId = pRequest->m_sObjectId.section("/", 0, -2);

                LOG(VB_UPNP, LOG_DEBUG, QString("UPnpCDS::Browse: BrowseMetadata (%1)").arg(pRequest->m_sObjectId));
                if (LoadMetadata(pRequest, pResults, tokens, currentToken))
                    return pResults;
                pResults->m_eErrorCode = UPnPResult_CDS_NoSuchObject;
                break;
            }

            case CDS_BrowseDirectChildren:
            {
                pRequest->m_sParentId = pRequest->m_sObjectId;
                LOG(VB_UPNP, LOG_DEBUG, QString("UPnpCDS::Browse: BrowseDirectChildren (%1)").arg(pRequest->m_sObjectId));
                if (LoadChildren(pRequest, pResults, tokens, currentToken))
                    return pResults;
                pResults->m_eErrorCode = UPnPResult_CDS_NoSuchObject;
                break;
            }

            default:
            {
                pResults->m_eErrorCode = UPnPResult_CDS_NoSuchObject;
                pResults->m_sErrorDesc = "";
            }
        }

    }

    return( pResults );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSExtension::IsSearchRequestForUs( UPnpCDSRequest *pRequest )
{
    return m_sClass.startsWith( pRequest->m_sSearchClass );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSExtension::Search( UPnpCDSRequest *pRequest )
{
    // -=>TODO: Need to add Filter & Sorting Support.
    // -=>TODO: Need to add Sub-Folder/Category Support!!!!!

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDSExtension::Search : m_sClass = %1 : "
                "m_sSearchClass = %2")
            .arg(m_sClass, pRequest->m_sSearchClass));

    if ( !IsSearchRequestForUs( pRequest ))
    {
        LOG(VB_UPNP, LOG_INFO,
            QString("UPnpCDSExtension::Search - Not For Us : "
                    "m_sClass = %1 : m_sSearchClass = %2")
                .arg(m_sClass, pRequest->m_sSearchClass));
        return nullptr;
    }

    auto *pResults = new UPnpCDSExtensionResults();

//    CreateItems( pRequest, pResults, 0, "", false );

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSExtension::RemoveToken( const QString &sToken,
                                       const QString &sStr, int num )
{
    QString sResult( "" );
    int     nPos = -1;

    for (int nIdx=0; nIdx < num; nIdx++)
    {
        nPos = sStr.lastIndexOf( sToken, nPos );
        if (nPos == -1)
            break;
    }

    if (nPos > 0)
        sResult = sStr.left( nPos );

    return sResult;
}

/**
 *  \brief Fetch just the metadata for the item identified in the request
 *
 *  This is the 'BrowseMetadata' request type.
 *
 *  The ID may refer to a container or an object.
 *
 *  \arg pRequest The request object to read the id from
 *  \arg pResults The result object to write into
 *
 *  \return true if we could load the metadata
 *
 */
bool UPnpCDSExtension::LoadMetadata(const UPnpCDSRequest* /*pRequest*/,
                                    UPnpCDSExtensionResults* /*pResults*/,
                                    const IDTokenMap& /*tokens*/,
                                    const QString& /*currentToken*/)
{
    return false;
}


/**
 *  \brief Fetch the children of the container identified in the request
 *
 *  This is the 'BrowseDirectChildren' request type.
 *
 *  The ID may only refer to a container.
 *
 *  \arg pRequest The request object to read the id from
 *  \arg pResults The result object to write into
 *
 *  \return true if we could load the children
 *
 */
bool UPnpCDSExtension::LoadChildren(const UPnpCDSRequest* /*pRequest*/,
                                    UPnpCDSExtensionResults* /*pResults*/,
                                    const IDTokenMap& /*tokens*/,
                                    const QString& /*currentToken*/)
{
    return false;
}

/**
 *  \brief Split the 'Id' String up into tokens for handling by each extension
 *
 *  Some example strings:
 *
 *      Recordings/RecGroup=3/Date=1380844800 (2013-10-04 in 'epoch' form)
 *      Video/Genre=10
 *      Music/Artist=123/Album=15
 *      Music/Genre=32/Artist=616/Album=13/Track=2632
 *
 *  Special case where we only care about the last token:
 *
 *      Video/Directory=45/Directory=63/Directory=82
 */
IDTokenMap UPnpCDSExtension::TokenizeIDString(const QString& Id)
{
    IDTokenMap tokenMap;

    QStringList tokens = Id.split('/');

    QStringList::iterator it;
    for (it = tokens.begin() + 1; it < tokens.end(); ++it) // Skip the 'root' token
    {

        QString key = (*it).section('=', 0, 0).toLower();
        QString value = (*it).section('=', 1, 1);

        tokenMap.insert(key, value);
        LOG(VB_UPNP, LOG_DEBUG, QString("Token Key: %1 Value: %2").arg(key,
                                                                       value));
    }

    return tokenMap;
}


/**
 *  \brief Split the 'Id' String up into tokens and return the last (current) token
 *
 *  Some example strings:
 *
 *      Recordings/RecGroup=3/Date=1380844800 (2013-10-04 in 'epoch' form)
 *      Video/Genre=10
 *      Music/Artist=123/Album=15
 *      Music/Genre=32/Artist=616/Album=13/Track=2632
 *
 *  Special case where we only care about the last token:
 *
 *      Video/Directory=45/Directory=63/Directory=82
 */
IDToken UPnpCDSExtension::GetCurrentToken(const QString& Id)
{
    QStringList tokens = Id.split('/');
    const QString& current = tokens.last();
    QString key = current.section('=', 0, 0).toLower();
    QString value = current.section('=', 1, 1);

    return {key, value};
}

QString UPnpCDSExtension::CreateIDString(const QString &requestId,
                                         const QString &name,
                                         int value)
{
    return CreateIDString(requestId, name, QString::number(value));
}

QString UPnpCDSExtension::CreateIDString(const QString &requestId,
                                         const QString &name,
                                         const QString &value)
{
    IDToken currentToken = GetCurrentToken(requestId);
    QString currentName = currentToken.first;
    QString currentValue = currentToken.second;

    // For metadata requests the request ID will be the ID of the result, so
    // we don't need to do anything
    if (currentName == name.toLower() && !currentValue.isEmpty() &&
        currentValue == value.toLower())
        return requestId;
    if (currentName == name.toLower() && currentValue.isEmpty())
        return QString("%1=%2").arg(requestId, value);
    return QString("%1/%2=%3").arg(requestId, name, value);
}

void UPnpCDSExtension::CreateRoot()
{
    LOG(VB_GENERAL, LOG_CRIT, "UPnpCDSExtension::CreateRoot() called on base class");
    m_pRoot = CDSObject::CreateContainer(m_sExtensionId,
                                         m_sName,
                                         "0");
}

CDSObject* UPnpCDSExtension::GetRoot()
{
    if (!m_pRoot)
        CreateRoot();

    return m_pRoot;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnPShortcutFeature::CreateXML()
{
    QString xml;

    xml = "<shortcutlist>\r\n";

    QMap<ShortCutType, QString>::iterator it;
    for (it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it)
    {
        ShortCutType type = it.key();
        const QString& objectID = *it;
        xml += "<shortcut>\r\n";
        xml += QString("<name>%1</name>\r\n").arg(TypeToName(type));
        xml += QString("<objectID>%1</objectID>\r\n").arg(HTTPRequest::Encode(objectID));
        xml += "</shortcut>\r\n";
    }

    xml += "</shortcutlist>\r\n";

    return xml;
}

bool UPnPShortcutFeature::AddShortCut(ShortCutType type,
                                         const QString &objectID)
{
    if (!m_shortcuts.contains(type))
        m_shortcuts.insert(type, objectID);
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("UPnPCDSShortcuts::AddShortCut(): "
                                         "Attempted to register duplicate "
                                         "shortcut").arg(TypeToName(type)));
    }

    return false;
}

QString UPnPShortcutFeature::TypeToName(ShortCutType type)
{
    QString str;

    switch (type)
    {
       case MUSIC :
          str = "MUSIC";
          break;
       case MUSIC_ALBUMS :
          str = "MUSIC_ALBUMS";
          break;
       case MUSIC_ARTISTS :
          str = "MUSIC_ARTISTS";
          break;
       case MUSIC_GENRES :
          str = "MUSIC_GENRES";
          break;
       case MUSIC_PLAYLISTS :
          str = "MUSIC_PLAYLISTS";
          break;
       case MUSIC_RECENTLY_ADDED :
          str = "MUSIC_RECENTLY_ADDED";
          break;
       case MUSIC_LAST_PLAYED :
          str = "MUSIC_LAST_PLAYED";
          break;
       case MUSIC_AUDIOBOOKS :
          str = "MUSIC_AUDIOBOOKS";
          break;
       case MUSIC_STATIONS :
          str = "MUSIC_STATIONS";
          break;
       case MUSIC_ALL :
          str = "MUSIC_ALL";
          break;
       case MUSIC_FOLDER_STRUCTURE :
          str = "MUSIC_FOLDER_STRUCTURE";
          break;

       case IMAGES :
          str = "IMAGES";
          break;
       case IMAGES_YEARS :
          str = "IMAGES_YEARS";
          break;
       case IMAGES_YEARS_MONTH :
          str = "IMAGES_YEARS_MONTH";
          break;
       case IMAGES_ALBUM :
          str = "IMAGES_ALBUM";
          break;
       case IMAGES_SLIDESHOWS :
          str = "IMAGES_SLIDESHOWS";
          break;
       case IMAGES_RECENTLY_ADDED :
          str = "IMAGES_RECENTLY_ADDED";
          break;
       case IMAGES_LAST_WATCHED :
          str = "IMAGES_LAST_WATCHED";
          break;
       case IMAGES_ALL :
          str = "IMAGES_ALL";
          break;
       case IMAGES_FOLDER_STRUCTURE :
          str = "IMAGES_FOLDER_STRUCTURE";
          break;

       case VIDEOS :
          str = "VIDEOS";
          break;
       case VIDEOS_GENRES :
          str = "VIDEOS_GENRES";
          break;
       case VIDEOS_YEARS :
          str = "VIDEOS_YEARS";
          break;
       case VIDEOS_YEARS_MONTH :
          str = "VIDEOS_YEARS_MONTH";
          break;
       case VIDEOS_ALBUM :
          str = "VIDEOS_ALBUM";
          break;
       case VIDEOS_RECENTLY_ADDED :
          str = "VIDEOS_RECENTLY_ADDED";
          break;
       case VIDEOS_LAST_PLAYED :
          str = "VIDEOS_LAST_PLAYED";
          break;
       case VIDEOS_RECORDINGS :
          str = "VIDEOS_RECORDINGS";
          break;
       case VIDEOS_ALL :
          str = "VIDEOS_ALL";
          break;
       case VIDEOS_FOLDER_STRUCTURE :
       case FOLDER_STRUCTURE :
          str = "VIDEOS_FOLDER_STRUCTURE";
          break;
    }

    return str;
}

// vim:ts=4:sw=4:ai:et:si:sts=4
