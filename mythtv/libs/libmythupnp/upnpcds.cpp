//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcds.cpp
//                                                                            
// Purpose - uPnp Content Directory Service 
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpcds.h"

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

    QString sSharePath    = gContext->GetShareDir();
    QString sUPnpDescPath = gContext->GetSetting("upnpDescXmlPath", sSharePath);

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

        VERBOSE(VB_UPNP,QString("UPnpCDS::ProcessRequest : %1 : %2 :")
                        .arg(pRequest->m_sBaseUrl)
                        .arg(pRequest->m_sMethod));

        switch( GetMethod( pRequest->m_sMethod ) )
        {
            case CDSM_GetServiceDescription : pRequest->FormatFileResponse( m_sServiceDescFileName ); break;
            case CDSM_Browse                : HandleBrowse                ( pRequest ); break;
            case CDSM_Search                : HandleSearch                ( pRequest ); break;
            case CDSM_GetSearchCapabilities : HandleGetSearchCapabilities ( pRequest ); break;
            case CDSM_GetSortCapabilities   : HandleGetSortCapabilities   ( pRequest ); break;
            case CDSM_GetSystemUpdateID     : HandleGetSystemUpdateID     ( pRequest ); break;
            default:
                pRequest->FormatErrorReponse( 401, "Invalid Action" );
                pRequest->m_nResponseStatus = 401; //501;
                break;
        }       

        return true;
    }

    return false;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString &UPnpCDS::Encode( QString &sStr )
{
    sStr.replace(QRegExp( "&"), "&amp;" ); // This _must_ come first
    sStr.replace(QRegExp( "<"), "&lt;"  );
    sStr.replace(QRegExp( ">"), "&gt;"  );
//    sStr.replace(QRegExp("\""), "&quot;");
//    sStr.replace(QRegExp( "'"), "&apos;");

    return( sStr );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleBrowse( HTTPRequest *pRequest )
{
    UPnpCDSExtensionResults *pResult  = NULL;
    UPnpCDSBrowseRequest     request;

    request.m_sObjectId         = pRequest->m_mapParams[ "ObjectID"      ];
    request.m_sParentId         = "0";
    request.m_eBrowseFlag       = GetBrowseFlag( pRequest->m_mapParams[ "BrowseFlag"    ] );
    request.m_sFilter           = pRequest->m_mapParams[ "Filter"        ];
    request.m_nStartingIndex    = pRequest->m_mapParams[ "StartingIndex" ].toLong();
    request.m_nRequestedCount   = pRequest->m_mapParams[ "RequestedCount"].toLong();
    request.m_sSortCriteria     = pRequest->m_mapParams[ "SortCriteria"  ];

    short   nErrorCode      = 701;
    QString sErrorDesc      = "No such object";
    short   nNumberReturned = 0;
    short   nTotalMatches   = 0;
    short   nUpdateID       = 0;
    QString sResultXML;

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
        
                nErrorCode      = 0;
                sErrorDesc      = "";
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

                nErrorCode      = 0;
                sErrorDesc      = "";
                nNumberReturned = m_extensions.count();
                nTotalMatches   = m_extensions.count();
                nUpdateID       = m_root.m_nUpdateId;

                UPnpCDSExtension    *pExtension = m_extensions.first();

                request.m_sParentId         = "0";
                request.m_eBrowseFlag       = CDS_BrowseMetadata;
                request.m_sFilter           = "";
                request.m_nStartingIndex    = 0;
                request.m_nRequestedCount   = 1;
                request.m_sSortCriteria     = "";

                while ( pExtension != NULL )
                {
                    request.m_sObjectId = pExtension->m_sExtensionId;

                    pResult = pExtension->Browse( &request );

                    if (pResult != NULL)
                    {
                        if (pResult->m_nErrorCode == 0)
                            sResultXML  += pResult->GetResultXML();

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
            nErrorCode  = pResult->m_nErrorCode;
            sErrorDesc  = pResult->m_sErrorDesc;

            if (nErrorCode == 0)
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

    if (nErrorCode == 0)
    {
        NameValueList list;

        QString sResults = DIDL_LITE_BEGIN;
        sResults += sResultXML;
        sResults += DIDL_LITE_END;

        list.append( new NameValue( "Result"        , Encode( sResults                 )));
        list.append( new NameValue( "NumberReturned", QString::number( nNumberReturned )));
        list.append( new NameValue( "TotalMatches"  , QString::number( nTotalMatches   )));
        list.append( new NameValue( "UpdateID"      , QString::number( nUpdateID       )));

        pRequest->FormatActionReponse( &list );
    }
    else
        pRequest->FormatErrorReponse ( nErrorCode, sErrorDesc );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleSearch( HTTPRequest *pRequest )
{
    UPnpCDSExtensionResults *pResult  = NULL;
    UPnpCDSSearchRequest     request;

    QString old_sClass      = m_root.m_sClass;
    short   nErrorCode      = 401;
    QString sErrorDesc      = "Invalid Action";
    short   nNumberReturned = 0;
    short   nTotalMatches   = 0;
    short   nUpdateID       = 0;
    QString sResultXML;

    request.m_sContainerID      = pRequest->m_mapParams[ "ContainerID"   ];
    request.m_sFilter           = pRequest->m_mapParams[ "Filter"        ];
    request.m_nStartingIndex    = pRequest->m_mapParams[ "StartingIndex" ].toLong();
    request.m_nRequestedCount   = pRequest->m_mapParams[ "RequestedCount"].toLong();
    request.m_sSortCriteria     = pRequest->m_mapParams[ "SortCriteria"  ];
    request.m_sSearchCriteria   = "";
    m_root.m_sClass = pRequest->m_mapParams[ "SearchCriteria"];

    //pRequest->m_mapParams[ "SearchCriteria"];

    nErrorCode      = 0;
    sErrorDesc      = "";

    UPnpCDSExtension    *pExtension = m_extensions.first();

    bool bSearchDone = false;

    while (( pExtension != NULL ) && ( !bSearchDone ))
    {

        pResult = pExtension->Search( &request );

        if (pResult != NULL)
        {
            nErrorCode  = pResult->m_nErrorCode;
            sErrorDesc  = pResult->m_sErrorDesc;

            if (nErrorCode == 0)
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

    if (nErrorCode == 0)
    {
        NameValueList list;
        QString sResults = DIDL_LITE_BEGIN;
        sResults += sResultXML;
        sResults += DIDL_LITE_END;

        list.append( new NameValue( "Result"        , Encode( sResults                 )));
        list.append( new NameValue( "NumberReturned", QString::number( nNumberReturned )));
        list.append( new NameValue( "TotalMatches"  , QString::number( nTotalMatches   )));
        list.append( new NameValue( "UpdateID"      , QString::number( nUpdateID       )));

        pRequest->FormatActionReponse( &list );
    }
    else
        pRequest->FormatErrorReponse ( nErrorCode, sErrorDesc );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSearchCapabilities( HTTPRequest *pRequest )
{
    NameValueList list;

    // -=>TODO: Need to implement based on CDS Extension Capabilities

    list.append( new NameValue( "SearchCaps", "dc:title,dc:creator,dc:date,upnp:class,res@size" ));

    pRequest->FormatActionReponse( &list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSortCapabilities( HTTPRequest *pRequest )
{
    NameValueList list;

    // -=>TODO: Need to implement based on CDS Extension Capabilities

    list.append( new NameValue( "SortCaps", "dc:title,dc:creator,dc:date,upnp:class,res@size" ));

    pRequest->FormatActionReponse( &list );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDS::HandleGetSystemUpdateID( HTTPRequest *pRequest )
{
    NameValueList list;

    unsigned short nId = GetValue< unsigned short >( "SystemUpdateID" );

    list.append( new NameValue( "Id", QString::number( nId ) ));

    pRequest->FormatActionReponse( &list );
}

