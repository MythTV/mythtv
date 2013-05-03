//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcds.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : uPnp Content Directory Service
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <algorithm>
using namespace std;

#include "upnp.h"
#include "upnpcds.h"
#include "upnputil.h"
#include "mythlogging.h"

#define DIDL_LITE_BEGIN "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">"
#define DIDL_LITE_END   "</DIDL-Lite>";

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSExtensionResults::Add( CDSObject *pObject )
{
    if (pObject)
        m_List.append( pObject );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSExtensionResults::GetResultXML(FilterMap &filter)
{
    QString sXML;

    CDSObjects::const_iterator it = m_List.begin();
    for (; it != m_List.end(); ++it)
        sXML += (*it)->toXml(filter);

    return sXML;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDS::UPnpCDS( UPnpDevice *pDevice, const QString &sSharePath )
  : Eventing( "UPnpCDS", "CDS_Event", sSharePath )
{
    m_root.m_eType      = OT_Container;
    m_root.m_sId        = "0";
    m_root.m_sParentId  = "-1";
    m_root.m_sTitle     = "MythTV";
    m_root.m_sClass     = "object.container";
    m_root.m_bRestricted= true;
    m_root.m_bSearchable= true;

    AddVariable( new StateVariable< QString        >( "TransferIDs"       , true ) );
    AddVariable( new StateVariable< QString        >( "ContainerUpdateIDs", true ) );
    AddVariable( new StateVariable< unsigned short >( "SystemUpdateID"    , true ) );

    SetValue< unsigned short >( "SystemUpdateID", 1 );

    QString sUPnpDescPath = UPnp::GetConfiguration()->GetValue( "UPnP/DescXmlPath", sSharePath );

    m_sServiceDescFileName = sUPnpDescPath + "CDS_scpd.xml";
    m_sControlUrl          = "/CDS_Control";


    // Add our Service Definition to the device.

    RegisterService( pDevice );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDS::~UPnpCDS()
{
    while (!m_extensions.empty())
    {
        delete m_extensions.back();
        m_extensions.pop_back();
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
    if (pExtension != NULL )
        m_extensions.append( pExtension );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::UnregisterExtension( UPnpCDSExtension *pExtension )
{
    if (pExtension)
    {
        delete pExtension;
        m_extensions.removeAll(pExtension);
    }
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
            default:
                UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );
                break;
        }

        return true;
    }

    return false;
}

static UPnpCDSClientException clientExceptions[] = {
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
      "cn=\"Sony Corporation\"; mn=\"Blu-ray Disc Player\"" },
};
static uint clientExceptionCount = sizeof(clientExceptions) /
                                   sizeof(clientExceptions[0]);

void UPnpCDS::DetermineClient( HTTPRequest *pRequest,
                               UPnpCDSRequest *pCDSRequest )
{
    pCDSRequest->m_eClient = CDS_ClientDefault;
    pCDSRequest->m_nClientVersion = 0;
    bool found = false;

    // Do we know this client string?
    for ( uint i = 0; !found && i < clientExceptionCount; i++ )
    {
        UPnpCDSClientException *except = &clientExceptions[i];

        QString sHeaderValue = pRequest->GetHeaderValue(except->sHeaderKey, "");
        int idx = sHeaderValue.indexOf(except->sHeaderValue);
        if (idx != -1)
        {
            pCDSRequest->m_eClient = except->nClientType;

            idx += except->sHeaderValue.length();
 
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
                    .arg(except->sHeaderKey) .arg(sHeaderValue)
                    .arg(pCDSRequest->m_eClient)
                    .arg(pCDSRequest->m_nClientVersion));
            found = true;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleBrowse( HTTPRequest *pRequest )
{
    UPnpCDSExtensionResults *pResult  = NULL;
    UPnpCDSRequest           request;

    DetermineClient( pRequest, &request );
    request.m_sObjectId         = pRequest->m_mapParams[ "ObjectID"      ];
    request.m_sContainerID      = pRequest->m_mapParams[ "ContainerID"   ];
    request.m_sParentId         = "0";
    request.m_eBrowseFlag       =
        GetBrowseFlag( pRequest->m_mapParams[ "BrowseFlag"    ] );
    request.m_sFilter           = pRequest->m_mapParams[ "Filter"        ];
    request.m_nStartingIndex    =
        pRequest->m_mapParams[ "StartingIndex" ].toLong();
    request.m_nRequestedCount   =
        pRequest->m_mapParams[ "RequestedCount"].toLong();
    request.m_sSortCriteria     = pRequest->m_mapParams[ "SortCriteria"  ];

#if 0
    LOG(VB_UPNP, LOG_DEBUG, QString("UPnpCDS::ProcessRequest \n"
                                    ": url            = %1 \n"
                                    ": Method         = %2 \n"
                                    ": ObjectId       = %3 \n"
                                    ": BrowseFlag     = %4 \n"
                                    ": Filter         = %5 \n"
                                    ": StartingIndex  = %6 \n"
                                    ": RequestedCount = %7 \n"
                                    ": SortCriteria   = %8 " )
                       .arg( pRequest->m_sBaseUrl     )
                       .arg( pRequest->m_sMethod      )
                       .arg( request.m_sObjectId      )
                       .arg( request.m_eBrowseFlag    )
                       .arg( request.m_sFilter        )
                       .arg( request.m_nStartingIndex )
                       .arg( request.m_nRequestedCount)
                       .arg( request.m_sSortCriteria  ));
#endif

    UPnPResultCode eErrorCode      = UPnPResult_CDS_NoSuchObject;
    QString        sErrorDesc      = "";
    short          nNumberReturned = 0;
    short          nTotalMatches   = 0;
    short          nUpdateID       = 0;
    QString        sResultXML;
    FilterMap filter =  (FilterMap) request.m_sFilter.split(',');

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::HandleBrowse ObjectID=%1, ContainerId=%2")
            .arg(request.m_sObjectId) .arg(request.m_sContainerID));

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

                sResultXML      = m_root.toXml(filter);

                break;
            }

            case CDS_BrowseDirectChildren:
            {
                // Loop Through each extension and Build the Root Folders

                // -=>TODO: Need to handle StartingIndex & RequestedCount

                eErrorCode      = UPnPResult_Success;
                nTotalMatches   = m_extensions.count();
                nUpdateID       = m_root.m_nUpdateId;

                if (request.m_nRequestedCount == 0)
                    request.m_nRequestedCount = nTotalMatches;

                short nStart = Max( request.m_nStartingIndex, short( 0 ));
                short nCount = Min( nTotalMatches, request.m_nRequestedCount );

                UPnpCDSRequest       childRequest;

                DetermineClient( pRequest, &request );
                childRequest.m_sParentId         = "0";
                childRequest.m_eBrowseFlag       = CDS_BrowseMetadata;
                childRequest.m_sFilter           = "";
                childRequest.m_nStartingIndex    = 0;
                childRequest.m_nRequestedCount   = 1;
                childRequest.m_sSortCriteria     = "";

                for (uint i = nStart;
                     (i < (uint)m_extensions.size()) &&
                         (nNumberReturned < nCount);
                     i++)
                {
                    UPnpCDSExtension *pExtension = m_extensions[i];
                    childRequest.m_sObjectId = pExtension->m_sExtensionId;

                    pResult = pExtension->Browse( &childRequest );

                    if (pResult != NULL)
                    {
                        if (pResult->m_eErrorCode == UPnPResult_Success)
                        {
                            sResultXML  += pResult->GetResultXML(filter);
                            nNumberReturned ++;
                        }

                        delete pResult;
                    }
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
                    .arg((*it)->m_sExtensionId).arg(request.m_sObjectId));

            pResult = (*it)->Browse(&request);
        }

        if (pResult != NULL)
        {
            eErrorCode  = pResult->m_eErrorCode;
            sErrorDesc  = pResult->m_sErrorDesc;

            if (eErrorCode == UPnPResult_Success)
            {
                nNumberReturned = pResult->m_List.count();
                nTotalMatches   = pResult->m_nTotalMatches;
                nUpdateID       = pResult->m_nUpdateID;
                sResultXML      = pResult->GetResultXML(filter);
            }

            delete pResult;
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
        UPnp::FormatErrorResponse ( pRequest, eErrorCode, sErrorDesc );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleSearch( HTTPRequest *pRequest )
{
    UPnpCDSExtensionResults *pResult  = NULL;
    UPnpCDSRequest           request;

    UPnPResultCode eErrorCode      = UPnPResult_InvalidAction;
    QString       sErrorDesc      = "";
    short         nNumberReturned = 0;
    short         nTotalMatches   = 0;
    short         nUpdateID       = 0;
    QString       sResultXML;

    DetermineClient( pRequest, &request );
    request.m_sObjectId         = pRequest->m_mapParams[ "ObjectID"      ];
    request.m_sContainerID      = pRequest->m_mapParams[ "ContainerID"   ];
    request.m_sFilter           = pRequest->m_mapParams[ "Filter"        ];
    request.m_nStartingIndex    =
        pRequest->m_mapParams[ "StartingIndex" ].toLong();
    request.m_nRequestedCount   =
        pRequest->m_mapParams[ "RequestedCount"].toLong();
    request.m_sSortCriteria     = pRequest->m_mapParams[ "SortCriteria"  ];
    request.m_sSearchCriteria   = pRequest->m_mapParams[ "SearchCriteria"];

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::HandleSearch ObjectID=%1, ContainerId=%2")
            .arg(request.m_sObjectId) .arg(request.m_sContainerID));

    // ----------------------------------------------------------------------
    // Break the SearchCriteria into it's parts
    // -=>TODO: This DOES NOT handle ('s or other complex expressions
    // ----------------------------------------------------------------------

    QRegExp  rMatch( "\\b(or|and)\\b" );
    rMatch.setCaseSensitivity(Qt::CaseInsensitive);

    request.m_sSearchList  = request.m_sSearchCriteria.split(
        rMatch, QString::SkipEmptyParts);
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
            QStringList sParts = (*it).split(' ', QString::SkipEmptyParts);

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
                       .arg( pRequest->m_sBaseUrl     )
                       .arg( pRequest->m_sMethod      )
                       .arg( request.m_sObjectId      )
                       .arg( request.m_sSearchCriteria)
                       .arg( request.m_sFilter        )
                       .arg( request.m_nStartingIndex )
                       .arg( request.m_nRequestedCount)
                       .arg( request.m_sSortCriteria  )
                       .arg( request.m_sSearchClass   ));

#if 0
    bool bSearchDone = false;
#endif

    UPnpCDSExtensionList::iterator it = m_extensions.begin();
    for (; (it != m_extensions.end()) && !pResult; ++it)
        pResult = (*it)->Search(&request);

    if (pResult != NULL)
    {
        eErrorCode  = pResult->m_eErrorCode;
        sErrorDesc  = pResult->m_sErrorDesc;

        if (eErrorCode == UPnPResult_Success)
        {
            FilterMap filter =  (FilterMap) request.m_sFilter.split(',');
            nNumberReturned = pResult->m_List.count();
            nTotalMatches   = pResult->m_nTotalMatches;
            nUpdateID       = pResult->m_nUpdateID;
            sResultXML      = pResult->GetResultXML(filter);
#if 0
            bSearchDone = true;
#endif
        }

        delete pResult;
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
        UPnp::FormatErrorResponse( pRequest, eErrorCode, sErrorDesc );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSearchCapabilities( HTTPRequest *pRequest )
{
    NameValues list;

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::ProcessRequest : %1 : %2")
            .arg(pRequest->m_sBaseUrl) .arg(pRequest->m_sMethod));

    // -=>TODO: Need to implement based on CDS Extension Capabilities

    list.push_back(
        NameValue("SearchCaps",
                  "dc:title,dc:creator,dc:date,upnp:class,res@size"));

    pRequest->FormatActionResponse(list);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSortCapabilities( HTTPRequest *pRequest )
{
    NameValues list;

    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDS::ProcessRequest : %1 : %2")
            .arg(pRequest->m_sBaseUrl) .arg(pRequest->m_sMethod));

    // -=>TODO: Need to implement based on CDS Extension Capabilities

    list.push_back(
        NameValue("SortCaps",
                  "dc:title,dc:creator,dc:date,upnp:class,res@size"));

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
            .arg(pRequest->m_sBaseUrl) .arg(pRequest->m_sMethod));

    unsigned short nId = GetValue<unsigned short>("SystemUpdateID");

    list.push_back(NameValue("Id", nId));

    pRequest->FormatActionResponse(list);
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpCDSExtension Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSExtension::IsBrowseRequestForUs( UPnpCDSRequest *pRequest )
{
    if (!pRequest->m_sObjectId.startsWith(m_sExtensionId, Qt::CaseSensitive))
        return false;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSExtension::Browse( UPnpCDSRequest *pRequest )
{
    // -=>TODO: Need to add Filter & Sorting Support.
    // -=>TODO: Need to add Sub-Folder/Category Support!!!!!

    if (!IsBrowseRequestForUs( pRequest ))
        return( NULL );

    // ----------------------------------------------------------------------
    // Parse out request object's path
    // ----------------------------------------------------------------------

    QStringList idPath = pRequest->m_sObjectId.section('=',0,0)
                             .split("/", QString::SkipEmptyParts);

    QString key = pRequest->m_sObjectId.section('=',1);

    if (idPath.isEmpty())
        return( NULL );

    // ----------------------------------------------------------------------
    // Process based on location in hierarchy
    // ----------------------------------------------------------------------

    UPnpCDSExtensionResults *pResults = new UPnpCDSExtensionResults();

    if (pResults != NULL)
    {
        if (!key.isEmpty())
            idPath.last().append(QString("=%1").arg(key));
        else
        {
            if (pRequest->m_sObjectId.contains("item"))
            {
                idPath.removeLast();
                idPath = idPath.last().split(" ", QString::SkipEmptyParts);
                idPath = idPath.first().split('?', QString::SkipEmptyParts);

                if (idPath[0].startsWith(QString("Id")))
                    idPath[0] = QString("item=%1")
                                 .arg(idPath[0].right(idPath[0].length() - 2));
            }
        }

        QString sLast = idPath.last();

        pRequest->m_sParentId = pRequest->m_sObjectId;

        if (sLast == m_sExtensionId)
            return ProcessRoot(pRequest, pResults, idPath);

        if (sLast == "0")
            return ProcessAll(pRequest, pResults, idPath);

        if (sLast.startsWith(QString("key") , Qt::CaseSensitive))
            return ProcessKey(pRequest, pResults, idPath);

        if (sLast.startsWith(QString("item"), Qt::CaseSensitive))
            return ProcessItem(pRequest, pResults, idPath);

        int nNodeIdx = sLast.toInt();

        if ((nNodeIdx > 0) && (nNodeIdx < GetRootCount()))
            return ProcessContainer(pRequest, pResults, nNodeIdx, idPath);

        pResults->m_eErrorCode = UPnPResult_CDS_NoSuchObject;
        pResults->m_sErrorDesc = "";
    }

    return( pResults );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSExtension::IsSearchRequestForUs( UPnpCDSRequest *pRequest )
{
    if ( !m_sClass.startsWith( pRequest->m_sSearchClass ))
        return false;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSExtension::Search( UPnpCDSRequest *pRequest )
{
    // -=>TODO: Need to add Filter & Sorting Support.
    // -=>TODO: Need to add Sub-Folder/Category Support!!!!!

    QStringList sEmptyList;
    LOG(VB_UPNP, LOG_INFO,
        QString("UPnpCDSExtension::Search : m_sClass = %1 : "
                "m_sSearchClass = %2")
            .arg(m_sClass).arg(pRequest->m_sSearchClass));

    if ( !IsSearchRequestForUs( pRequest ))
    {
        LOG(VB_UPNP, LOG_INFO,
            QString("UPnpCDSExtension::Search - Not For Us : "
                    "m_sClass = %1 : m_sSearchClass = %2")
                .arg(m_sClass).arg(pRequest->m_sSearchClass));
        return NULL;
    }

    UPnpCDSExtensionResults *pResults = new UPnpCDSExtensionResults();

    CreateItems( pRequest, pResults, 0, "", false );

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
        if ((nPos = sStr.lastIndexOf( sToken, nPos )) == -1)
            break;
    }

    if (nPos > 0)
        sResult = sStr.left( nPos );

    return sResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *
    UPnpCDSExtension::ProcessRoot( UPnpCDSRequest          *pRequest,
                                   UPnpCDSExtensionResults *pResults,
                                   QStringList             &/*idPath*/ )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    short nRootCount = GetRootCount();

    switch( pRequest->m_eBrowseFlag )
    {
        case CDS_BrowseMetadata:
        {
            // --------------------------------------------------------------
            // Return Root Object Only
            // --------------------------------------------------------------

            pResults->m_nTotalMatches   = 1;
            pResults->m_nUpdateID       = 1;

            CDSObject *pRoot = CreateContainer( m_sExtensionId, m_sName, "0");

            pRoot->SetChildCount( nRootCount );

            pResults->Add( pRoot );

            break;
        }

        case CDS_BrowseDirectChildren:
        {
            LOG(VB_UPNP, LOG_DEBUG, "CDS_BrowseDirectChildren");
            pResults->m_nUpdateID     = 1;
            pResults->m_nTotalMatches = nRootCount ;

            if ( pRequest->m_nRequestedCount == 0)
                pRequest->m_nRequestedCount = nRootCount ;

            short nStart = max(pRequest->m_nStartingIndex, short(0));
            short nEnd   = min(nRootCount,
                               short(nStart + pRequest->m_nRequestedCount));

            if (nStart < nRootCount)
            {
                for (short nIdx = nStart; nIdx < nEnd; nIdx++)
                {
                    UPnpCDSRootInfo *pInfo = GetRootInfo( nIdx );
                    if (pInfo != NULL)
                    {
                        QString sId = QString("%1/%2")
                                          .arg(pRequest->m_sObjectId)
                                          .arg(nIdx);

                        CDSObject *pItem =
                            CreateContainer( sId, QObject::tr( pInfo->title ),
                                             m_sExtensionId );

                        pItem->SetChildCount( GetDistinctCount( pInfo ) );

                        pResults->Add( pItem );
                    }
                }
            }
        }

        case CDS_BrowseUnknown:
        default:
            break;
    }

    return pResults;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *
    UPnpCDSExtension::ProcessAll ( UPnpCDSRequest          *pRequest,
                                   UPnpCDSExtensionResults *pResults,
                                   QStringList             &/*idPath*/ )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------

    switch( pRequest->m_eBrowseFlag )
    {
        case CDS_BrowseMetadata:
        {
            // --------------------------------------------------------------
            // Return Container Object Only
            // --------------------------------------------------------------

            UPnpCDSRootInfo *pInfo = GetRootInfo( 0 );

            if (pInfo != NULL)
            {
                pResults->m_nTotalMatches   = 1;
                pResults->m_nUpdateID       = 1;

                CDSObject *pItem =
                    CreateContainer( pRequest->m_sObjectId,
                                     QObject::tr( pInfo->title ),
                                     m_sExtensionId );

                pItem->SetChildCount( GetDistinctCount( pInfo ) );

                pResults->Add( pItem );
            }

            break;
        }

        case CDS_BrowseDirectChildren:
        {
            CreateItems( pRequest, pResults, 0, "", false );

            break;
        }

        case CDS_BrowseUnknown:
        default:
            break;
    }

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *
    UPnpCDSExtension::ProcessItem(UPnpCDSRequest          *pRequest,
                                  UPnpCDSExtensionResults *pResults,
                                  QStringList             &idPath)
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------
#if 0
    LOG(VB_UPNP, LOG_INFO, QString("UPnpCDSExtension::ProcessItem : %1")
                               .arg(idPath));
#endif
    switch( pRequest->m_eBrowseFlag )
    {
        case CDS_BrowseMetadata:
        {
            // --------------------------------------------------------------
            // Return 1 Item
            // --------------------------------------------------------------

            QStringMap  mapParams;
            QString     sParams = idPath.last().section( '?', 1, 1 );
            sParams.replace("&amp;", "&");

            HTTPRequest::GetParameters( sParams, mapParams );

            MSqlQuery query(MSqlQuery::InitCon());

            if (query.isConnected())
            {
                BuildItemQuery( query, mapParams );

                if (query.exec() && query.next())
                {
                    pRequest->m_sObjectId =
                        RemoveToken( "/", pRequest->m_sObjectId, 1 );

                    AddItem( pRequest, pRequest->m_sObjectId, pResults, false,
                             query );
                    pResults->m_nTotalMatches = 1;
                }
            }
            break;
        }
        case CDS_BrowseDirectChildren:
        {
            // Items don't have any children.
            break;
        }
        case CDS_BrowseUnknown:
            break;
    }

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *
    UPnpCDSExtension::ProcessKey( UPnpCDSRequest          *pRequest,
                                  UPnpCDSExtensionResults *pResults,
                                  QStringList             &idPath )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------

    QString sKey = idPath.takeLast().section( '=', 1, 1 );
    sKey = QUrl::fromPercentEncoding(sKey.toUtf8());

    if (!sKey.isEmpty())
    {
        int nNodeIdx = idPath.takeLast().toInt();

        switch( pRequest->m_eBrowseFlag )
        {
            case CDS_BrowseMetadata:
            {
                UPnpCDSRootInfo *pInfo = GetRootInfo( nNodeIdx );

                if (pInfo == NULL)
                    return pResults;

                pRequest->m_sParentId =
                    RemoveToken( "/", pRequest->m_sObjectId, 1 );

                // ----------------------------------------------------------
                // Since Key is not always the title, we need to lookup title.
                // ----------------------------------------------------------

                MSqlQuery query(MSqlQuery::InitCon());

                if (query.isConnected())
                {
                    QString sSQL = QString(pInfo->sql) .arg(pInfo->where);

                    // -=>TODO: There is a problem when called for an Item,
                    //          instead of a container
                    //          sKey = '<KeyName>/item?ChanId'
                    //          which is incorrect.

                    query.prepare  ( sSQL );
                    query.bindValue( ":KEY", sKey );

                    if (query.exec() && query.next())
                    {
                        // ----------------------------------------------
                        // Return Container Object Only
                        // ----------------------------------------------

                        pResults->m_nTotalMatches   = 1;
                        pResults->m_nUpdateID       = 1;

                        CDSObject *pItem =
                            CreateContainer( pRequest->m_sObjectId,
                                             query.value(1).toString(),
                                             pRequest->m_sParentId );

                        pItem->SetChildCount( GetDistinctCount( pInfo ));

                        pResults->Add( pItem );
                    }
                }
                break;
            }

            case CDS_BrowseDirectChildren:
            {
                CreateItems( pRequest, pResults, nNodeIdx, sKey, true );

                break;
            }

            case CDS_BrowseUnknown:
                default:
                break;
        }
    }

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *
    UPnpCDSExtension::ProcessContainer( UPnpCDSRequest          *pRequest,
                                        UPnpCDSExtensionResults *pResults,
                                        int                      nNodeIdx,
                                        QStringList             &/*idPath*/ )
{
    pResults->m_nUpdateID     = 1;
    pResults->m_nTotalMatches = 0;

    UPnpCDSRootInfo *pInfo = GetRootInfo( nNodeIdx );

    if (pInfo == NULL)
        return pResults;

    switch( pRequest->m_eBrowseFlag )
    {
        case CDS_BrowseMetadata:
        {
            // --------------------------------------------------------------
            // Return Container Object Only
            // --------------------------------------------------------------

            pResults->m_nTotalMatches   = 1;
            pResults->m_nUpdateID       = 1;

            CDSObject *pItem = CreateContainer( pRequest->m_sObjectId,
                                                QObject::tr( pInfo->title ),
                                                m_sExtensionId );

            pItem->SetChildCount( GetDistinctCount( pInfo ));

            pResults->Add( pItem );
            break;
        }

        case CDS_BrowseDirectChildren:
        {
            pResults->m_nTotalMatches = GetDistinctCount( pInfo );
            pResults->m_nUpdateID     = 1;

            if (pRequest->m_nRequestedCount == 0)
                pRequest->m_nRequestedCount = SHRT_MAX;

            MSqlQuery query(MSqlQuery::InitCon());

            if (query.isConnected())
            {
                // Remove where clause placeholder.
                QString sSQL = pInfo->sql;

                sSQL.remove( "%1" );
                sSQL += QString( " LIMIT %2, %3" )
                           .arg( pRequest->m_nStartingIndex  )
                           .arg( pRequest->m_nRequestedCount );

                query.prepare( sSQL );

                if (query.exec())
                {
                    while(query.next())
                    {
                        QString sKey   = query.value(0).toString();
                        QString sTitle = query.value(1).toString();
                        long    nCount = query.value(2).toInt();

                        if (sTitle.length() == 0)
                            sTitle = "(undefined)";

                        QString sId = QString( "%1/key=%2" )
                                         .arg( pRequest->m_sParentId )
                                         .arg( sKey );

                        CDSObject *pRoot =
                            CreateContainer(sId, sTitle, pRequest->m_sParentId);

                        pRoot->SetChildCount( nCount );

                        pResults->Add( pRoot );
                    }
                }
            }
            break;
        }

        case CDS_BrowseUnknown:
            break;
    }

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSExtension::GetDistinctCount( UPnpCDSRootInfo *pInfo )
{
    int nCount = 0;

    if ((pInfo == NULL) || (pInfo->column == NULL))
        return 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        // Note: Tried to use Bind, however it would not allow me to use it
        //       for column & table names

        QString sSQL;

        if (strncmp( pInfo->column, "*", 1) == 0)
        {
            sSQL = QString( "SELECT count( %1 ) FROM %2" )
                      .arg( pInfo->column )
                      .arg( GetTableName( pInfo->column ));
        }
        else
        {
            sSQL = QString( "SELECT count( DISTINCT %1 ) FROM %2" )
                      .arg( pInfo->column )
                      .arg( GetTableName( pInfo->column ) );
        }

        query.prepare( sSQL );

        if (query.exec() && query.next())
        {
            nCount = query.value(0).toInt();
        }
    }

    return( nCount );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSExtension::GetCount( const QString &sColumn, const QString &sKey )
{
    int nCount = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        QString sSQL = QString("SELECT count( %1 ) FROM %2")
                       .arg( sColumn ).arg( GetTableName( sColumn ) );

        if ( sKey.length() )
            sSQL += " WHERE " + sColumn + " = :KEY";

        query.prepare( sSQL );
        if ( sKey.length() )
            query.bindValue( ":KEY", sKey );

        if (query.exec() && query.next())
        {
            nCount = query.value(0).toInt();
        }
        LOG(VB_UPNP, LOG_DEBUG, "UPnpCDSExtension::GetCount() - " +
                                sSQL + " = " + QString::number(nCount));
    }

    return( nCount );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSExtension::CreateItems( UPnpCDSRequest          *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    int                      nNodeIdx,
                                    const QString           &sKey,
                                    bool                     bAddRef )
{
    pResults->m_nTotalMatches = 0;
    pResults->m_nUpdateID     = 1;

    UPnpCDSRootInfo *pInfo = GetRootInfo( nNodeIdx );

    if (pInfo == NULL)
        return;

    pResults->m_nTotalMatches = GetCount( pInfo->column, sKey );
    pResults->m_nUpdateID     = 1;

    if (pRequest->m_nRequestedCount == 0)
        pRequest->m_nRequestedCount = SHRT_MAX;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        QString sWhere( "" );
        QString sOrder( "" );

        if ( sKey.length() > 0)
        {
           sWhere = QString( "WHERE %1=:KEY " )
                       .arg( pInfo->column );
        }

        QString orderColumn( pInfo->orderColumn );
        if (orderColumn.length() != 0) {
            sOrder = QString( "ORDER BY %1 " )
                       .arg( orderColumn );
        }

        QString sSQL = QString( "%1 %2 LIMIT %3, %4" )
                          .arg( GetItemListSQL( pInfo->column )  )
                          .arg( sWhere + sOrder )
                          .arg( pRequest->m_nStartingIndex  )
                          .arg( pRequest->m_nRequestedCount );

        query.prepare  ( sSQL );
        if ( sKey.length() )
            query.bindValue(":KEY", sKey );

        if (query.exec())
        {
            while(query.next())
                AddItem( pRequest, pRequest->m_sObjectId, pResults, bAddRef,
                         query );
        }
    }
}

// vim:ts=4:sw=4:ai:et:si:sts=4
