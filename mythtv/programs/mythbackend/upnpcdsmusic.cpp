//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsmusic.cpp
//
// Purpose - uPnp Content Directory Extension for Music
//
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#include <climits>

#include <QFileInfo>

#include "storagegroup.h"
#include "upnpcdsmusic.h"
#include "httprequest.h"
#include "mythcorecontext.h"

/*
   Music                            Music
    - All Music                     Music/All
      + <Track 1>                   Music/All/item?Id=1
      + <Track 2>
      + <Track 3>
    - PlayLists
    - By Artist                     Music/artist
      - <Artist 1>                  Music/artist/artistKey=Pink Floyd
        - <Album 1>                 Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall
          + <Track 1>               Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall/item?Id=1
          + <Track 2>
    - By Album
      - <Album 1>
        + <Track 1>
        + <Track 2>
    - By Recently Added
      + <Track 1>
      + <Track 2>
    - By Genre
      - By Artist                   Music/artist
        - <Artist 1>                Music/artist/artistKey=Pink Floyd
          - <Album 1>               Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall
            + <Track 1>             Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall/item?Id=1
            + <Track 2>
*/

UPnpCDSRootInfo UPnpCDSMusic::g_RootNodes[] =
{
    {   "All Music",
        "*",
        "SELECT song_id as id, "
          "name, "
          "1 as children "
            "FROM music_songs song "
            "%1 "
            "ORDER BY name",
        "",
        "name",
        "object.container",
        "object.item.audioItem.musicTrack" },

#if 0
// This is currently broken... need to handle list of items with single parent
// (like 'All Music')

    {   "Recently Added",
        "*",
        "SELECT song_id id, "
          "name, "
          "1 as children "
            "FROM music_songs song "
            "%1 "
            "ORDER BY name",
        "WHERE (DATEDIFF( CURDATE(), date_modified ) <= 30 ) ", "" },
#endif

    {   "By Album",
        "song.album_id",
        "SELECT a.album_id as id, "
          "a.album_name as name, "
          "count( song.album_id ) as children "
            "FROM music_songs song join music_albums a on a.album_id = song.album_id "
            "%1 "
            "GROUP BY a.album_name "
            "ORDER BY a.album_name",
        "WHERE song.album_id=:KEY",
        "song.track",
        "object.container",
        "object.container.album.musicAlbum" },

    {   "By Artist",
        "song.artist_id",
        "SELECT a.artist_id as id, "
          "a.artist_name as name, "
          "count( distinct song.artist_id ) as children "
            "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
            "%1 "
            "GROUP BY a.artist_id "
            "ORDER BY a.artist_name",
        "WHERE song.artist_id=:KEY",
        "",
        "object.container",
        "object.container.person.musicArtist" },

    {   "By Genre",
        "song.genre_id",
        "SELECT g.genre_id as id, "
          "genre as name, "
          "count( distinct song.genre_id ) as children "
            "FROM music_songs song join music_genres g on g.genre_id = song.genre_id "
            "%1 "
            "GROUP BY g.genre_id "
            "ORDER BY g.genre",
        "WHERE song.genre_id=:KEY",
        "",
        "object.container",
        "object.container.genre.musicGenre" },

};

int UPnpCDSMusic::g_nRootCount = sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

UPnpCDSMusic::UPnpCDSMusic()
             : UPnpCDSExtension( "Music", "Music",
                                 "object.item.audioItem.musicTrack" )
{
    QString sServerIp   = gCoreContext->GetBackendServerIP4();
    int sPort           = gCoreContext->GetBackendStatusPort();
    m_URIBase.setScheme("http");
    m_URIBase.setHost(sServerIp);
    m_URIBase.setPort(sPort);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSMusic::GetRootInfo( int nIdx )
{
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]);

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSMusic::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSMusic::GetTableName( QString sColumn )
{
    return "music_songs song";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSMusic::GetItemListSQL( QString /* sColumn */ )
{
    return "SELECT song.song_id as intid, artist.artist_name as artist, "     \
           "album.album_name as album, song.name as title, "                  \
           "genre.genre, song.year, song.track as tracknum, "                 \
           "song.description, song.filename, song.length, song.size, "         \
           "song.numplays, song.lastplay "                                    \
           "FROM music_songs song "                                           \
           " join music_artists artist on artist.artist_id = song.artist_id " \
           " join music_albums album on album.album_id = song.album_id "      \
           " join music_genres genre on  genre.genre_id = song.genre_id ";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int     nId    = mapParams[ "Id" ].toInt();

    QString sSQL = QString( "%1 WHERE song.song_id=:ID " )
                      .arg( GetItemListSQL() );

    query.prepare( sSQL );

    query.bindValue( ":ID", (int)nId );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::IsBrowseRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // Xbox360 compatibility code.

    if (pRequest->m_eClient == CDS_ClientXBox &&
        pRequest->m_sContainerID == "7")
    {
        pRequest->m_sObjectId = "Music";

        LOG(VB_UPNP, LOG_INFO,
            "UPnpCDSMusic::IsBrowseRequestForUs - Yes, ContainerId == 7");

        return true;
    }

    if ((pRequest->m_sObjectId.isEmpty()) &&
        (!pRequest->m_sContainerID.isEmpty()))
        pRequest->m_sObjectId = pRequest->m_sContainerID;

    LOG(VB_UPNP, LOG_INFO,
        "UPnpCDSMusic::IsBrowseRequestForUs - Not sure... Calling base class.");

    return UPnpCDSExtension::IsBrowseRequestForUs( pRequest );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::IsSearchRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // XBox 360 compatibility code

    if (pRequest->m_eClient == CDS_ClientXBox &&
        pRequest->m_sContainerID == "7")
    {
        pRequest->m_sObjectId       = "Music/1";
        pRequest->m_sSearchCriteria = "object.container.album.musicAlbum";
        pRequest->m_sSearchList.append( pRequest->m_sSearchCriteria );

        LOG(VB_UPNP, LOG_INFO, "UPnpCDSMusic::IsSearchRequestForUs... Yes.");

        return true;
    }

    if (pRequest->m_sContainerID == "4")
    {
        pRequest->m_sObjectId       = "Music";
        pRequest->m_sSearchCriteria = "object.item.audioItem.musicTrack";
        pRequest->m_sSearchList.append( pRequest->m_sSearchCriteria );

        LOG(VB_UPNP, LOG_INFO, "UPnpCDSMusic::IsSearchRequestForUs... Yes.");

        return true;
    }

    if ((pRequest->m_sObjectId.isEmpty()) &&
        (!pRequest->m_sContainerID.isEmpty()))
        pRequest->m_sObjectId = pRequest->m_sContainerID;

    LOG(VB_UPNP, LOG_INFO,
        "UPnpCDSMusic::IsSearchRequestForUs.. Don't know, calling base class.");

    return UPnpCDSExtension::IsSearchRequestForUs( pRequest );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::AddItem( const UPnpCDSRequest    *pRequest,
                            const QString           &sObjectId,
                            UPnpCDSExtensionResults *pResults,
                            bool                     bAddRef,
                            MSqlQuery               &query )
{
    QString        sName;

    int            nId          = query.value( 0).toInt();
    QString        sArtist      = query.value( 1).toString();
    QString        sAlbum       = query.value( 2).toString();
    QString        sTitle       = query.value( 3).toString();
    QString        sGenre       = query.value( 4).toString();
    int            nYear        = query.value( 5).toInt();
    int            nTrackNum    = query.value( 6).toInt();
    QString        sDescription = query.value( 7).toString();
    QString        sFileName    = query.value( 8).toString();
    uint           nLengthMS    = query.value( 9).toInt();
    uint64_t       nFileSize    = (quint64)query.value(10).toULongLong();

    int            nPlaybackCount = query.value(11).toInt();
    QDateTime      lastPlayedTime = query.value(12).toDateTime();

#if 0
    if ((nNodeIdx == 0) || (nNodeIdx == 1))
    {
        sName = QString( "%1-%2:%3" )
                   .arg( sArtist)
                   .arg( sAlbum )
                   .arg( sTitle );
    }
    else
#endif
        sName = sTitle;


    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------

#if 0
    if (!m_mapBackendIp.contains( sHostName ))
        m_mapBackendIp[ sHostName ] = gCoreContext->GetSettingOnHost( "BackendServerIp", sHostName);

    if (!m_mapBackendPort.contains( sHostName ))
        m_mapBackendPort[ sHostName ] = gCoreContext->GetSettingOnHost("BackendStatusPort", sHostName);
#endif

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sId        = QString( "Music/1/item?Id=%1")
                            .arg( nId );

    CDSObject *pItem   = CDSObject::CreateMusicTrack( sId,
                                                      sName,
                                                      sObjectId );
    pItem->m_bRestricted  = true;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "NOT_WRITABLE";

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item?Id=%2").arg( m_sExtensionId )
                                                    .arg( nId            );

        pItem->SetPropValue( "refID", sRefId );
    }

    pItem->SetPropValue( "genre"                , sGenre      );
    pItem->SetPropValue( "description"          , sTitle      );
    pItem->SetPropValue( "longDescription"      , sDescription);

    pItem->SetPropValue( "artist"               ,  sArtist    );
    pItem->SetPropValue( "creator"              ,  sArtist    );
    pItem->SetPropValue( "album"                ,  sAlbum     );
    pItem->SetPropValue( "originalTrackNumber"  ,  QString::number(nTrackNum));
    if (nYear > 0 && nYear < 9999)
        pItem->SetPropValue( "date",  QDate(nYear,1,1).toString(Qt::ISODate));

    pItem->SetPropValue( "playbackCount"        , QString::number(nPlaybackCount));
    pItem->SetPropValue( "lastPlaybackTime"     , lastPlayedTime.toString(Qt::ISODate));

    // Artwork
    PopulateArtworkURIS(pItem, nId);

    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Music Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------

    QFileInfo fInfo( sFileName );

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.suffix() );

    QString sAdditionalInfo = "*";
    if (sMimeType == "audio/mpeg")
    {
        sAdditionalInfo = "DLNA.ORG_PN=MP3X";
    }
    else if (sMimeType == "audio/x-ms-wma")
    {
        sAdditionalInfo = "DLNA.ORG_PN=WMAFULL";
    }
    else if (sMimeType == "audio/vnd.dolby.dd-raw")
    {
        sAdditionalInfo = "DLNA.ORG_PN=AC3";
    }
    else if (sMimeType == "audio/mp4")
    {
        sAdditionalInfo = "DLNA.ORG_PN=AAC_ISO_320";
    }

    QString sProtocol = QString( "http-get:*:%1:%2" ).arg( sMimeType )
                                                      .arg( sAdditionalInfo );
    QUrl    resURI    = m_URIBase;
    resURI.setPath("Content/GetMusic");
    resURI.addQueryItem("Id", QString::number(nId));

    Resource *pRes = pItem->AddResource( sProtocol, resURI.toEncoded() );
    int nLengthSecs = nLengthMS / 1000;

    QString sDur;
    // H:M:S[.MS]
    sDur.sprintf("%02d:%02d:%02d.%03d",
                  (nLengthSecs / 3600) % 24,
                  (nLengthSecs / 60) % 60,
                  nLengthSecs % 60,
                  nLengthMS % 1000);

    pRes->AddAttribute( "duration"  , sDur      );
    if (nFileSize > 0)
        pRes->AddAttribute( "size"      , QString::number( nFileSize) );
}

CDSObject* UPnpCDSMusic::CreateContainer(const QString& sId,
                                         const QString& sTitle,
                                         const QString& sParentId,
                                         const QString& sClass )
{
    CDSObject *pContainer = NULL;
    if (sClass == "object.container.person.musicArtist")
    {
        pContainer = CDSObject::CreateMusicArtist( sId, sTitle, m_sExtensionId );
    }
    else if (sClass == "object.container.album.musicAlbum")
    {
        pContainer = CDSObject::CreateMusicAlbum( sId, sTitle, m_sExtensionId );

        PopulateAlbumContainer( pContainer, sId );
    }
    else if (sClass == "object.container.genre.musicGenre")
    {
        pContainer = CDSObject::CreateMusicGenre( sId, sTitle, m_sExtensionId );
    }
    else
        pContainer = UPnpCDSExtension::CreateContainer(sId, sTitle, sParentId, sClass);

    return pContainer;
}

void UPnpCDSMusic::PopulateAlbumContainer(CDSObject* pContainer,
                                          const QString& sId)
{

    int nAlbumId = sId.section('=',1).toInt();
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT a.album_name, t.artist_name, a.year, a.compilation, "
                  "s.song_id, g.genre "
                  "FROM music_albums a "
                  "LEFT JOIN music_artists t ON a.artist_id=t.artist_id "
                  "LEFT JOIN music_songs s ON a.album_id=s.album_id "
                  "LEFT JOIN music_genres g ON s.genre_id=g.genre_id "
                  "WHERE a.album_id=:ALBUM_ID LIMIT 1");
    query.bindValue(":ALBUM_ID", nAlbumId);
    if (query.exec() && query.next())
    {
        QString sAlbumName = query.value(0).toString();
        QString sArtist = query.value(1).toString();
        QString sYear = query.value(2).toString();
        bool bCompilation = query.value(3).toBool();
        int nSongId = query.value(4).toInt(); // TODO: Allow artwork lookups by album ID
        QString sGenre = query.value(5).toString();

        pContainer->SetPropValue("artist", sArtist);
        pContainer->SetPropValue("date", sYear);
        pContainer->SetPropValue("genre", sGenre);

        // Artwork
        PopulateArtworkURIS(pContainer, nSongId);
    }
}

void UPnpCDSMusic::PopulateArtworkURIS(CDSObject* pItem, int nAlbumID)
{
    QUrl artURI = m_URIBase;
    artURI.setPath("Content/GetAlbumArt");
    artURI.addQueryItem("Id", QString::number(nAlbumID));

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

// vim:ts=4:sw=4:ai:et:si:sts=4
