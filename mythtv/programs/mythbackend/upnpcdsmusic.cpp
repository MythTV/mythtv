//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsmusic.cpp
//
// Purpose - uPnp Content Directory Extension for Music
//
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

// C++
#include <climits>

// Qt
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/storagegroup.h"
#include "libmythupnp/httprequest.h"
#include "libmythupnp/upnphelpers.h"

// MythBackend
#include "upnpcdsmusic.h"

/**
 * \brief Music Extension for UPnP ContentDirectory Service
 *
 *  Music                            Music
 *   - All Music                     Music/Track
 *     + <Track 1>                   Music/Track=1
 *     + <Track 2>                   Music/Track=2
 *     + <Track 3>                   Music/Track=3
 *
 *   - PlayLists                     // TODO
 *
 *   - By Artist                     Music/Artist
 *     - <Artist 1>                  Music/Artist=123
 *       - <Album 1>                 Music/Artist=123/Album=345
 *         + <Track 1>               Music/Artist=123/Album=345/Track=1
 *         + <Track 2>               Music/Artist=123/Album=345/Track=2
 *
 *   - By Album                      Music/Album
 *     - <Album 1>                   Music/Album=789
 *       + <Track 1>                 Music/Album=789/Track=1
 *       + <Track 2>                 Music/Album=789/Track=2
 *
 *   - By Recently Added             // TODO
 *     + <Track 1>
 *     + <Track 2>
 *
 *   - By Genre                      Music/Genre
 *     - <Genre 1>                   Music/Genre=252
 *       - By Artist                 Music/Genre=252/Artist
 *         - <Artist 1>              Music/Genre=252/Artist=123
 *           - <Album 1>             Music/Genre=252/Artist=123/Album=345
 *             + <Track 1>           Music/Genre=252/Artist=123/Album=345/Track=1
 *             + <Track 2>           Music/Genre=252/Artist=123/Album=345/Track=2
 *
 */
UPnpCDSMusic::UPnpCDSMusic()
             : UPnpCDSExtension( QObject::tr("Music"), "Music",
                                 "object.item.audioItem.musicTrack" )
{
    QString sServerIp   = gCoreContext->GetBackendServerIP();
    int sPort           = gCoreContext->GetBackendStatusPort();
    m_uriBase.setScheme("http");
    m_uriBase.setHost(sServerIp);
    m_uriBase.setPort(sPort);

    // ShortCuts
    m_shortcuts.insert(UPnPShortcutFeature::MUSIC, "Music");
    m_shortcuts.insert(UPnPShortcutFeature::MUSIC_ALL, "Music/Track");
    m_shortcuts.insert(UPnPShortcutFeature::MUSIC_ALBUMS, "Music/Album");
    m_shortcuts.insert(UPnPShortcutFeature::MUSIC_ARTISTS, "Music/Artist");
    m_shortcuts.insert(UPnPShortcutFeature::MUSIC_GENRES, "Music/Genre");
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::CreateRoot()
{
    if (m_pRoot)
        return;

    m_pRoot = CDSObject::CreateContainer(m_sExtensionId,
                                         m_sName,
                                         "0");

    QString containerId = m_sExtensionId + "/%1";

    // HACK: I'm not entirely happy with this solution, but it's at least
    // tidier than passing through half a dozen extra args to Load[Foo]
    // or having yet more methods just to load the counts
    auto *pRequest = new UPnpCDSRequest();
    pRequest->m_nRequestedCount = 0; // We don't want to load any results, we just want the TotalCount
    auto *pResult = new UPnpCDSExtensionResults();
    IDTokenMap tokens;
    // END HACK

    // -----------------------------------------------------------------------
    // All Tracks
    // -----------------------------------------------------------------------
    CDSObject* pContainer = CDSObject::CreateContainer ( containerId.arg("Track"),
                                              QObject::tr("All Tracks"),
                                              m_sExtensionId, // Parent Id
                                              nullptr );
    // HACK
    LoadTracks(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(0);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Artist
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Artist"),
                                              QObject::tr("Artist"),
                                              m_sExtensionId, // Parent Id
                                              nullptr );
    // HACK
    LoadArtists(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Album
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Album"),
                                              QObject::tr("Album"),
                                              m_sExtensionId, // Parent Id
                                              nullptr );
    // HACK
    LoadAlbums(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Genre
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Genre"),
                                              QObject::tr("Genre"),
                                              m_sExtensionId, // Parent Id
                                              nullptr );
    // HACK
    LoadGenres(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Directory
    // -----------------------------------------------------------------------
//     pContainer = CDSObject::CreateStorageSystem ( containerId.arg("Directory"),
//                                                   QObject::tr("Directory"),
//                                                   m_sExtensionId, // Parent Id
//                                                   nullptr );
//     // HACK
//     LoadDirectories(pRequest, pResult, tokens);
//     pContainer->SetChildCount(pResult->m_nTotalMatches);
//     pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
//     // END HACK
//     m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------

    // HACK
    delete pRequest;
    delete pResult;
    // END HACK
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

//     if (pRequest->m_eClient == CDS_ClientXBox &&
//         pRequest->m_sContainerID == "7")
//     {
//         pRequest->m_sObjectId = "Music";
//
//         LOG(VB_UPNP, LOG_INFO,
//             "UPnpCDSMusic::IsBrowseRequestForUs - Yes, ContainerId == 7");
//
//         return true;
//     }
//
//     if ((pRequest->m_sObjectId.isEmpty()) &&
//         (!pRequest->m_sContainerID.isEmpty()))
//         pRequest->m_sObjectId = pRequest->m_sContainerID;

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

//     if (pRequest->m_eClient == CDS_ClientXBox &&
//         pRequest->m_sContainerID == "7")
//     {
//         pRequest->m_sObjectId       = "Music/1";
//         pRequest->m_sSearchCriteria = "object.container.album.musicAlbum";
//         pRequest->m_sSearchList.append( pRequest->m_sSearchCriteria );
//
//         LOG(VB_UPNP, LOG_INFO, "UPnpCDSMusic::IsSearchRequestForUs... Yes.");
//
//         return true;
//     }
//
//     if (pRequest->m_sContainerID == "4")
//     {
//         pRequest->m_sObjectId       = "Music";
//         pRequest->m_sSearchCriteria = "object.item.audioItem.musicTrack";
//         pRequest->m_sSearchList.append( pRequest->m_sSearchCriteria );
//
//         LOG(VB_UPNP, LOG_INFO, "UPnpCDSMusic::IsSearchRequestForUs... Yes.");
//
//         return true;
//     }
//
//     if ((pRequest->m_sObjectId.isEmpty()) &&
//         (!pRequest->m_sContainerID.isEmpty()))
//         pRequest->m_sObjectId = pRequest->m_sContainerID;

    LOG(VB_UPNP, LOG_INFO,
        "UPnpCDSMusic::IsSearchRequestForUs.. Don't know, calling base class.");

    return UPnpCDSExtension::IsSearchRequestForUs( pRequest );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::LoadMetadata(const UPnpCDSRequest* pRequest,
                                 UPnpCDSExtensionResults* pResults,
                                 const IDTokenMap& tokens, const QString& currentToken)
{
    if (currentToken.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("UPnpCDSMusic::LoadMetadata: Final "
                                         "token missing from id: %1")
                                        .arg(pRequest->m_sParentId));
        return false;
    }

    // Root or Root + 1
    if (tokens[currentToken].isEmpty())
    {
        CDSObject *container = nullptr;

        if (pRequest->m_sObjectId == m_sExtensionId)
            container = GetRoot();
        else
            container = GetRoot()->GetChild(pRequest->m_sObjectId);

        if (container)
        {
            pResults->Add(container);
            pResults->m_nTotalMatches = 1;
            return true;
        }

        LOG(VB_GENERAL, LOG_ERR, QString("UPnpCDSMusic::LoadMetadata: Requested "
                                         "object cannot be found: %1")
                                           .arg(pRequest->m_sObjectId));
        return false;
    }
    if (currentToken == "genre")
    {
        // Genre is presently a top tier node, since it doesn't appear
        // below Artist/Album/etc we don't need to pass through
        // the ids for filtering
        return LoadGenres(pRequest, pResults, tokens);
    }
    if (currentToken == "artist")
    {
        return LoadArtists(pRequest, pResults, tokens);
    }
    if (currentToken == "album")
    {
        return LoadAlbums(pRequest, pResults, tokens);
    }
    if (currentToken == "track")
    {
        return LoadTracks(pRequest, pResults, tokens);
    }

    LOG(VB_GENERAL, LOG_ERR,
        QString("UPnpCDSMusic::LoadMetadata(): "
                "Unhandled metadata request for '%1'.").arg(currentToken));
    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::LoadChildren(const UPnpCDSRequest* pRequest,
                                 UPnpCDSExtensionResults* pResults,
                                 const IDTokenMap& tokens, const QString& currentToken)
{
    if (currentToken.isEmpty() || currentToken == m_sExtensionId.toLower())
    {
        // Root
        pResults->Add(GetRoot()->GetChildren());
        pResults->m_nTotalMatches = GetRoot()->GetChildCount();
        return true;
    }
    if (currentToken == "track")
    {
        return LoadTracks(pRequest, pResults, tokens);
    }
    if (currentToken == "genre")
    {
        if (tokens["genre"].toInt() > 0)
            return LoadArtists(pRequest, pResults, tokens);
        return LoadGenres(pRequest, pResults, tokens);
    }
    if (currentToken == "artist")
    {
        if (tokens["artist"].toInt() > 0)
            return LoadAlbums(pRequest, pResults, tokens);
        return LoadArtists(pRequest, pResults, tokens);
    }
    if (currentToken == "album")
    {
        if (tokens["album"].toInt() > 0)
            return LoadTracks(pRequest, pResults, tokens);
        return LoadAlbums(pRequest, pResults, tokens);
    }
    LOG(VB_GENERAL, LOG_ERR,
        QString("UPnpCDSMusic::LoadChildren(): "
                "Unhandled metadata request for '%1'.").arg(currentToken));

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::PopulateArtworkURIS(CDSObject* pItem, int nSongID)
{
    QUrl artURI = m_uriBase;
    artURI.setPath("/Content/GetAlbumArt");
    QUrlQuery artQuery;
    artQuery.addQueryItem("Id", QString::number(nSongID));
    artURI.setQuery(artQuery);

    QList<Property*> propList = pItem->GetProperties("albumArtURI");
    if (propList.size() >= 4)
    {
        // Prefer JPEG over PNG here, although PNG is allowed DLNA requires JPEG
        // and crucially the filesizes are smaller which speeds up loading times
        // over the network

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
            QUrlQuery mediumQuery(mediumURI.query());
            mediumQuery.addQueryItem("Width", "1024");
            mediumQuery.addQueryItem("Height", "768");
            mediumURI.setQuery(mediumQuery);
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
            QUrlQuery thumbQuery(thumbURI.query());
            thumbQuery.addQueryItem("Width", "160");
            thumbQuery.addQueryItem("Height", "160");
            thumbURI.setQuery(thumbQuery);
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
            QUrlQuery smallQuery(smallURI.query());
            smallQuery.addQueryItem("Width", "640");
            smallQuery.addQueryItem("Height", "480");
            smallURI.setQuery(smallQuery);
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
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unable to designate album artwork "
                                         "for '%1' with class '%2' and id '%3'")
                                            .arg(pItem->m_sId,
                                                 pItem->m_sClass,
                                                 QString::number(nSongID)));
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::LoadAlbums(const UPnpCDSRequest *pRequest,
                              UPnpCDSExtensionResults *pResults,
                              const IDTokenMap& tokens)
{
    QString sRequestId = pRequest->m_sObjectId;

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "a.album_id, a.album_name, t.artist_name, a.year, "
                  "a.compilation, s.song_id, g.genre, "
                  "COUNT(a.album_id), w.albumart_id "
                  "FROM music_albums a "
                  "LEFT JOIN music_artists t ON a.artist_id=t.artist_id "
                  "LEFT JOIN music_songs s ON a.album_id=s.album_id "
                  "LEFT JOIN music_genres g ON s.genre_id=g.genre_id "
                  "LEFT JOIN music_albumart w ON s.song_id=w.song_id "
                  "%1 " // WHERE clauses
                  "GROUP BY a.album_id "
                  "ORDER BY a.album_name "
                  "LIMIT :OFFSET,:COUNT";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        int nAlbumID = query.value(0).toInt();
        QString sAlbumName = query.value(1).toString();
        QString sArtist = query.value(2).toString();
        QString sYear = query.value(3).toString();
        bool bCompilation = query.value(4).toBool();
        int nSongId = query.value(5).toInt(); // TODO: Allow artwork lookups by album ID
        QString sGenre = query.value(6).toString();
        int nTrackCount = query.value(7).toInt();
        int nAlbumArtID = query.value(8).toInt();

        CDSObject* pContainer = CDSObject::CreateMusicAlbum( CreateIDString(sRequestId, "Album", nAlbumID),
                                                             sAlbumName,
                                                             pRequest->m_sParentId,
                                                             nullptr );
        pContainer->SetPropValue("artist", sArtist);
        pContainer->SetPropValue("date", sYear);
        pContainer->SetPropValue("genre", sGenre);
        pContainer->SetChildCount(nTrackCount);
        pContainer->SetChildContainerCount(0);
        if (bCompilation)
        {
            // Do nothing for now
        }

        // Artwork

        if (nAlbumArtID > 0)
            PopulateArtworkURIS(pContainer, nSongId);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::LoadArtists(const UPnpCDSRequest *pRequest,
                               UPnpCDSExtensionResults *pResults,
                               const IDTokenMap& tokens)
{
    QString sRequestId = pRequest->m_sObjectId;

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an accurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "t.artist_id, t.artist_name, CONCAT_WS(',', g.genre), "
                  "COUNT(DISTINCT a.album_id) "
                  "FROM music_artists t "
                  "LEFT JOIN music_albums a ON a.artist_id = t.artist_id "
                  "JOIN music_songs s ON t.artist_id = s.artist_id "
                  "LEFT JOIN music_genres g ON s.genre_id = g.genre_id "
                  "%1 " // WHERE clauses
                  "GROUP BY t.artist_id "
                  "ORDER BY t.artist_name "
                  "LIMIT :OFFSET,:COUNT";


    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        int nArtistId = query.value(0).toInt();
        QString sArtistName = query.value(1).toString();
//      QStringList sGenres = query.value(2).toString().split(',');
        int nAlbumCount = query.value(3).toInt();

        CDSObject* pContainer = CDSObject::CreateMusicArtist( CreateIDString(sRequestId, "Artist", nArtistId),
                                                              sArtistName,
                                                              pRequest->m_sParentId,
                                                              nullptr );
// TODO: Add SetPropValues option for multi-value properties
//         QStringList::Iterator it;
//         for (it = sGenres.begin(); it != sGenres.end(); ++it)
//         {
//             pContainer->SetPropValues("genre", sGenres);
//         }
        pContainer->SetChildCount(nAlbumCount);
        pContainer->SetChildContainerCount(nAlbumCount);

        // Artwork
        //PopulateArtistURIS(pContainer, nArtistId);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::LoadGenres(const UPnpCDSRequest *pRequest,
                              UPnpCDSExtensionResults *pResults,
                              const IDTokenMap& tokens )
{
    QString sRequestId = pRequest->m_sObjectId;

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS g.genre_id, g.genre, "
                  "COUNT( DISTINCT t.artist_id ) "
                  "FROM music_genres g "
                  "LEFT JOIN music_songs s ON g.genre_id = s.genre_id "
                  "LEFT JOIN music_artists t ON t.artist_id = s.artist_id "
                  "%1 " // WHERE clauses
                  "GROUP BY g.genre_id "
                  "ORDER BY g.genre "
                  "LIMIT :OFFSET,:COUNT";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        int nGenreId = query.value(0).toInt();
        QString sGenreName = query.value(1).toString();
        int nArtistCount = query.value(2).toInt();

        CDSObject* pContainer = CDSObject::CreateMusicGenre( CreateIDString(sRequestId, "Genre", nGenreId),
                                                             sGenreName,
                                                             pRequest->m_sParentId,
                                                             nullptr );
        pContainer->SetPropValue("description", sGenreName);

        pContainer->SetChildCount(nArtistCount);
        pContainer->SetChildContainerCount(nArtistCount);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::LoadTracks(const UPnpCDSRequest *pRequest,
                              UPnpCDSExtensionResults *pResults,
                              const IDTokenMap& tokens)
{
    QString sRequestId = pRequest->m_sObjectId;

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS s.song_id, t.artist_name, "
                  "a.album_name, s.name, "
                  "g.genre, s.year, s.track, "
                  "s.description, s.filename, s.length, s.size, "
                  "s.numplays, s.lastplay, w.albumart_id "
                  "FROM music_songs s "
                  "LEFT JOIN music_artists t ON t.artist_id = s.artist_id "
                  "LEFT JOIN music_albums a ON a.album_id = s.album_id "
                  "LEFT JOIN music_genres g ON  g.genre_id = s.genre_id "
                  "LEFT JOIN music_albumart w ON s.song_id = w.song_id "
                  "%1 " // WHERE clauses
                  "GROUP BY s.song_id "
                  "ORDER BY t.artist_name, a.album_name, s.track "
                  "LIMIT :OFFSET,:COUNT";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        int            nId          = query.value( 0).toInt();
        QString        sArtist      = query.value( 1).toString();
        QString        sAlbum       = query.value( 2).toString();
        QString        sTitle       = query.value( 3).toString();
        QString        sGenre       = query.value( 4).toString();
        int            nYear        = query.value( 5).toInt();
        int            nTrackNum    = query.value( 6).toInt();
        QString        sDescription = query.value( 7).toString();
        QString        sFileName    = query.value( 8).toString();
        auto           nLengthMS    = std::chrono::milliseconds(query.value( 9).toUInt());
        uint64_t       nFileSize    = query.value(10).toULongLong();

        int            nPlaybackCount = query.value(11).toInt();
        QDateTime      lastPlayedTime = query.value(12).toDateTime();
        int            nAlbumArtID    = query.value(13).toInt();

        CDSObject* pItem = CDSObject::CreateMusicTrack( CreateIDString(sRequestId, "Track", nId),
                                                        sTitle,
                                                        pRequest->m_sParentId,
                                                        nullptr );

        // Only add the reference ID for items which are not in the
        // 'All Tracks' container
        QString sRefIDBase = QString("%1/Track").arg(m_sExtensionId);
        if ( pRequest->m_sParentId != sRefIDBase )
        {
            QString sRefId = QString( "%1=%2")
                                .arg( sRefIDBase )
                                .arg( nId );

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
        pItem->SetPropValue( "lastPlaybackTime"     , UPnPDateTime::DateTimeFormat(lastPlayedTime));

        // Artwork
        if (nAlbumArtID > 0)
            PopulateArtworkURIS(pItem, nId);

        // ----------------------------------------------------------------------
        // Add Music Resource Element based on File extension (HTTP)
        // ----------------------------------------------------------------------

        QFileInfo fInfo( sFileName );

        QUrl    resURI    = m_uriBase;
        QUrlQuery resQuery;
        resURI.setPath("/Content/GetMusic");
        resQuery.addQueryItem("Id", QString::number(nId));
        resURI.setQuery(resQuery);

        QString sMimeType = HTTPRequest::GetMimeType( fInfo.suffix() );

        QString sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                                     sMimeType);

        Resource *pResource = pItem->AddResource( sProtocol, resURI.toEncoded() );

        pResource->AddAttribute( "duration" , UPnPDateTime::resDurationFormat(nLengthMS) );
        if (nFileSize > 0)
            pResource->AddAttribute( "size"      , QString::number( nFileSize) );

        pResults->Add(pItem);
        pItem->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSMusic::BuildWhereClause( QStringList clauses,
                                     IDTokenMap tokens)
{
    if (tokens["track"].toInt() > 0)
        clauses.append("s.song_id=:TRACK_ID");
    if (tokens["album"].toInt() > 0)
        clauses.append("s.album_id=:ALBUM_ID");
    if (tokens["artist"].toInt() > 0)
        clauses.append("s.artist_id=:ARTIST_ID");
    if (tokens["genre"].toInt() > 0)
        clauses.append("s.genre_id=:GENRE_ID");
    if (tokens["year"].toInt() > 0)
        clauses.append("s.year=:YEAR");
    if (tokens["directory"].toInt() > 0)
        clauses.append("s.directory_id=:DIRECTORY_ID");

//     if (tokens["album"].toInt() > 0)
//         clauses.append("a.album_id=:ALBUM_ID");
//     if (tokens["artist"].toInt() > 0)
//         clauses.append("a.artist_id=:ARTIST_ID");

    QString whereString;
    if (!clauses.isEmpty())
    {
        whereString = " WHERE ";
        whereString.append(clauses.join(" AND "));
    }

    return whereString;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::BindValues( MSqlQuery &query,
                            IDTokenMap tokens)
{
    if (tokens["track"].toInt() > 0)
        query.bindValue(":TRACK_ID", tokens["track"]);
    if (tokens["album"].toInt() > 0)
        query.bindValue(":ALBUM_ID", tokens["album"]);
    if (tokens["artist"].toInt() > 0)
        query.bindValue(":ARTIST_ID", tokens["artist"]);
    if (tokens["genre"].toInt() > 0)
        query.bindValue(":GENRE_ID", tokens["genre"]);
    if (tokens["year"].toInt() > 0)
        query.bindValue(":YEAR", tokens["year"]);
    if (tokens["directory"].toInt() > 0)
        query.bindValue(":DIRECTORY_ID", tokens["directory"]);
}

// vim:ts=4:sw=4:ai:et:si:sts=4
