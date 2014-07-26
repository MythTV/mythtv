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
            "FROM recorded r "
            "%1 "
            "ORDER BY r.starttime DESC",
        "",
        "r.starttime DESC",
        "object.container",
        "object.item.videoItem" },

    {   "By Title",
        "r.title",
        "SELECT r.title as id, "
          "r.title as name, "
          "count( r.title ) as children "
            "FROM recorded r "
            "%1 "
            "GROUP BY r.title "
            "ORDER BY r.title",
        "WHERE r.title=:KEY",
        "r.title",
        "object.container",
        "object.container" },

    {   "By Genre",
        "r.category",
        "SELECT r.category as id, "
          "r.category as name, "
          "count( r.category ) as children "
            "FROM recorded r "
            "%1 "
            "GROUP BY r.category "
            "ORDER BY r.category",
        "WHERE r.category=:KEY",
        "r.category",
        "object.container",
        "object.container.genre.movieGenre" },

    {   "By Date",
        "DATE_FORMAT(r.starttime, '%Y-%m-%d')",
        "SELECT  DATE_FORMAT(r.starttime, '%Y-%m-%d') as id, "
          "DATE_FORMAT(r.starttime, '%Y-%m-%d %W') as name, "
          "count( DATE_FORMAT(r.starttime, '%Y-%m-%d %W') ) as children "
            "FROM recorded r "
            "%1 "
            "GROUP BY name "
            "ORDER BY r.starttime DESC",
        "WHERE DATE_FORMAT(r.starttime, '%Y-%m-%d') =:KEY",
        "r.starttime DESC",
        "object.container",
        "object.container"
    },

    {   "By Channel",
        "r.chanid",
        "SELECT channel.chanid as id, "
          "CONCAT(channel.channum, ' ', channel.callsign) as name, "
          "count( channum ) as children "
            "FROM channel "
                "INNER JOIN recorded r ON channel.chanid = r.chanid "
            "%1 "
            "GROUP BY name "
            "ORDER BY channel.chanid",
        "WHERE channel.chanid=:KEY",
        "",
        "object.container",
        "object.container"}, // Cannot be .channelGroup because children of channelGroup must be videoBroadcast items

    {   "By Group",
        "recgroup",
        "SELECT recgroup as id, "
          "recgroup as name, count( recgroup ) as children "
            "FROM recorded "
            "%1 "
            "GROUP BY recgroup "
            "ORDER BY recgroup",
        "WHERE recgroup=:KEY",
        "recgroup",
        "object.container",
        "object.container.album" }
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
    return "recorded r";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSTv::GetItemListSQL( QString /* sColumn */ )
{
    return "SELECT r.chanid, r.starttime, r.endtime, r.title, " \
                  "r.subtitle, r.description, r.category, "     \
                  "r.hostname, r.recgroup, r.filesize, "        \
                  "r.basename, r.progstart, r.progend, "        \
                  "r.storagegroup, r.inetref, "                 \
                  "p.category_type, c.callsign, c.channum, "    \
                  "p.episode, p.totalepisodes, p.season, "      \
                  "r.programid, r.seriesid, r.recordid, "       \
                  "c.default_authority, c.name "                \
           "FROM recorded r "                                   \
           "LEFT JOIN channel c ON r.chanid=c.chanid "          \
           "LEFT JOIN recordedprogram p ON p.chanid=r.chanid "  \
           "                          AND p.starttime=r.progstart";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSTv::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int     nChanId    = mapParams[ "ChanId"    ].toInt();
    QString sStartTime = mapParams[ "StartTime" ];

    QString sSQL = QString("%1 WHERE r.chanid=:CHANID AND r.starttime=:STARTTIME ")
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
    QString        sCatType     = query.value(15).toString();
    QString        sCallsign    = query.value(16).toString();
    QString        sChanNum     = query.value(17).toString();

    int            nEpisode      = query.value(18).toInt();
    int            nEpisodeTotal = query.value(19).toInt();
    int            nSeason       = query.value(20).toInt();

    QString        sProgramId   = query.value(21).toString();
    QString        sSeriesId    = query.value(22).toString();
    int            nRecordId    = query.value(23).toInt();

    QString        sDefaultAuthority = query.value(24).toString();
    QString        sChanName    = query.value(25).toString();

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

    QUrl URIBase;
    URIBase.setScheme("http");
    URIBase.setHost(m_mapBackendIp[sHostName]);
    URIBase.setPort(m_mapBackendPort[sHostName]);

    QString sId        = QString( "RecTv/0/item?ChanId=%1&amp;StartTime=%2")
                            .arg( nChanid )
                            .arg( dtStartTime.toString(Qt::ISODate));

    CDSObject *pItem   = CDSObject::CreateVideoItem( sId,
                                                     sName,
                                                     sObjectId );
    pItem->m_bRestricted  = false;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "WRITABLE";

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item?ChanId=%2&amp;StartTime=%3")
                            .arg( m_sExtensionId )
                            .arg( nChanid )
                            .arg( dtStartTime.toString(Qt::ISODate));

        pItem->SetPropValue( "refID", sRefId );
    }

    pItem->SetPropValue( "genre"          , sCategory    );
    pItem->SetPropValue( "longDescription", sDescription );
    pItem->SetPropValue( "description"    , sSubtitle    );

    pItem->SetPropValue( "channelName"    , sChanName   );
    pItem->SetPropValue( "channelID"      , QString::number(nChanid), "NETWORK");
    pItem->SetPropValue( "callSign"       , sCallsign    );
    pItem->SetPropValue( "channelNr"      , sChanNum     );

    if (sCatType != "movie")
    {
        pItem->SetPropValue( "seriesTitle"  , sTitle);
        pItem->SetPropValue( "programTitle"  , sSubtitle);
    }
    else
        pItem->SetPropValue( "programTitle"  , sTitle);

    if (   nEpisode > 0 || nSeason > 0) // There has got to be a better way
    {
        pItem->SetPropValue( "episodeNumber" , QString::number(nEpisode));
        pItem->SetPropValue( "episodeCount"  , QString::number(nEpisodeTotal));
    }

    pItem->SetPropValue( "recordedStartDateTime", dtStartTime.toString(Qt::ISODate)); //dtStartTime
    //pItem->SetPropValue( "recordedDuration", );
    pItem->SetPropValue( "recordedDayOfWeek"    , dtStartTime.toString("ddd"));
    pItem->SetPropValue( "srsRecordScheduleID"  , QString::number(nRecordId));

    if (!sSeriesId.isEmpty())
    {
        // FIXME: This should be set correctly for EIT data to SI_SERIESID and
        //        for known sources such as TMS to the correct identifier
        QString sIdType = "mythtv.org_XMLTV";
        if (sSeriesId.contains(sDefaultAuthority))
            sIdType = "mythtv.org_EIT";

        pItem->SetPropValue( "seriesID", sSeriesId, sIdType );
    }

    if (!sProgramId.isEmpty())
    {
        // FIXME: This should be set correctly for EIT data to SI_PROGRAMID and
        //        for known sources such as TMS to the correct identifier
        QString sIdType = "mythtv.org_XMLTV";
        if (sProgramId.contains(sDefaultAuthority))
            sIdType = "mythtv.org_EIT";

        pItem->SetPropValue( "programID", sProgramId, sIdType );
    }

    pItem->SetPropValue( "date"          , dtStartTime.toString(Qt::ISODate));


    // Bookmark support
    //pItem->SetPropValue( "lastPlaybackPosition", QString::number());


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

//     pItem->SetPropValue( "creator"       , "[Unknown Author]" );
//     pItem->SetPropValue( "artist"        , "[Unknown Author]" );
//     pItem->SetPropValue( "album"         , "[Unknown Series]" );
//     pItem->SetPropValue( "actor"         , "[Unknown Author]" );

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

    // HACK: If we are dealing with a Sony Blu-ray player then we fake the
    // MIME type to force the video to appear
    if ( pRequest->m_eClient == CDS_ClientSonyDB )
        sMimeType = "video/avi";

    // HACK: DLNA string below is temp fix for ps3 seeking.
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01500000000000000000000000000000" ).arg( sMimeType  );

    // HACK: Sony BDPS370 requires a DLNA Profile Name
    // FIXME: detection to determine the correct DLNA Profile Name
    if (sMimeType == "video/mpeg")
    {
        sProtocol += ";DLNA.ORG_PN=MPEG_TS_SD_NA_ISO";
    }

    QUrl    resURI    = URIBase;
    resURI.setPath("Content/GetRecording");
    resURI.addQueryItem("ChanId", QString::number(nChanid));
    resURI.addQueryItem("StartTime", dtStartTime.toString(Qt::ISODate));

    Resource *pRes = pItem->AddResource( sProtocol, resURI.toEncoded() );
    uint uiStart = dtProgStart.toTime_t();
    uint uiEnd   = dtProgEnd.toTime_t();
    uint uiDurMS   = (uiEnd - uiStart) * 1000;

    MSqlQuery query2(MSqlQuery::InitCon());
    query2.prepare( "SELECT data FROM recordedmarkup WHERE chanid=:CHANID AND "
                    "starttime=:STARTTIME AND type = 33" );
    query2.bindValue(":CHANID", (int)nChanid);
    query2.bindValue(":STARTTIME", dtProgStart);
    if (query2.exec() && query2.next())
        uiDurMS = query2.value(0).toUInt();

    QString sDur;
    // H:M:S[.MS] (We don't store the number of seconds for recordings)
    sDur.sprintf("%02d:%02d:%02d.%03d",
                  (uiDurMS / 3600000) % 24,
                  (uiDurMS / 60000) % 60,
                   uiDurMS % 60000,
                   uiDurMS % 1000);

    LOG(VB_UPNP, LOG_DEBUG, "Duration: " + sDur );

    pRes->AddAttribute( "duration"  , sDur      );
    pRes->AddAttribute( "size"      , QString::number( nFileSize) );

    // ----------------------------------------------------------------------
    // Add Preview URI as <res>
    // MUST be _TN and 160px
    // ----------------------------------------------------------------------

    QUrl previewURI = URIBase;
    previewURI.setPath("Content/GetPreviewImage");
    previewURI.addQueryItem("ChanId", QString::number(nChanid));
    previewURI.addQueryItem("StartTime", dtStartTime.toString(Qt::ISODate));
    previewURI.addQueryItem("Width", "160");
    previewURI.addQueryItem("Format", "JPG");
    sProtocol = QString( "http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN");
    pItem->AddResource( sProtocol, previewURI.toEncoded());

    // ----------------------------------------------------------------------
    // Add Artwork URI as albumArt
    // ----------------------------------------------------------------------

    if (!sInetRef.isEmpty())
    {
        QUrl artURI = URIBase;
        artURI.setPath("Content/GetRecordingArtwork");
        artURI.addQueryItem("Type", "coverart");
        artURI.addQueryItem("Inetref", sInetRef);

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
                QUrl mediumURI = artURI;
                mediumURI.addQueryItem("Width", "1024");
                mediumURI.addQueryItem("Height", "768");
                pProp->SetValue(mediumURI.toEncoded());
                pProp->AddAttribute("dlna:profileID", "JPEG_MED");
                pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
            }

            // Thumbnail
            pProp = propList.at(1);
            if (pProp)
            {
                // At least one albumArtURI must be a ThumbNail (TN) no larger
                // than 160x160, and it must also be a jpeg
                QUrl thumbURI = artURI;
                thumbURI.addQueryItem("Width", "160");
                thumbURI.addQueryItem("Height", "160");
                pProp->SetValue(thumbURI.toEncoded());
                pProp->AddAttribute("dlna:profileID", "JPEG_TN");
                pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
            }

            // Small
            pProp = propList.at(2);
            if (pProp)
            {
                // Must be no more than 640x480
                QUrl smallURI = artURI;
                smallURI.addQueryItem("Width", "640");
                smallURI.addQueryItem("Height", "480");
                pProp->SetValue(smallURI.toEncoded());
                pProp->AddAttribute("dlna:profileID", "JPEG_SM");
                pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
            }

            // Large
            pProp = propList.at(3);
            if (pProp)
            {
                // Must be no more than 4096x4096 - for our purposes, just return
                // a fullsize image
                pProp->SetValue(artURI.toEncoded());
                pProp->AddAttribute("dlna:profileID", "JPEG_LRG");
                pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
            }
        }
    }


}

// vim:ts=4:sw=4:ai:et:si:sts=4
