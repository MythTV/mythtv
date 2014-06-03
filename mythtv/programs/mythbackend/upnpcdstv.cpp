//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdstv.cpp
//
// Purpose - uPnp Content Directory Extension for Recorded TV
//
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

// POSIX headers
#include <limits.h>
#include <stdint.h>

// MythTV headers
#include "upnpcdstv.h"
#include "httprequest.h"
#include "storagegroup.h"
#include "mythdate.h"
#include "mythcorecontext.h"


/*
   Recordings                              RecTv
    - All Programs                         RecTv/All
      + <recording 1>                      RecTv/All/item?ChanId=1004&StartTime=2006-04-06T20:00:00
      + <recording 2>
      + <recording 3>
    - By Title                             RecTv/title
      - <title 1>                          RecTv/title/key=Stargate SG-1
        + <recording 1>                    RecTv/title/key=Stargate SG-1/item?ChanId=1004&StartTime=2006-04-06T20:00:00
        + <recording 2>
    - By Genre
    - By Date
    - By Channel
    - By Group
*/


UPnpCDSRootInfo UPnpCDSTv::g_RootNodes[] =
{
    {   "All Recordings",
        "*",
        "SELECT 0 as key, "
          "CONCAT( title, ': ', subtitle) as name, "
          "1 as children "
            "FROM recorded "
            "%1 "
            "ORDER BY starttime DESC",
        "", "starttime DESC" },

    {   "By Title",
        "title",
        "SELECT title as id, "
          "title as name, "
          "count( title ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY title "
            "ORDER BY title",
        "WHERE title=:KEY", "title" },

    {   "By Genre",
        "category",
        "SELECT category as id, "
          "category as name, "
          "count( category ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY category "
            "ORDER BY category",
        "WHERE category=:KEY", "category" },

    {   "By Date",
        "DATE_FORMAT(starttime, '%Y-%m-%d')",
        "SELECT  DATE_FORMAT(starttime, '%Y-%m-%d') as id, "
          "DATE_FORMAT(starttime, '%Y-%m-%d %W') as name, "
          "count( DATE_FORMAT(starttime, '%Y-%m-%d %W') ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY name "
            "ORDER BY starttime DESC",
        "WHERE DATE_FORMAT(starttime, '%Y-%m-%d') =:KEY", "starttime DESC" },

    {   "By Channel",
        "chanid",
        "SELECT channel.chanid as id, "
          "CONCAT(channel.channum, ' ', channel.callsign) as name, "
          "count( channum ) as children "
            "FROM channel "
                "INNER JOIN recorded ON channel.chanid = recorded.chanid "
            "%1 "
            "GROUP BY name "
            "ORDER BY channel.chanid",
        "WHERE channel.chanid=:KEY", ""},

    {   "By Group",
        "recgroup",
        "SELECT recgroup as id, "
          "recgroup as name, count( recgroup ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY recgroup "
            "ORDER BY recgroup",
        "WHERE recgroup=:KEY", "recgroup" }
};

int UPnpCDSTv::g_nRootCount = sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSTv::GetRootInfo( int nIdx )
{
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]);

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSTv::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSTv::GetTableName( QString /* sColumn */)
{
    return "recorded";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSTv::GetItemListSQL( QString /* sColumn */ )
{
    return "SELECT chanid, starttime, endtime, title, " \
                  "subtitle, description, category, "   \
                  "hostname, recgroup, filesize, "      \
                  "basename, progstart, progend, "      \
                  "storagegroup, inetref "              \
           "FROM recorded ";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSTv::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int     nChanId    = mapParams[ "ChanId"    ].toInt();
    QString sStartTime = mapParams[ "StartTime" ];

    QString sSQL = QString("%1 WHERE chanid=:CHANID AND starttime=:STARTTIME ")
                      .arg( GetItemListSQL() );

    query.prepare( sSQL );

    query.bindValue(":CHANID"   , (int)nChanId    );
    query.bindValue(":STARTTIME", sStartTime );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::IsBrowseRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // ----------------------------------------------------------------------
    // Xbox360 compatibility code.
    // ----------------------------------------------------------------------

    if (pRequest->m_eClient == CDS_ClientXBox && 
        pRequest->m_sContainerID == "15" &&
        gCoreContext->GetSetting("UPnP/WMPSource") != "1") 
    {
        pRequest->m_sObjectId = "Videos/0";

        LOG(VB_UPNP, LOG_INFO,
            "UPnpCDSTv::IsBrowseRequestForUs - Yes ContainerID == 15");
        return true;
    }

    // ----------------------------------------------------------------------
    // WMP11 compatibility code
    // ----------------------------------------------------------------------
    if (pRequest->m_eClient == CDS_ClientWMP && 
        pRequest->m_nClientVersion < 12.0 && 
        pRequest->m_sContainerID == "13" &&
        gCoreContext->GetSetting("UPnP/WMPSource") != "1")
    {
        pRequest->m_sObjectId = "RecTv/0";

        LOG(VB_UPNP, LOG_INFO,
            "UPnpCDSTv::IsBrowseRequestForUs - Yes, ObjectId == 13");
        return true;
    }

    LOG(VB_UPNP, LOG_INFO,
        "UPnpCDSTv::IsBrowseRequestForUs - Not sure... Calling base class.");

    return UPnpCDSExtension::IsBrowseRequestForUs( pRequest );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::IsSearchRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // ----------------------------------------------------------------------
    // XBox 360 compatibility code
    // ----------------------------------------------------------------------

    if (pRequest->m_eClient == CDS_ClientXBox && 
        pRequest->m_sContainerID == "15" &&
        gCoreContext->GetSetting("UPnP/WMPSource") !=  "1") 
    {
        pRequest->m_sObjectId = "Videos/0";

        LOG(VB_UPNP, LOG_INFO, "UPnpCDSTv::IsSearchRequestForUs... Yes.");

        return true;
    }


    if ((pRequest->m_sObjectId.isEmpty()) && 
        (!pRequest->m_sContainerID.isEmpty()))
        pRequest->m_sObjectId = pRequest->m_sContainerID;

    // ----------------------------------------------------------------------

    bool bOurs = UPnpCDSExtension::IsSearchRequestForUs( pRequest );

    // ----------------------------------------------------------------------
    // WMP11 compatibility code
    //
    // In this mode browsing for "Videos" is forced to either RecordedTV (us)
    // or Videos (handled by upnpcdsvideo)
    //
    // ----------------------------------------------------------------------

    if ( bOurs && pRequest->m_eClient == CDS_ClientWMP && 
         pRequest->m_nClientVersion < 12.0)
    {
        // GetBoolSetting()?
        if ( gCoreContext->GetSetting("UPnP/WMPSource") != "1")
        {
            pRequest->m_sObjectId = "RecTv/0";
            // -=>TODO: Not sure why this was added
            pRequest->m_sParentId = '8';        
        }
        else
            bOurs = false;
    }

    return bOurs;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSTv::AddItem( const UPnpCDSRequest    *pRequest, 
                         const QString           &sObjectId,
                         UPnpCDSExtensionResults *pResults,
                         bool                     bAddRef,
                         MSqlQuery               &query )
{
    int            nChanid      = query.value( 0).toInt();
    QDateTime      dtStartTime  = MythDate::as_utc(query.value(1).toDateTime());
    QDateTime      dtEndTime    = MythDate::as_utc(query.value(2).toDateTime());
    QString        sTitle       = query.value( 3).toString();
    QString        sSubtitle    = query.value( 4).toString();
    QString        sDescription = query.value( 5).toString();
    QString        sCategory    = query.value( 6).toString();
    QString        sHostName    = query.value( 7).toString();
    QString        sRecGroup    = query.value( 8).toString();
    uint64_t       nFileSize    = query.value( 9).toULongLong();
    QString        sBaseName    = query.value(10).toString();

    QDateTime      dtProgStart  =
        MythDate::as_utc(query.value(11).toDateTime());
    QDateTime      dtProgEnd    =
        MythDate::as_utc(query.value(12).toDateTime());
    QString        sStorageGrp  = query.value(13).toString();

    QString        sInetRef     = query.value(14).toString();

    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------

    if (!m_mapBackendIp.contains( sHostName ))
        m_mapBackendIp[ sHostName ] = gCoreContext->GetBackendServerIP4(sHostName);

    if (!m_mapBackendPort.contains( sHostName ))
        m_mapBackendPort[ sHostName ] = gCoreContext->GetBackendStatusPort(sHostName);

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sName      = sTitle + ": " + (sSubtitle.isEmpty() ? sDescription.left(128) : sSubtitle);

    QString sURIBase   = QString( "http://%1:%2/Content/" )
                            .arg( m_mapBackendIp  [ sHostName ] )
                            .arg( m_mapBackendPort[ sHostName ] );

    QString sURIParams = QString( "?ChanId=%1&amp;StartTime=%2" )
                            .arg( nChanid )
                            .arg( dtStartTime.toString(Qt::ISODate));

    QString sId        = QString( "RecTv/0/item%1")
                            .arg( sURIParams );

    CDSObject *pItem   = CDSObject::CreateVideoItem( sId,
                                                     sName,
                                                     sObjectId );
    pItem->m_bRestricted  = false;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "WRITABLE";

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item%2")
                            .arg( m_sExtensionId )
                            .arg( sURIParams     );

        pItem->SetPropValue( "refID", sRefId );
    }

    pItem->SetPropValue( "genre"          , sCategory    );
    pItem->SetPropValue( "longDescription", sDescription );
    pItem->SetPropValue( "description"    , sSubtitle    );

    //pItem->SetPropValue( "producer"       , );
    //pItem->SetPropValue( "rating"         , );
    //pItem->SetPropValue( "actor"          , );
    //pItem->SetPropValue( "director"       , );
    //pItem->SetPropValue( "publisher"      , );
    //pItem->SetPropValue( "language"       , );
    //pItem->SetPropValue( "relation"       , );
    //pItem->SetPropValue( "region"         , );

    // ----------------------------------------------------------------------
    // Needed for Microsoft Media Player Compatibility
    // (Won't display correct Title without them)
    // ----------------------------------------------------------------------

    pItem->SetPropValue( "creator"       , "[Unknown Author]" );
    pItem->SetPropValue( "artist"        , "[Unknown Author]" );
    pItem->SetPropValue( "album"         , "[Unknown Series]" );
    pItem->SetPropValue( "actor"         , "[Unknown Author]" );
    pItem->SetPropValue( "date"          , dtStartTime.toString(Qt::ISODate));

    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File contents/extension (HTTP)
    // ----------------------------------------------------------------------

    StorageGroup sg(sStorageGrp, sHostName);
    QString sFilePath = sg.FindFile(sBaseName);
    QString sMimeType;

    if ( QFile::exists(sFilePath) )
        sMimeType = HTTPRequest::TestMimeType( sFilePath );
    else
        sMimeType = HTTPRequest::TestMimeType( sBaseName );


    // If we are dealing with Window Media Player 12 (i.e. Windows 7)
    // then fake the Mime type to place the recorded TV in the
    // recorded TV section.
    if (pRequest->m_eClient == CDS_ClientWMP &&
        pRequest->m_nClientVersion >= 12.0)
    {
        sMimeType = "video/x-ms-dvr";
    }

    // If we are dealing with a Sony Blu-ray player then we fake the
    // MIME type to force the video to appear
    if ( pRequest->m_eClient == CDS_ClientSonyDB )
    {
        sMimeType = "video/avi";
    }


    // DLNA string below is temp fix for ps3 seeking.
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01500000000000000000000000000000" ).arg( sMimeType  );
    QString sURI      = QString( "%1GetRecording%2").arg( sURIBase   )
                                                    .arg( sURIParams );

    // Sony BDPS370 requires a DLNA Profile Name
    // FIXME: detection to determine the correct DLNA Profile Name
    if (sMimeType == "video/mpeg")
    {
        sProtocol += ";DLNA.ORG_PN=MPEG_TS_SD_NA_ISO";
    }

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

    uint uiStart = dtProgStart.toTime_t();
    uint uiEnd   = dtProgEnd.toTime_t();
    uint uiDur   = uiEnd - uiStart;
    
    MSqlQuery query2(MSqlQuery::InitCon());
    query2.prepare( "SELECT data FROM recordedmarkup WHERE chanid=:CHANID AND "
                    "starttime=:STARTTIME AND type = 33" );
    query2.bindValue(":CHANID", (int)nChanid);
    query2.bindValue(":STARTTIME", dtProgStart);
    if (query2.exec() && query2.next())
        uiDur = query2.value(0).toUInt() / 1000;

    QString sDur;

    sDur.sprintf("%02d:%02d:%02d",
                  (uiDur / 3600) % 24,
                  (uiDur / 60) % 60,
                   uiDur % 60);

    LOG(VB_UPNP, LOG_DEBUG, "Duration: " + sDur );

    pRes->AddAttribute( "duration"  , sDur      );
    pRes->AddAttribute( "size"      , QString::number( nFileSize) );

/*
    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File extension (mythtv)
    // ----------------------------------------------------------------------

    sProtocol = QString( "myth:*:%1:*"     ).arg( sMimeType  );
    sURI      = QString( "myth://%1/%2" )
                   .arg( m_mapBackendIp  [ sHostName ] )
                   .arg( sBaseName );

    pRes = pItem->AddResource( sProtocol, sURI );

    pRes->AddAttribute( "duration"  , sDur      );
    pRes->AddAttribute( "size"      , QString::number( nFileSize) );
*/

    // ----------------------------------------------------------------------
    // Add Preview URI as <res>
    // MUST be _TN and 160px
    // ----------------------------------------------------------------------

    sURI = QString( "%1GetPreviewImage%2%3").arg( sURIBase   )
                                            .arg( sURIParams )
                                            .arg( "&amp;Width=160" );

    // TODO: Must be JPG for minimal compliance
    sProtocol = QString( "http-get:*:image/png:DLNA.ORG_PN=PNG_TN");
    pItem->AddResource( sProtocol, sURI );

    // ----------------------------------------------------------------------
    // Add Artwork URI as albumArt
    // ----------------------------------------------------------------------

    sURI = QString( "%1GetRecordingArtwork?Type=coverart&amp;Inetref=%3")
                                              .arg( sURIBase   )
                                              .arg( sInetRef   );

    QList<Property*> propList = pItem->GetProperties("albumArtURI");
    if (propList.size() >= 4)
    {
        // Prefer JPEG over PNG here, although PNG is allowed JPEG probably
        // has wider device support and crucially the filesizes are smaller
        // which speeds up loading times over the network

        // We MUST include the thumbnail size, but since some clients may use the
        // first image they see and the thumbnail is tiny, instead return the
        // medium first. The large could be very large, which is no good if the
        // client is pulling images for an entire list at once!

        // Medium
        Property *pProp = propList.at(0);
        if (pProp)
        {
            // Must be no more than 1024x768
            pProp->m_sValue = sURI;
            pProp->m_sValue.append("&amp;Width=1024&amp;Height=768");
            pProp->AddAttribute("dlna:profileID", "JPG_MED");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        // Thumbnail
        pProp = propList.at(1);
        if (pProp)
        {
            // At least one albumArtURI must be a ThumbNail (TN) no larger
            // than 160x160, and it must also be a jpeg
            pProp->m_sValue = sURI;
            pProp->m_sValue.append("&amp;Width=160&amp;Height=160");
            pProp->AddAttribute("dlna:profileID", "JPG_TN");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        // Medium
        pProp = propList.at(2);
        if (pProp)
        {
            // Must be no more than 1024x768
            pProp->m_sValue = sURI;
            pProp->m_sValue.append("&amp;Width=1024&amp;Height=768");
            pProp->AddAttribute("dlna:profileID", "JPG_MED");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        // Large
        pProp = propList.at(3);
        if (pProp)
        {
            // Must be no more than 4096x4096 - for our purposes, just return
            // a fullsize image
            pProp->m_sValue = sURI;
            pProp->AddAttribute("dlna:profileID", "JPG_LRG");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }
    }


}

// vim:ts=4:sw=4:ai:et:si:sts=4
