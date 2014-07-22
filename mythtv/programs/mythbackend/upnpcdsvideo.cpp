// Program Name: upnpcdsvideo.cpp
//
// Purpose - UPnP Content Directory Extension for MythVideo Videos
//
//////////////////////////////////////////////////////////////////////////////

// POSIX headers
#include <limits.h>

// Qt headers
#include <QFileInfo>

// MythTV headers
#include "upnpcdsvideo.h"
#include "httprequest.h"
#include "mythdate.h"
#include "mythcorecontext.h"
#include "storagegroup.h"

#define LOC QString("UPnpCDSVideo: ")
#define LOC_WARN QString("UPnpCDSVideo, Warning: ")
#define LOC_ERR QString("UPnpCDSVideo, Error: ")

UPnpCDSRootInfo UPnpCDSVideo::g_RootNodes[] =
{
    {   "All Videos",
        "*",
        "SELECT 0 as key, "
          "title as name, "
          "1 as children "
            "FROM videometadata "
            "%1 "
            "ORDER BY title",
        "",
        "title",
        "object.container",
        "object.item.videoItem" },

    {   "By Folder",
        "fldr.folder",
        "SELECT "
            "fldr.folder          AS id, "
            "fldr.folder          AS name, "
            "COUNT( fldr.folder ) AS children "
          "FROM ( "
            "SELECT "
                "SUBSTRING_INDEX(SUBSTRING_INDEX(filename,'/',-2),'/',1) "
                  "AS folder "
              "FROM videometadata "
          ") AS fldr "
          "%1 "
          "GROUP BY fldr.folder "
          "ORDER BY fldr.folder",
        "WHERE fldr.folder =  :KEY",
        "title",
        "object.container.storageSystem",
        "object.container.storageFolder" },

    {   "By Length (10min int)",
        "ROUND(length+4,-1)",
        "SELECT "
            "ROUND(length+4,-1)          AS id, "
            "ROUND(length+4,-1)          AS name, "
            "COUNT( ROUND(length+4,-1) ) AS children "
          "FROM videometadata "
          "%1 "
          "GROUP BY name "
          "ORDER BY ROUND(length+4,-1)",
        "WHERE ROUND(length+4,-1) = :KEY",
        "title",
        "object.container",
        "object.container" },

    {   "By User Rating (rounded)",
        "ROUND(userrating)",
        "SELECT "
            "ROUND(userrating)          AS id, "
            "ROUND(userrating)          AS name, "
            "COUNT( ROUND(userrating) ) AS children "
          "FROM videometadata "
          "%1 "
          "GROUP BY ROUND(userrating) "
          "ORDER BY ROUND(userrating)",
        "WHERE ROUND(userrating)=ROUND(:KEY)",
        "title",
        "object.container",
        "object.container" },

    {   "By Maturity Rating",
        "rating",
        "SELECT "
            "rating          AS id, "
            "rating          AS name, "
            "COUNT( rating ) AS children "
          "FROM videometadata "
          "%1 "
          "GROUP BY rating "
          "ORDER BY rating",
        "WHERE rating=:KEY",
        "title",
        "object.container",
        "object.container" },

    {   "By Genre",
        "category",
        "SELECT "
            "videometadata.category          AS id, "
            "videocategory.category          AS name, "
            "COUNT( videometadata.category ) AS children "
          "FROM videometadata "
            "LEFT JOIN videocategory ON videocategory.intid = videometadata.category "
          "%1 "
          "GROUP BY videometadata.category "
          "ORDER BY videocategory.category",
        "WHERE videometadata.category=:KEY",
        "title",
        "object.container",
        "object.container.genre.movieGenre" },

    {   "By Director",
        "director",
        "SELECT "
            "director          AS id, "
            "director          AS name, "
            "COUNT( director ) AS children "
          "FROM videometadata "
          "%1 "
          "GROUP BY director "
          "ORDER BY director",
        "WHERE director=:KEY",
        "title",
        "object.container",
        "object.container.person" },

    {   "By Studio",
        "studio",
        "SELECT "
            "studio          AS id, "
            "studio          AS name, "
            "COUNT( studio ) AS children "
          "FROM videometadata "
            "%1 "
            "GROUP BY studio "
            "ORDER BY studio",
        "WHERE studio=:KEY",
        "title",
        "object.container",
        "object.container" },

    {   "By Year",
        "year",
        "SELECT "
            "year          AS id, "
            "year          AS name, "
            "COUNT( year ) AS children "
          "FROM videometadata "
          "%1 "
          "GROUP BY year "
          "ORDER BY year",
        "WHERE year=:KEY",
        "title",
        "object.container",
        "object.container" },

    {   "By Content Type",
        "contenttype",
        "SELECT "
            "contenttype          AS id, "
            "contenttype          AS name, "
            "COUNT( contenttype ) AS children "
          "FROM videometadata "
          "%1 "
          "GROUP BY contenttype "
          "ORDER BY contenttype",
        "WHERE contenttype=:KEY",
        "title",
        "object.container",
        "object.container.album" },

    {   "By Season",
        "season",
        "SELECT "
            "season          AS id, "
            "season          AS name, "
            "COUNT( season ) AS children "
          "FROM videometadata "
          "%1 "
          "GROUP BY season "
          "ORDER BY season",
        "WHERE season=:KEY",
        "title",
        "object.container",
        "object.container" }

};

int UPnpCDSVideo::g_nRootCount = sizeof( UPnpCDSVideo::g_RootNodes ) /
                                 sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSVideo::GetRootInfo( int nIdx )
{
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[nIdx]);

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
    if(sColumn == "fldr.folder")
    {
        return "( SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(filename,'/',-2),'/',1) "
                           "AS folder from videometadata ) "
               "AS fldr";
    }
    else
        return "videometadata";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetItemListSQL( QString sColumn )
{
    return "SELECT "
           "videometadata.intid, title, subtitle, filename, director, plot, "
           "rating, year, userrating, length, "
           "season, episode, coverfile, insertdate, host, "
           "category, studio, collectionref, homepage, contenttype, "
           "round(length+4,-1) AS length10, "
           "fldr.folder AS folder , "
           "playcount "
         "FROM videometadata "
            "INNER JOIN ( "
              "SELECT "
                  "intid,SUBSTRING_INDEX(SUBSTRING_INDEX(filename,'/',-2),'/',1) "
                    "AS folder "
                "FROM videometadata "
            ") AS fldr ON fldr.intid=videometadata.intid";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int nVideoID = mapParams[ "Id" ].toInt();

    QString sSQL = QString( "%1 WHERE intid=:VIDEOID ORDER BY title DESC" )
                    .arg( GetItemListSQL( ) );

    query.prepare( sSQL );

    query.bindValue( ":VIDEOID", (int)nVideoID );
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

        LOG(VB_UPNP, LOG_INFO,
            "UPnpCDSVideo::IsBrowseRequestForUs - Yes ContainerID == 15");
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

        LOG(VB_UPNP, LOG_INFO,
            "UPnpCDSVideo::IsBrowseRequestForUs - Yes ContainerID == 13");
        return true;
    }

    LOG(VB_UPNP, LOG_INFO,
        "UPnpCDSVideo::IsBrowseRequestForUs - Not sure... Calling base class.");

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

        LOG(VB_UPNP, LOG_INFO, "UPnpCDSVideo::IsSearchRequestForUs... Yes.");

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

void UPnpCDSVideo::AddItem( const UPnpCDSRequest    *pRequest,
                            const QString           &sObjectId,
                            UPnpCDSExtensionResults *pResults,
                            bool                     bAddRef,
                            MSqlQuery               &query )
{

    int            nVidID       = query.value( 0).toInt();
    QString        sTitle       = query.value( 1).toString();
    QString        sSubtitle    = query.value( 2).toString();
    QString        sFilePath    = query.value( 3).toString();
    QString        sDirector    = query.value( 4).toString();
    QString        sPlot        = query.value( 5).toString();
    // QString        sRating      = query.value( 6).toString();
    // int             nYear        = query.value( 7).toInt();
    // int             nUserRating  = query.value( 8).toInt();
    int            nLength      = query.value( 9).toInt();
    int            nSeason      = query.value(10).toInt();
    int            nEpisode     = query.value(11).toInt();
    QString        sCoverArt    = query.value(12).toString();
    QDateTime      dtInsertDate =
        MythDate::as_utc(query.value(13).toDateTime());
    QString        sHostName    = query.value(14).toString();
//    int            nCategory    = query.value(15).toInt();
//    QString        sStudio      = query.value(16).toString();
//    QString        sHomePage    = query.value(17).toString();
//    QString        sContentType = query.value(18).toString();

    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------

    // If the host-name is empty then we assume it is our local host
    // otherwise, we look up the host's IP address and port.  When the
    // client then trys to play the video it will be directed to the
    // host which actually has the content.
    if (!m_mapBackendIp.contains( sHostName ))
    {
        if (sHostName.isEmpty())
        {
            m_mapBackendIp[sHostName] =
                gCoreContext->GetBackendServerIP4();
        }
        else
        {
            m_mapBackendIp[sHostName] =
                gCoreContext->GetBackendServerIP4(sHostName);
        }
    }

    if (!m_mapBackendPort.contains( sHostName ))
    {
        if (sHostName.isEmpty())
        {
            m_mapBackendPort[sHostName] =
                gCoreContext->GetBackendStatusPort();
        }
        else
        {
            m_mapBackendPort[sHostName] =
                gCoreContext->GetBackendStatusPort(sHostName);
        }
    }


    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sName      = sTitle;
    if( !sSubtitle.isEmpty() )
    {
        sName += " - " + sSubtitle;
    }

    QUrl URIBase;
    URIBase.setScheme("http");
    URIBase.setHost(m_mapBackendIp[sHostName]);
    URIBase.setPort(m_mapBackendPort[sHostName]);

    QString sId        = QString( "Videos/0/item?Id=%1").arg( nVidID );

    QString sParentID = "Videos/0";

    CDSObject *pItem = CDSObject::CreateVideoItem( sId, sName, sParentID );

    pItem->m_bRestricted  = false;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "WRITABLE";

    pItem->SetPropValue( "longDescription", sPlot );
    // ?? pItem->SetPropValue( "description"    , sPlot );
    pItem->SetPropValue( "director"       , sDirector );

    if (nEpisode > 0 || nSeason > 0) // There has got to be a better way
    {
        pItem->SetPropValue( "seriesTitle"  , sTitle);
        pItem->SetPropValue( "programTitle"  , sSubtitle);
        pItem->SetPropValue( "episodeNumber"  , QString::number(nEpisode));
        //pItem->SetPropValue( "episodeCount"  , nEpisodeCount);
    }

    // TODO Implement the following
    // pItem->SetPropValue( "genre"       , sCategory );

    // HACK: Windows Media Centre Compat
    pItem->SetPropValue( "genre"          , "[Unknown Genre]"     );
    pItem->SetPropValue( "actor"          , "[Unknown Author]"    );
    pItem->SetPropValue( "creator"        , "[Unknown Creator]"   );
    pItem->SetPropValue( "album"          , "[Unknown Album]"     );
    ////

    //pItem->SetPropValue( "producer"       , );
    //pItem->SetPropValue( "rating"         , );
    //pItem->SetPropValue( "actor"          , );
    //pItem->SetPropValue( "publisher"      , );
    //pItem->SetPropValue( "language"       , );
    //pItem->SetPropValue( "relation"       , );
    //pItem->SetPropValue( "region"         , );

    QUrl artURI = URIBase;
    artURI.setPath("Content/GetVideoArtwork");
    artURI.addQueryItem("Id", QString::number(nVidID));

    QList<Property*> propList = pItem->GetProperties("albumArtURI");
    if ((!sCoverArt.isEmpty()) && (sCoverArt != "No Cover")
                               && propList.size() >= 4)
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
            // Must be no more than 1024x768
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

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item?Id=%2").arg( m_sExtensionId )
                                                    .arg( nVidID         );

        pItem->SetPropValue( "refID", sRefId );
    }

    QString sFullFileName = sFilePath;
    if (!QFile::exists( sFullFileName ))
    {
        StorageGroup sgroup("Videos");
        sFullFileName = sgroup.FindFile( sFullFileName );
    }
    QFileInfo fInfo( sFullFileName );

    pItem->SetPropValue( "date", dtInsertDate.toString("yyyy-MM-dd"));
    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.suffix() );

    // If we are dealing with a Sony Blu-ray player then we fake the
    // MIME type to force the video to appear
    if ( pRequest->m_eClient == CDS_ClientSonyDB )
    {
        sMimeType = "video/avi";
    }

    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;"
                                 "DLNA.ORG_FLAGS=0150000000000000000000000000"
                                 "0000" ).arg( sMimeType  );

    QUrl    resURI    = URIBase;
    resURI.setPath("Content/GetVideo");
    resURI.addQueryItem("Id", QString::number(nVidID));

    Resource *pRes = pItem->AddResource( sProtocol, resURI.toEncoded() );
    LOG(VB_GENERAL, LOG_NOTICE, resURI.toEncoded());
    pRes->AddAttribute( "size"      , QString("%1").arg(fInfo.size()) );

    QString sDur;
    // H:M:S[.MS] (We don't store the number of seconds for videos)
    sDur.sprintf("%02d:%02d:00.000", (nLength / 60), nLength % 60 );

    pRes->AddAttribute( "duration"  , sDur      );
}

CDSObject* UPnpCDSVideo::CreateContainer(const QString& sId, const QString& sTitle, const QString& sParentId, const QString& sClass)
{
    CDSObject *pContainer = NULL;
    if (sClass == "object.container.person.movieGenre")
    {
        pContainer = CDSObject::CreateMovieGenre(sId, sTitle, m_sExtensionId );
    }
    else
        pContainer = UPnpCDSExtension::CreateContainer(sId, sTitle, sParentId, sClass);

    return pContainer;
}
