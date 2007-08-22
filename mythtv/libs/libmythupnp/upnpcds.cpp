//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcds.cpp
//                                                                            
// Purpose - uPnp Content Directory Service 
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnp.h"
#include "upnpcds.h"
#include "upnputil.h"

#include "util.h"

#include <qtextstream.h>
#include <math.h>
#include <qregexp.h>

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

QString UPnpCDSExtensionResults::GetResultXML()
{
    QString sXML;

    for ( CDSObject *pObject  = m_List.first(); 
                     pObject != NULL;
                     pObject  = m_List.next() )
    {
        sXML += pObject->toXml();    
    }

    return( sXML );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDS::UPnpCDS( UPnpDevice *pDevice ) : Eventing( "UPnpCDS", "CDS_Event" )
{
    m_extensions.setAutoDelete( true );

    m_root.m_eType      = OT_Container;
    m_root.m_sId        = "0";
    m_root.m_sParentId  = "-1";
    m_root.m_sTitle     = "MythTv";
    m_root.m_sClass     = "object.container";
    m_root.m_bRestricted= true;
    m_root.m_bSearchable= true;

    AddVariable( new StateVariable< QString        >( "TransferIDs"       , true ) );
    AddVariable( new StateVariable< QString        >( "ContainerUpdateIDs", true ) );
    AddVariable( new StateVariable< unsigned short >( "SystemUpdateID"    , true ) );

    SetValue< unsigned short >( "SystemUpdateID", 1 );

    QString sUPnpDescPath = UPnp::g_pConfig->GetValue( "UPnP/DescXmlPath", m_sSharePath );

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
    if (pExtension != NULL )
        m_extensions.remove( pExtension );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDS::ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if (Eventing::ProcessRequest( pThread, pRequest ))
            return true;

        if ( pRequest->m_sBaseUrl != m_sControlUrl )
        {
//            VERBOSE( VB_UPNP, QString("UPnpCDS::ProcessRequest - BaseUrl (%1) not ours...").arg(pRequest->m_sBaseUrl ));
            return false;
        }

        switch( GetMethod( pRequest->m_sMethod ) )
        {
            case CDSM_GetServiceDescription : pRequest->FormatFileResponse( m_sServiceDescFileName ); break;
            case CDSM_Browse                : HandleBrowse                ( pRequest ); break;
            case CDSM_Search                : HandleSearch                ( pRequest ); break;
            case CDSM_GetSearchCapabilities : HandleGetSearchCapabilities ( pRequest ); break;
            case CDSM_GetSortCapabilities   : HandleGetSortCapabilities   ( pRequest ); break;
            case CDSM_GetSystemUpdateID     : HandleGetSystemUpdateID     ( pRequest ); break;
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

void UPnpCDS::HandleBrowse( HTTPRequest *pRequest )
{
    UPnpCDSExtensionResults *pResult  = NULL;
    UPnpCDSRequest           request;

    request.m_sObjectId         = pRequest->m_mapParams[ "ObjectID"      ];
    request.m_sParentId         = "0";
    request.m_eBrowseFlag       = GetBrowseFlag( pRequest->m_mapParams[ "BrowseFlag"    ] );
    request.m_sFilter           = pRequest->m_mapParams[ "Filter"        ];
    request.m_nStartingIndex    = pRequest->m_mapParams[ "StartingIndex" ].toLong();
    request.m_nRequestedCount   = pRequest->m_mapParams[ "RequestedCount"].toLong();
    request.m_sSortCriteria     = pRequest->m_mapParams[ "SortCriteria"  ];

/*
    VERBOSE(VB_UPNP,QString("UPnpCDS::ProcessRequest \n"
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
*/

    UPnPResultCode eErrorCode      = UPnPResult_CDS_NoSuchObject;
    QString        sErrorDesc      = "";
    short          nNumberReturned = 0;
    short          nTotalMatches   = 0;
    short          nUpdateID       = 0;
    QString        sResultXML;

    if (request.m_sObjectId == "0")
    {
        // ------------------------------------------------------------------
        // This is for the root object... lets handle it.
        // ------------------------------------------------------------------

        switch( request.m_eBrowseFlag )
        { 
            case CDS_BrowseMetadata:
            {
                // ----------------------------------------------------------------------
                // Return Root Object Only
                // ----------------------------------------------------------------------
        
                eErrorCode      = UPnPResult_Success;
                nNumberReturned = 1;
                nTotalMatches   = 1;
                nUpdateID       = m_root.m_nUpdateId;

                m_root.SetChildCount( m_extensions.count() );

                sResultXML      = m_root.toXml();

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

                UPnpCDSExtension    *pExtension = m_extensions.at( nStart );
                UPnpCDSRequest       childRequest;

                childRequest.m_sParentId         = "0";
                childRequest.m_eBrowseFlag       = CDS_BrowseMetadata;
                childRequest.m_sFilter           = "";
                childRequest.m_nStartingIndex    = 0;
                childRequest.m_nRequestedCount   = 1;
                childRequest.m_sSortCriteria     = "";

                while (( pExtension != NULL ) && (nNumberReturned < nCount ))
                {
                    childRequest.m_sObjectId = pExtension->m_sExtensionId;

                    pResult = pExtension->Browse( &childRequest );

                    if (pResult != NULL)
                    {
                        if (pResult->m_eErrorCode == UPnPResult_Success)
                        {
                            sResultXML  += pResult->GetResultXML();
                            nNumberReturned ++;
                        }

                        delete pResult;
                    }

                    pExtension = m_extensions.next();
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

        UPnpCDSExtension *pExtension = m_extensions.first();

        while (( pExtension != NULL ) && (pResult == NULL))
        {
            if ( request.m_sObjectId.startsWith( pExtension->m_sExtensionId, true ))
            {
                pResult = pExtension->Browse(  &request );
            }

            pExtension = m_extensions.next();
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
                sResultXML      = pResult->GetResultXML();
            }

            delete pResult;
        }
    }
    
    // ----------------------------------------------------------------------
    // Output Results of Browse Method
    // ----------------------------------------------------------------------

    if (eErrorCode == UPnPResult_Success)
    {
        NameValueList list;

        QString sResults = DIDL_LITE_BEGIN;
        sResults += sResultXML;
        sResults += DIDL_LITE_END;

        list.append( new NameValue( "Result"        , sResults                          ));
        list.append( new NameValue( "NumberReturned", QString::number( nNumberReturned )));
        list.append( new NameValue( "TotalMatches"  , QString::number( nTotalMatches   )));
        list.append( new NameValue( "UpdateID"      , QString::number( nUpdateID       )));

        pRequest->FormatActionResponse( &list );
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

    request.m_sObjectId         = pRequest->m_mapParams[ "ContainerID"   ];
    request.m_sFilter           = pRequest->m_mapParams[ "Filter"        ];
    request.m_nStartingIndex    = pRequest->m_mapParams[ "StartingIndex" ].toLong();
    request.m_nRequestedCount   = pRequest->m_mapParams[ "RequestedCount"].toLong();
    request.m_sSortCriteria     = pRequest->m_mapParams[ "SortCriteria"  ];
    request.m_sSearchCriteria   = pRequest->m_mapParams[ "SearchCriteria"];

    // ----------------------------------------------------------------------
    // Break the SearchCriteria into it's parts 
    // -=>TODO: This DOES NOT handle ('s or other complex expressions
    // ----------------------------------------------------------------------

    QRegExp  rMatch( "\\b(or|and)\\b" );
             rMatch.setCaseSensitive( FALSE );

    request.m_sSearchList  = QStringList::split( rMatch, request.m_sSearchCriteria );
    request.m_sSearchClass = "object";  // Default to all objects.

    // ----------------------------------------------------------------------
    // -=>TODO: Need to process all expressions in searchCriteria... for now,
    //          Just focus on the "upnp:class derivedfrom" expression
    // ----------------------------------------------------------------------

    for ( QStringList::Iterator it  = request.m_sSearchList.begin(); 
                                it != request.m_sSearchList.end(); 
                              ++it ) 
    {
        if ( (*it).contains( "upnp:class derivedfrom", FALSE ) > 0)
        {
            QStringList sParts = QStringList::split( ' ', *it );

            if (sParts.count() > 2)
            {
                request.m_sSearchClass = sParts[2].stripWhiteSpace();
                request.m_sSearchClass.remove( '"' );

                break;
            }
        }
    }

    // ----------------------------------------------------------------------

/*
    VERBOSE(VB_UPNP,QString("UPnpCDS::ProcessRequest \n"
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
*/

    UPnpCDSExtension    *pExtension = m_extensions.first();

    bool bSearchDone = false;

    while (( pExtension != NULL ) && ( !bSearchDone ))
    {

        pResult = pExtension->Search( &request );

        if (pResult != NULL)
        {
            eErrorCode  = pResult->m_eErrorCode;
            sErrorDesc  = pResult->m_sErrorDesc;

            if (eErrorCode == UPnPResult_Success)
            {
                nNumberReturned = pResult->m_List.count();
                nTotalMatches   = pResult->m_nTotalMatches;
                nUpdateID       = pResult->m_nUpdateID;
                sResultXML      = pResult->GetResultXML();
                bSearchDone = true;
            }

            delete pResult;
        }

        pExtension = m_extensions.next();

    }

    // nUpdateID       = 0;
    //VERBOSE(VB_UPNP,sResultXML);

    if (eErrorCode == UPnPResult_Success)
    {
        NameValueList list;
        QString sResults = DIDL_LITE_BEGIN;
        sResults += sResultXML;
        sResults += DIDL_LITE_END;

        list.append( new NameValue( "Result"        , sResults                          ));
        list.append( new NameValue( "NumberReturned", QString::number( nNumberReturned )));
        list.append( new NameValue( "TotalMatches"  , QString::number( nTotalMatches   )));
        list.append( new NameValue( "UpdateID"      , QString::number( nUpdateID       )));

        pRequest->FormatActionResponse( &list );
    }
    else
        UPnp::FormatErrorResponse( pRequest, eErrorCode, sErrorDesc );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSearchCapabilities( HTTPRequest *pRequest )
{
    NameValueList list;

    VERBOSE(VB_UPNP,QString("UPnpCDS::ProcessRequest : %1 : %2")
                       .arg(pRequest->m_sBaseUrl)
                       .arg(pRequest->m_sMethod));

    // -=>TODO: Need to implement based on CDS Extension Capabilities

    list.append( new NameValue( "SearchCaps", "dc:title,dc:creator,dc:date,upnp:class,res@size" ));

    pRequest->FormatActionResponse( &list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSortCapabilities( HTTPRequest *pRequest )
{
    NameValueList list;

    VERBOSE(VB_UPNP,QString("UPnpCDS::ProcessRequest : %1 : %2")
                       .arg(pRequest->m_sBaseUrl)
                       .arg(pRequest->m_sMethod));

    // -=>TODO: Need to implement based on CDS Extension Capabilities

    list.append( new NameValue( "SortCaps", "dc:title,dc:creator,dc:date,upnp:class,res@size" ));

    pRequest->FormatActionResponse( &list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSystemUpdateID( HTTPRequest *pRequest )
{
    NameValueList list;

    VERBOSE(VB_UPNP,QString("UPnpCDS::ProcessRequest : %1 : %2")
                       .arg(pRequest->m_sBaseUrl)
                       .arg(pRequest->m_sMethod));

    unsigned short nId = GetValue< unsigned short >( "SystemUpdateID" );

    list.append( new NameValue( "Id", QString::number( nId ) ));

    pRequest->FormatActionResponse( &list );
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

UPnpCDSExtensionResults *UPnpCDSExtension::Browse( UPnpCDSRequest *pRequest )
{

    // -=>TODO: Need to add Filter & Sorting Support.
    // -=>TODO: Need to add Sub-Folder/Category Support!!!!!

    if (! pRequest->m_sObjectId.startsWith( m_sExtensionId, true ))
        return( NULL );

    // ----------------------------------------------------------------------
    // Parse out request object's path
    // ----------------------------------------------------------------------

    QStringList idPath = QStringList::split( "/", pRequest->m_sObjectId.section('=',0,0) );

    QString key = pRequest->m_sObjectId.section('=',1);

    if (idPath.count() == 0)
        return( NULL );

    // ----------------------------------------------------------------------
    // Process based on location in hierarchy
    // ----------------------------------------------------------------------

    UPnpCDSExtensionResults *pResults = new UPnpCDSExtensionResults();

    if (pResults != NULL)
    {
        if (key)  
            idPath.last().append(QString("=%1").arg(key)); 

        QString sLast = idPath.last();

        pRequest->m_sParentId = pRequest->m_sObjectId;

        if (sLast == m_sExtensionId         ) { return( ProcessRoot   ( pRequest, pResults, idPath )); }
        if (sLast == "0"                    ) { return( ProcessAll    ( pRequest, pResults, idPath )); }
        if (sLast.startsWith( "key" , true )) { return( ProcessKey    ( pRequest, pResults, idPath )); }
        if (sLast.startsWith( "item", true )) { return( ProcessItem   ( pRequest, pResults, idPath )); }

        int nNodeIdx = sLast.toInt();

        if ((nNodeIdx > 0) && (nNodeIdx < GetRootCount()))
            return( ProcessContainer( pRequest, pResults, nNodeIdx, idPath ));

        pResults->m_eErrorCode = UPnPResult_CDS_NoSuchObject;
        pResults->m_sErrorDesc = "";
    }

    return( pResults );        
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSExtension::Search( UPnpCDSRequest *pRequest )
{
    // -=>TODO: Need to add Filter & Sorting Support.
    // -=>TODO: Need to add Sub-Folder/Category Support!!!!!

    QStringList sEmptyList;

     if ( !m_sClass.startsWith( pRequest->m_sSearchClass ))
        return NULL;

    UPnpCDSExtensionResults *pResults = new UPnpCDSExtensionResults();

    CreateItems( pRequest, pResults, 0, "", false );

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSExtension::RemoveToken( const QString &sToken, const QString &sStr, int num )
{
    QString sResult( "" );
    int     nPos = -1;

    for (int nIdx=0; nIdx < num; nIdx++)
    {
        if ((nPos = sStr.findRev( sToken, nPos )) == -1)
            break;
    }

    if (nPos > 0)
        sResult = sStr.left( nPos );

    return sResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSExtension::ProcessRoot( UPnpCDSRequest          *pRequest, 
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
            pResults->m_nUpdateID     = 1;
            pResults->m_nTotalMatches = nRootCount ;
            
            if ( pRequest->m_nRequestedCount == 0)
                pRequest->m_nRequestedCount = nRootCount ;

            short nStart = Max( pRequest->m_nStartingIndex, short( 0 ));
            short nEnd   = Min( nRootCount, short( nStart + pRequest->m_nRequestedCount));

            if (nStart < nRootCount)
            {
                for (short nIdx = nStart; nIdx < nEnd; nIdx++)
                {
                    UPnpCDSRootInfo *pInfo = GetRootInfo( nIdx );

                    if (pInfo != NULL)
                    {

                        QString sId = QString( "%1/%2" ).arg( pRequest->m_sObjectId   )
                                                        .arg( nIdx );

                        CDSObject *pItem = CreateContainer( sId,  
                                                            QObject::tr( pInfo->title ), 
                                                            m_sExtensionId );

                        pItem->SetChildCount( GetDistinctCount( pInfo->column ) );

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

UPnpCDSExtensionResults *UPnpCDSExtension::ProcessAll ( UPnpCDSRequest          *pRequest,
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

                CDSObject *pItem = CreateContainer( pRequest->m_sObjectId,  
                                                    QObject::tr( pInfo->title ), 
                                                    m_sExtensionId );

                pItem->SetChildCount( GetDistinctCount( pInfo->column ) );

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

UPnpCDSExtensionResults *UPnpCDSExtension::ProcessItem( UPnpCDSRequest          *pRequest,
                                                        UPnpCDSExtensionResults *pResults, 
                                                        QStringList             &idPath )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------
    
    if ( pRequest->m_eBrowseFlag == CDS_BrowseMetadata )
    {
        // --------------------------------------------------------------
        // Return 1 Item
        // --------------------------------------------------------------

        QStringMap  mapParams;
        QString     sParams = idPath.last().section( '?', 1, 1 );
            
        sParams.replace(QRegExp( "&amp;"), "&" ); 

        HTTPRequest::GetParameters( sParams, mapParams );

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.isConnected())                                                           
        {
            BuildItemQuery( query, mapParams );

            query.exec();

            if (query.isActive() && query.size() > 0)
            {
                if ( query.next() )
                {
                    pRequest->m_sObjectId = RemoveToken( "/", pRequest->m_sObjectId, 1 );

                    AddItem( pRequest->m_sObjectId, pResults, false, query );
                    pResults->m_nTotalMatches = 1;
                }
            }
        }
    }

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSExtension::ProcessKey( UPnpCDSRequest          *pRequest,
                                                       UPnpCDSExtensionResults *pResults, 
                                                       QStringList             &idPath )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------
    
    QString sKey = idPath.last().section( '=', 1, 1 );
    QUrl::decode( sKey );

    if (sKey.length() > 0)
    {
        int nNodeIdx = idPath[ idPath.count() - 2 ].toInt();

        switch( pRequest->m_eBrowseFlag )
        { 

            case CDS_BrowseMetadata:
            {                              
                UPnpCDSRootInfo *pInfo = GetRootInfo( nNodeIdx );

                if (pInfo == NULL)
                    return pResults;

                pRequest->m_sParentId = RemoveToken( "/", pRequest->m_sObjectId, 1 );

                // --------------------------------------------------------------
                // Since Key is not always the title, we need to lookup title.
                // --------------------------------------------------------------

                MSqlQuery query(MSqlQuery::InitCon());

                if (query.isConnected())
                {
                    QString sSQL   = QString( pInfo->sql )
                                        .arg( pInfo->where );

                    // -=>TODO: There is a problem when called for an Item, instead of a container
                    //          sKey = '<KeyName>/item?ChanId' which is incorrect.


                    query.prepare  ( sSQL );
                    query.bindValue( ":KEY", sKey );
                    query.exec();

                    if (query.isActive() && query.size() > 0)
                    {
                        if ( query.next() )
                        {
                            // ----------------------------------------------
                            // Return Container Object Only
                            // ----------------------------------------------

                            pResults->m_nTotalMatches   = 1;
                            pResults->m_nUpdateID       = 1;

                            CDSObject *pItem = CreateContainer( pRequest->m_sObjectId,  
                                                                query.value(1).toString(), 
                                                                pRequest->m_sParentId );

                            pItem->SetChildCount( GetDistinctCount( pInfo->column  ));

                            pResults->Add( pItem ); 
                        }
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

UPnpCDSExtensionResults *UPnpCDSExtension::ProcessContainer( UPnpCDSRequest          *pRequest,
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

            pItem->SetChildCount( GetDistinctCount( pInfo->column ));

            pResults->Add( pItem ); 

            break;
        }

        case CDS_BrowseDirectChildren:
        {
            pResults->m_nTotalMatches = GetDistinctCount( pInfo->column );
            pResults->m_nUpdateID     = 1;

            if (pRequest->m_nRequestedCount == 0) 
                pRequest->m_nRequestedCount = SHRT_MAX;

            MSqlQuery query(MSqlQuery::InitCon());

            if (query.isConnected())
            {
                // Remove where clause placeholder.

                QString sSQL = pInfo->sql;

                sSQL.replace( "%1", "" );

                sSQL += QString( " LIMIT %2, %3" )
                           .arg( pRequest->m_nStartingIndex  )
                           .arg( pRequest->m_nRequestedCount );

                query.prepare( sSQL );
                query.exec();

                if (query.isActive() && query.size() > 0)
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

                        CDSObject *pRoot = CreateContainer( sId, sTitle, pRequest->m_sParentId );

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

int UPnpCDSExtension::GetDistinctCount( const QString &sColumn )
{
    int nCount = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        // Note: Tried to use Bind, however it would not allow me to use it
        //       for column & table names

        QString sSQL;
        
        if (sColumn == "*")
        {
            sSQL = QString( "SELECT count( %1 ) FROM %2" )
                      .arg( sColumn )
                      .arg( GetTableName( sColumn ));
        }
        else
        {
            sSQL = QString( "SELECT count( DISTINCT %1 ) FROM %2" )
                      .arg( sColumn )
                      .arg( GetTableName( sColumn ) );
        }

        query.prepare( sSQL );
        query.exec();

        if (query.size() > 0)
        {
            query.next();

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
        // Note: Tried to use Bind, however it would not allow me to use it
        //       for column & table names

        QString sSQL;
        
        if (sColumn == "*")
            sSQL = QString( "SELECT count( * ) FROM %1" ).arg( GetTableName( sColumn ) );
        else
            sSQL = QString( "SELECT count( %1 ) FROM %2 WHERE %3=:KEY" )
                      .arg( sColumn )
                      .arg( GetTableName( sColumn ) )
                      .arg( sColumn );

        query.prepare( sSQL );
        query.bindValue( ":KEY", sKey );
        query.exec();

        if (query.size() > 0)
        {
            query.next();

            nCount = query.value(0).toInt();
        }

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

        if ( sKey.length() > 0)
        {
           sWhere = QString( "WHERE %1=:KEY " )
                       .arg( pInfo->column );
        }

        QString sSQL = QString( "%1 %2 LIMIT %3, %4" )
                          .arg( GetItemListSQL( pInfo->column ) )
                          .arg( sWhere )
                          .arg( pRequest->m_nStartingIndex  )
                          .arg( pRequest->m_nRequestedCount );

        query.prepare  ( sSQL );
        query.bindValue(":KEY", sKey );
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
            while(query.next())
                AddItem( pRequest->m_sObjectId, pResults, bAddRef, query );
        }
    }
}
