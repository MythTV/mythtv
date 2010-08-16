// Program Name: upnpcdsvideo.cpp
//
// Purpose - uPnp Content Directory Extension for MythVideo Videos
//
//////////////////////////////////////////////////////////////////////////////

// POSIX headers
#include <limits.h>

// Qt headers
#include <QFileInfo>

// MythTV headers
#include "upnpcdsvideo.h"
#include "httprequest.h"
#include "upnpmedia.h"
#include "util.h"
#include "mythcorecontext.h"

#define LOC QString("UPnpCDSVideo: ")
#define LOC_WARN QString("UPnpCDSVideo, Warning: ")
#define LOC_ERR QString("UPnpCDSVideo, Error: ")

UPnpCDSRootInfo UPnpCDSVideo::g_RootNodes[] =
{
    {   "VideoRoot",
        "*",
        "SELECT 0 as key, "
          "title as name, "
          "1 as children "
            "FROM upnpmedia "
            "%1 "
            "ORDER BY title",
        "", "title" }

};

int UPnpCDSVideo::g_nRootCount = 1;

//int UPnpCDSVideo::g_nRootCount;
//= sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSVideo::GetRootInfo( int nIdx )
{
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]);

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSVideo::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetTableName( QString sColumn )
{
    return "upnpmedia";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetItemListSQL( QString sColumn )
{
    return "SELECT intid, title, filepath, " \
           "itemtype, itemproperties, parentid, "\
           "coverart FROM upnpmedia WHERE class = 'VIDEO'";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int nVideoID = mapParams[ "Id" ].toInt();

    QString sSQL = QString( "%1 AND intid=:VIDEOID ORDER BY title DESC" )
                                                    .arg( GetItemListSQL( ) );

    query.prepare( sSQL );

    query.bindValue( ":VIDEOID", (int)nVideoID    );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSVideo::IsBrowseRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // ----------------------------------------------------------------------
    // Xbox360 compatibility code.
    // ----------------------------------------------------------------------

    if (pRequest->m_eClient == CDS_ClientXBox && 
        pRequest->m_sContainerID == "15" &&
        gCoreContext->GetSetting("UPnP/WMPSource") == "1") 
    {
        pRequest->m_sObjectId = "Videos/0";

        VERBOSE( VB_UPNP, "UPnpCDSVideo::IsBrowseRequestForUs - Yes ContainerID == 15" );
        return true;
    }

    if ((pRequest->m_sObjectId.isEmpty()) && 
        (!pRequest->m_sContainerID.isEmpty()))
        pRequest->m_sObjectId = pRequest->m_sContainerID;

    // ----------------------------------------------------------------------
    // WMP11 compatibility code
    //
    // In this mode browsing for "Videos" is forced to either Videos (us)
    // or RecordedTV (handled by upnpcdstv)
    //
    // ----------------------------------------------------------------------

    if (pRequest->m_eClient == CDS_ClientWMP && 
        pRequest->m_sContainerID == "13" &&
        pRequest->m_nClientVersion < 12.0 &&
        gCoreContext->GetSetting("UPnP/WMPSource") == "1")
    {
        pRequest->m_sObjectId = "Videos/0";

        VERBOSE( VB_UPNP, "UPnpCDSVideo::IsBrowseRequestForUs - Yes ContainerID == 13" );
        return true;
    }

    VERBOSE( VB_UPNP, "UPnpCDSVideo::IsBrowseRequestForUs - Not sure... Calling base class." );

    return UPnpCDSExtension::IsBrowseRequestForUs( pRequest );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSVideo::IsSearchRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // ----------------------------------------------------------------------
    // XBox 360 compatibility code
    // ----------------------------------------------------------------------


    if (pRequest->m_eClient == CDS_ClientXBox && 
        pRequest->m_sContainerID == "15" &&
        gCoreContext->GetSetting("UPnP/WMPSource") == "1") 
    {
        pRequest->m_sObjectId = "Videos/0";

        VERBOSE( VB_UPNP, "UPnpCDSVideo::IsSearchRequestForUs... Yes." );

        return true;
    }

    if ((pRequest->m_sObjectId.isEmpty()) && 
        (!pRequest->m_sContainerID.isEmpty()))
        pRequest->m_sObjectId = pRequest->m_sContainerID;

    // ----------------------------------------------------------------------

    bool bOurs = UPnpCDSExtension::IsSearchRequestForUs( pRequest );

    // ----------------------------------------------------------------------
    // WMP11 compatibility code
    // ----------------------------------------------------------------------

    if ( bOurs && pRequest->m_eClient == CDS_ClientWMP && 
         pRequest->m_nClientVersion < 12.0 )
    {
        if ( gCoreContext->GetSetting("UPnP/WMPSource") == "1")
        {
            pRequest->m_sObjectId = "Videos/0";
            // -=>TODO: Not sure why this was added.
            pRequest->m_sParentId = "8"; 
        }
        else
            bOurs = false;
    }

    return bOurs;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSVideo::GetDistinctCount( UPnpCDSRootInfo *pInfo )
{
    int nCount = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT COUNT(*) FROM upnpmedia WHERE class = 'VIDEO' "
                    "AND parentid = :ROOTID");

    query.bindValue(":ROOTID", STARTING_VIDEO_OBJECTID);

    if (query.exec() && query.next())
    {
        nCount = query.value(0).toInt();
    }

    return nCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSExtensionResults *UPnpCDSVideo::ProcessItem( UPnpCDSRequest          *pRequest,
                                                    UPnpCDSExtensionResults *pResults,
                                                    QStringList             &idPath )
{
    pResults->m_nTotalMatches   = 0;
    pResults->m_nUpdateID       = 1;

    if (pRequest->m_sObjectId.length() == 0)
        return pResults;

    QStringList tokens = pRequest->m_sObjectId
        .split('/', QString::SkipEmptyParts);
    QString     sId    = tokens.last();

    if (sId.startsWith("Id"))
        sId = sId.right( sId.length() - 2);

    switch( pRequest->m_eBrowseFlag )
    {
        case CDS_BrowseMetadata:
        {
            // --------------------------------------------------------------
            // Return 1 Item
            // --------------------------------------------------------------

            QStringMap  mapParams;

            mapParams.insert( "Id", sId );

            MSqlQuery query(MSqlQuery::InitCon());

            if (query.isConnected())
            {
                BuildItemQuery( query, mapParams );

                if (query.exec() && query.next())
                {
                        AddItem( pRequest, pRequest->m_sParentId, pResults, false, query );
                        pResults->m_nTotalMatches = 1;
                }
            }

            break;
        }

        case CDS_BrowseDirectChildren:
        {
            pRequest->m_sParentId = sId;

            CreateItems( pRequest, pResults, 0, "", false );

            break;
        }
    }

    return pResults;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::CreateItems( UPnpCDSRequest          *pRequest,
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

    if (pRequest->m_nRequestedCount == 0)
        pRequest->m_nRequestedCount = SHRT_MAX;

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        QString ParentClause;
        QString sWhere;

        if ( sKey.length() > 0)
        {
           sWhere = QString( "WHERE %1=:KEY " )
                       .arg( pInfo->column );
        }

        if (pRequest->m_sObjectId.startsWith("Videos"))
        {
            if (!pRequest->m_sParentId.isEmpty())
            {
                if (pRequest->m_sParentId == "Videos/0")
                {
                    pRequest->m_sParentId = QString("%1")
                                .arg(STARTING_VIDEO_OBJECTID);
                }
            }
            else
            {
                QStringList tokens =
                    pRequest->m_sObjectId.split('=', QString::SkipEmptyParts);
                pRequest->m_sParentId = tokens.last();
            }

            if (pRequest->m_sSearchClass.isEmpty())
                ParentClause = " AND parentid = \"" + pRequest->m_sParentId + "\"";
            else
                pRequest->m_sParentId = '8';

            if (pRequest->m_sObjectId.startsWith("Videos/0"))
            {
                pRequest->m_sObjectId = "Videos/0";
            }

            /*
            VERBOSE(VB_UPNP, QString("pRequest->m_sParentId=:%1: , "
                                     "pRequest->m_sObjectId=:%2:, sKey=:%3:")
                                                 .arg(pRequest->m_sParentId)
                                                 .arg(pRequest->m_sObjectId)
                                                 .arg(sKey));
             */

            if ((!pRequest->m_sParentId.isEmpty()) && (pRequest->m_sParentId != "8"))
                pResults->m_nTotalMatches = GetCount( "parentid", pRequest->m_sParentId );
        }
        else
            VERBOSE( VB_UPNP, QString( "UPnpCDSVideo::CreateItems: ******* ParentID Does NOT Start with 'Videos' ParentId = {0}" )
                                  .arg( pRequest->m_sParentId ));

        QString sSQL = QString( "%1 %2 LIMIT %3, %4" )
                          .arg( GetItemListSQL( pInfo->column )  )
                          .arg( sWhere + ParentClause )
                          .arg( pRequest->m_nStartingIndex  )
                          .arg( pRequest->m_nRequestedCount );

        query.prepare  ( sSQL );
        //VERBOSE(VB_UPNP, QString("sSQL = %1").arg(sSQL));
        if ( sKey.length() )
            query.bindValue(":KEY", sKey );

        if (query.exec())
        {
            while(query.next())
                AddItem( pRequest, pRequest->m_sObjectId, pResults, bAddRef, query );

        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::AddItem( const UPnpCDSRequest    *pRequest,
                            const QString           &sObjectId,
                            UPnpCDSExtensionResults *pResults,
                            bool                     bAddRef,
                            MSqlQuery               &query )
{
    int            nVidID       = query.value( 0).toInt();
    QString        sTitle       = query.value( 1).toString();
    QString        sFileName    = query.value( 2).toString();
    QString        sItemType    = query.value( 3).toString();
    QString        sParentID    = query.value( 5).toString();
    QString        sCoverArt    = query.value( 6).toString();

    // VERBOSE(VB_UPNP,QString("ID = %1, Title = %2, fname = %3 sObjectId = %4").arg(nVidID).arg(sTitle).arg(sFileName).arg(sObjectId));

    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------
    QString sServerIp = gCoreContext->GetSetting("BackendServerIp"   );
    QString sPort     = gCoreContext->GetSetting("BackendStatusPort" );

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sName      = sTitle;

    QString sURIBase   = QString( "http://%1:%2/Myth/" )
                            .arg( sServerIp )
                            .arg( sPort     );

    QString sURIParams = QString( "/Id%1" ).arg( nVidID );
    QString sId        = QString( "Videos/0/item%1").arg( sURIParams );

    if (sParentID == QString("%1").arg(STARTING_VIDEO_OBJECTID))
    {
        sParentID = "Videos/0";
    }
    else
    {
        sParentID = QString( "Videos/0/item/Id%1")
                       .arg( sParentID );
    }

    QString sAlbumArtURI= QString( "%1GetVideoArt%2")
                        .arg( sURIBase   )
                        .arg( sURIParams );

    CDSObject *pItem = NULL;

    if (sItemType == "FOLDER")
    {
        pItem   = CDSObject::CreateStorageFolder( sId, sName, sParentID);
        pItem->SetChildCount( GetCount( "parentid",QString( "%1" ).arg( nVidID )) );

        pItem->SetPropValue( "storageUsed", "0" );  //-=>TODO: need proper value
    }
    else if (sItemType == "FILE" )
        pItem   = CDSObject::CreateVideoItem( sId, sName, sParentID );

    if (!pItem)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "AddItem(): " +
        QString("sItemType has unknown type '%1'").arg(sItemType));

        return;
    }

    pItem->m_bRestricted  = false;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "WRITABLE";

    pItem->SetPropValue( "genre"          , "[Unknown Genre]"     );
    pItem->SetPropValue( "actor"          , "[Unknown Author]"    );
    pItem->SetPropValue( "creator"        , "[Unknown Author]"    );
    pItem->SetPropValue( "album"          , "[Unknown Series]"    );

    if ((!sCoverArt.isEmpty()) && (sCoverArt != "No Cover"))
        pItem->SetPropValue( "albumArtURI"    , sAlbumArtURI);

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item%2").arg( m_sExtensionId )
                                                .arg( sURIParams     );

        pItem->SetPropValue( "refID", sRefId );
    }

    QFileInfo fInfo( sFileName );
    QDateTime fDate = fInfo.lastModified();

    pItem->SetPropValue( "date", fDate.toString( "yyyy-MM-dd"));
    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.suffix() );
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;"
                                 "DLNA.ORG_FLAGS=0150000000000000000000000000"
                                 "0000" ).arg( sMimeType  );
    QString sURI      = QString( "%1GetVideo%2").arg( sURIBase   )
                                                .arg( sURIParams );

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

    pRes->AddAttribute( "size"      , QString("%1").arg(fInfo.size()) );
    pRes->AddAttribute( "duration"  , "0:01:00.000"      );

}

// vim:ts=4:sw=4:ai:et:si:sts=4
