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
#include "upnphelpers.h"

#define LOC QString("UPnpCDSVideo: ")
#define LOC_WARN QString("UPnpCDSVideo, Warning: ")
#define LOC_ERR QString("UPnpCDSVideo, Error: ")

UPnpCDSVideo::UPnpCDSVideo()
             : UPnpCDSExtension( "Videos", "Videos",
                                 "object.item.videoItem" )
{
    QString sServerIp   = gCoreContext->GetBackendServerIP4();
    int sPort           = gCoreContext->GetBackendStatusPort();
    m_URIBase.setScheme("http");
    m_URIBase.setHost(sServerIp);
    m_URIBase.setPort(sPort);

    // ShortCuts
    m_shortcuts.insert(UPnPCDSShortcuts::VIDEOS, "Videos");
    m_shortcuts.insert(UPnPCDSShortcuts::VIDEOS_ALL, "Videos/Video");
    m_shortcuts.insert(UPnPCDSShortcuts::VIDEOS_GENRES, "Videos/Genre");
}

void UPnpCDSVideo::CreateRoot()
{
    if (m_pRoot)
        return;

    m_pRoot = CDSObject::CreateContainer(m_sExtensionId,
                                         m_sName,
                                         "0");

    CDSObject* pContainer;
    QString containerId = m_sExtensionId + "/%1";

    // HACK: I'm not entirely happy with this solution, but it's at least
    // tidier than passing through half a dozen extra args to Load[Foo]
    // or having yet more methods just to load the counts
    UPnpCDSRequest *pRequest = new UPnpCDSRequest();
    pRequest->m_nRequestedCount = 0; // We don't want to load any results, we just want the TotalCount
    UPnpCDSExtensionResults *pResult = new UPnpCDSExtensionResults();
    IDTokenMap tokens;
    // END HACK

    // -----------------------------------------------------------------------
    // All Videos
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Video"),
                                              QObject::tr("All Videos"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadVideos(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(0);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // Films
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Movie"),
                                              QObject::tr("Movies"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadMovies(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(0);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // Series
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Series"),
                                              QObject::tr("Series"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadSeries(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(0);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // Other (Home videos?)
    // -----------------------------------------------------------------------
//     pContainer = CDSObject::CreateContainer ( containerId.arg("Other"),
//                                               QObject::tr("Other"),
//                                               m_sExtensionId, // Parent Id
//                                               NULL );
//     m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // Genre
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Genre"),
                                              QObject::tr("Genre"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadGenres(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(0);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Directory
    // -----------------------------------------------------------------------
//     pContainer = CDSObject::CreateStorageSystem ( containerId.arg("Directory"),
//                                                   QObject::tr("Directory"),
//                                                   m_sExtensionId, // Parent Id
//                                                   NULL );
//    m_pRoot->AddChild(pContainer);

    // HACK
    delete pRequest;
    delete pResult;
    // END HACK
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

//     if (pRequest->m_eClient == CDS_ClientXBox &&
//         pRequest->m_sContainerID == "15" &&
//         gCoreContext->GetSetting("UPnP/WMPSource") == "1")
//     {
//         pRequest->m_sObjectId = "Videos/0";
//
//         LOG(VB_UPNP, LOG_INFO,
//             "UPnpCDSVideo::IsBrowseRequestForUs - Yes ContainerID == 15");
//         return true;
//     }
//
//     if ((pRequest->m_sObjectId.isEmpty()) &&
//         (!pRequest->m_sContainerID.isEmpty()))
//         pRequest->m_sObjectId = pRequest->m_sContainerID;

    // ----------------------------------------------------------------------
    // WMP11 compatibility code
    //
    // In this mode browsing for "Videos" is forced to either Videos (us)
    // or RecordedTV (handled by upnpcdstv)
    //
    // ----------------------------------------------------------------------

//     if (pRequest->m_eClient == CDS_ClientWMP &&
//         pRequest->m_sContainerID == "13" &&
//         pRequest->m_nClientVersion < 12.0 &&
//         gCoreContext->GetSetting("UPnP/WMPSource") == "1")
//     {
//         pRequest->m_sObjectId = "Videos/0";
//
//         LOG(VB_UPNP, LOG_INFO,
//             "UPnpCDSVideo::IsBrowseRequestForUs - Yes ContainerID == 13");
//         return true;
//     }

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


//     if (pRequest->m_eClient == CDS_ClientXBox &&
//         pRequest->m_sContainerID == "15" &&
//         gCoreContext->GetSetting("UPnP/WMPSource") == "1")
//     {
//         pRequest->m_sObjectId = "Videos/0";
//
//         LOG(VB_UPNP, LOG_INFO, "UPnpCDSVideo::IsSearchRequestForUs... Yes.");
//
//         return true;
//     }
//
//     if ((pRequest->m_sObjectId.isEmpty()) &&
//         (!pRequest->m_sContainerID.isEmpty()))
//         pRequest->m_sObjectId = pRequest->m_sContainerID;

    // ----------------------------------------------------------------------

    bool bOurs = UPnpCDSExtension::IsSearchRequestForUs( pRequest );

    // ----------------------------------------------------------------------
    // WMP11 compatibility code
    // ----------------------------------------------------------------------

//     if ( bOurs && pRequest->m_eClient == CDS_ClientWMP &&
//          pRequest->m_nClientVersion < 12.0 )
//     {
//         if ( gCoreContext->GetSetting("UPnP/WMPSource") == "1")
//         {
//             pRequest->m_sObjectId = "Videos/0";
//             // -=>TODO: Not sure why this was added.
//             pRequest->m_sParentId = "8";
//         }
//         else
//             bOurs = false;
//     }

    return bOurs;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSVideo::LoadMetadata(const UPnpCDSRequest* pRequest,
                                 UPnpCDSExtensionResults* pResults,
                                 IDTokenMap tokens, QString currentToken)
{
    if (currentToken.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("UPnpCDSTV::LoadMetadata: Final "
                                         "token missing from id: %1")
                                        .arg(pRequest->m_sParentId));
        return false;
    }

    // Root + 1
    if (tokens[currentToken].isEmpty())
    {
        CDSObject *container = m_pRoot->GetChild(pRequest->m_sObjectId);

        if (container)
        {
            pResults->Add(container);
            pResults->m_nTotalMatches = 1;
        }
        else
            LOG(VB_GENERAL, LOG_ERR, QString("UPnpCDSTV::LoadMetadata: Requested "
                                             "object cannot be found: %1")
                                               .arg(pRequest->m_sObjectId));
    }
    else if (currentToken == "series")
    {
         return LoadSeries(pRequest, pResults, tokens);
    }
    else if (currentToken == "season")
    {
         return LoadSeasons(pRequest, pResults, tokens);
    }
    else if (currentToken == "genre")
    {
        return LoadGenres(pRequest, pResults, tokens);
    }
    else if (currentToken == "movie")
    {
        return LoadMovies(pRequest, pResults, tokens);
    }
    else if (currentToken == "video")
    {
        return LoadVideos(pRequest, pResults, tokens);
    }
    else
        LOG(VB_GENERAL, LOG_ERR,
            QString("UPnpCDSVideo::LoadMetadata(): "
                    "Unhandled metadata request for '%1'.").arg(currentToken));

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSVideo::LoadChildren(const UPnpCDSRequest* pRequest,
                                UPnpCDSExtensionResults* pResults,
                                IDTokenMap tokens, QString currentToken)
{
    if (currentToken.isEmpty() || currentToken == m_sExtensionId.toLower())
    {
        // Root
        pResults->Add(GetRoot()->GetChildren());
        pResults->m_nTotalMatches = GetRoot()->GetChildCount();
        return true;
    }
    else if (currentToken == "series")
    {
         if (!tokens["series"].isEmpty())
             return LoadSeasons(pRequest, pResults, tokens);
         else
             return LoadSeries(pRequest, pResults, tokens);
    }
    else if (currentToken == "season")
    {
         if (tokens["season"].toInt() > 0)
             return LoadVideos(pRequest, pResults, tokens);
         else
             return LoadSeasons(pRequest, pResults, tokens);
    }
    else if (currentToken == "genre")
    {
        if (!tokens["genre"].isEmpty())
            return LoadVideos(pRequest, pResults, tokens);
        else
            return LoadGenres(pRequest, pResults, tokens);
    }
    else if (currentToken == "movie")
    {
         return LoadMovies(pRequest, pResults, tokens);
    }
    else if (currentToken == "video")
    {
        return LoadVideos(pRequest, pResults, tokens);
    }
    else
        LOG(VB_GENERAL, LOG_ERR,
            QString("UPnpCDSVideo::LoadChildren(): "
                    "Unhandled metadata request for '%1'.").arg(currentToken));

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSVideo::LoadSeries(const UPnpCDSRequest* pRequest,
                          UPnpCDSExtensionResults* pResults,
                          IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    sId.append("/Series=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "v.title, COUNT(DISTINCT v.season), v.intid "
                  "FROM videometadata v "
                  "%1 " // whereString
                  "GROUP BY v.title "
                  "ORDER BY v.title "
                  "LIMIT :OFFSET,:COUNT ";

    QStringList clauses;
    clauses.append("contenttype='TELEVISION'");
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        QString sTitle = query.value(0).toString();
        int nSeasonCount = query.value(1).toInt();
        int nVidID = query.value(2).toInt();

        // TODO Album or plain old container?
        CDSObject* pContainer = CDSObject::CreateAlbum( sId.arg(sTitle),
                                                        sTitle,
                                                        pRequest->m_sParentId,
                                                        NULL );
        pContainer->SetPropValue("description", QObject::tr("%n Seasons", "", nSeasonCount));
        pContainer->SetPropValue("longdescription", QObject::tr("%n Seasons", "", nSeasonCount));
        pContainer->SetPropValue("storageMedium", "HDD");

        pContainer->SetChildCount(nSeasonCount);
        pContainer->SetChildContainerCount(nSeasonCount);

        PopulateArtworkURIS(pContainer, nVidID, m_URIBase);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() >= 0)
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

bool UPnpCDSVideo::LoadSeasons(const UPnpCDSRequest* pRequest,
                               UPnpCDSExtensionResults* pResults,
                               IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    sId.append("/Season=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "v.season, COUNT(DISTINCT v.intid), v.intid "
                  "FROM videometadata v "
                  "%1 " // whereString
                  "GROUP BY v.season "
                  "ORDER BY v.season "
                  "LIMIT :OFFSET,:COUNT ";

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
        int nSeason = query.value(0).toInt();
        int nVideoCount = query.value(1).toInt();
        int nVidID = query.value(2).toInt();

        QString sTitle = QObject::tr("Season %1").arg(nSeason);

        // TODO Album or plain old container?
        CDSObject* pContainer = CDSObject::CreateAlbum( sId.arg(nSeason),
                                                        sTitle,
                                                        pRequest->m_sParentId,
                                                        NULL );
        pContainer->SetPropValue("description", QObject::tr("%n Episode(s)", "", nVideoCount));
        pContainer->SetPropValue("longdescription", QObject::tr("%n Episode(s)", "", nVideoCount));
        pContainer->SetPropValue("storageMedium", "HDD");

        pContainer->SetChildCount(nVideoCount);
        pContainer->SetChildContainerCount(0);

        PopulateArtworkURIS(pContainer, nVidID, m_URIBase);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() >= 0)
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

bool UPnpCDSVideo::LoadMovies(const UPnpCDSRequest* pRequest,
                              UPnpCDSExtensionResults* pResults,
                              IDTokenMap tokens)
{
    tokens["type"] = "MOVIE";
    //LoadGenres(pRequest, pResults, tokens);
    return LoadVideos(pRequest, pResults, tokens);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSVideo::LoadGenres(const UPnpCDSRequest* pRequest,
                              UPnpCDSExtensionResults* pResults,
                              IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    sId.append("/Genre=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "v.category, g.genre, COUNT(DISTINCT v.intid) "
                  "FROM videometadata v "
                  "LEFT JOIN videogenre g ON g.intid=v.category "
                  "%1 " // whereString
                  "GROUP BY g.intid "
                  "ORDER BY g.genre "
                  "LIMIT :OFFSET,:COUNT ";

    QStringList clauses;
    clauses.append("v.category != 0");
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        int nGenreID = query.value(0).toInt();
        QString sName = query.value(1).toString();
        int nVideoCount = query.value(2).toInt();

        // TODO Album or plain old container?
        CDSObject* pContainer = CDSObject::CreateMovieGenre( sId.arg(nGenreID),
                                                             sName,
                                                        pRequest->m_sParentId,
                                                        NULL );

        pContainer->SetChildCount(nVideoCount);
        pContainer->SetChildContainerCount(0);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() >= 0)
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

bool UPnpCDSVideo::LoadVideos(const UPnpCDSRequest* pRequest,
                              UPnpCDSExtensionResults* pResults,
                              IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
     if (GetCurrentToken(pRequest->m_sParentId).first != "video")
         sId.append("/Video");
    sId.append("=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "v.intid, title, subtitle, filename, director, plot, "
                  "rating, year, userrating, length, "
                  "season, episode, coverfile, insertdate, host, "
                  "g.genre, studio, collectionref, contenttype "
                  "FROM videometadata v "
                  "LEFT JOIN videogenre g ON g.intid=v.category "
                  "%1 " //
                  "ORDER BY title, season, episode "
                  "LIMIT :OFFSET,:COUNT ";

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

        int            nVidID       = query.value( 0).toInt();
        QString        sTitle       = query.value( 1).toString();
        QString        sSubtitle    = query.value( 2).toString();
        QString        sFilePath    = query.value( 3).toString();
        QString        sDirector    = query.value( 4).toString();
        QString        sPlot        = query.value( 5).toString();
        // QString        sRating      = query.value( 6).toString();
        int            nYear        = query.value( 7).toInt();
        // int             nUserRating  = query.value( 8).toInt();

        uint32_t       nLength      = query.value( 9).toUInt();
        // Convert from minutes to milliseconds
        nLength = (nLength * 60 *1000);

        int            nSeason      = query.value(10).toInt();
        int            nEpisode     = query.value(11).toInt();
        QString        sCoverArt    = query.value(12).toString();
        QDateTime      dtInsertDate =
            MythDate::as_utc(query.value(13).toDateTime());
        QString        sHostName    = query.value(14).toString();
        QString        sGenre       = query.value(15).toString();
    //    QString        sStudio      = query.value(16).toString();
    //    QString        sCollectionRef    = query.value(17).toString();
        QString        sContentType = query.value(18).toString();

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

        CDSObject *pItem;
        if (sContentType == "MOVIE")
        {
            pItem = CDSObject::CreateMovie( sId.arg(nVidID),
                                            sTitle,
                                            pRequest->m_sParentId );
        }
        else
        {
            pItem = CDSObject::CreateVideoItem( sId.arg(nVidID),
                                                sName,
                                                pRequest->m_sParentId );
        }

        if (!sSubtitle.isEmpty())
            pItem->SetPropValue( "description", sSubtitle );
        else
            pItem->SetPropValue( "description", sPlot.left(128).append(" ..."));
        pItem->SetPropValue( "longDescription", sPlot );
        pItem->SetPropValue( "director"       , sDirector );

        if (nEpisode > 0 || nSeason > 0) // There has got to be a better way
        {
            pItem->SetPropValue( "seriesTitle"  , sTitle );
            pItem->SetPropValue( "programTitle"  , sSubtitle );
            pItem->SetPropValue( "episodeNumber"  , QString::number(nEpisode));
            //pItem->SetPropValue( "episodeCount"  , nEpisodeCount);
        }

        pItem->SetPropValue( "genre"       , sGenre );
        if (nYear > 1830 && nYear < 9999)
            pItem->SetPropValue( "date",  QDate(nYear,1,1).toString(Qt::ISODate));
        else
            pItem->SetPropValue( "date", UPnPDateTime::DateTimeFormat(dtInsertDate) );

        // HACK: Windows Media Centre Compat (Not a UPnP or DLNA requirement, should only be done for WMC)
//         pItem->SetPropValue( "genre"          , "[Unknown Genre]"     );
//         pItem->SetPropValue( "actor"          , "[Unknown Author]"    );
//         pItem->SetPropValue( "creator"        , "[Unknown Creator]"   );
//         pItem->SetPropValue( "album"          , "[Unknown Album]"     );
        ////

        //pItem->SetPropValue( "producer"       , );
        //pItem->SetPropValue( "rating"         , );
        //pItem->SetPropValue( "actor"          , );
        //pItem->SetPropValue( "publisher"      , );
        //pItem->SetPropValue( "language"       , );
        //pItem->SetPropValue( "relation"       , );
        //pItem->SetPropValue( "region"         , );

        // Only add the reference ID for items which are not in the
        // 'All Videos' container
        QString sRefIDBase = QString("%1/Video").arg(m_sExtensionId);
        if ( pRequest->m_sParentId != sRefIDBase )
        {
            QString sRefId = QString( "%1=%2")
                                .arg( sRefIDBase )
                                .arg( nVidID );

            pItem->SetPropValue( "refID", sRefId );
        }

        // FIXME - If the slave or storage hosting this video is offline we
        //         won't find it. We probably shouldn't list it, but better
        //         still would be storing the filesize in the database so we
        //         don't waste time re-checking it constantly
        QString sFullFileName = sFilePath;
        if (!QFile::exists( sFullFileName ))
        {
            StorageGroup sgroup("Videos");
            sFullFileName = sgroup.FindFile( sFullFileName );
        }
        QFileInfo fInfo( sFullFileName );

        // ----------------------------------------------------------------------
        // Add Video Resource Element based on File extension (HTTP)
        // ----------------------------------------------------------------------

        QString sMimeType = HTTPRequest::GetMimeType( QFileInfo(sFilePath).suffix() );

        // HACK: If we are dealing with a Sony Blu-ray player then we fake the
        // MIME type to force the video to appear
//         if ( pRequest->m_eClient == CDS_ClientSonyDB )
//         {
//             sMimeType = "video/avi";
//         }

        QUrl    resURI    = URIBase;
        resURI.setPath("Content/GetVideo");
        resURI.addQueryItem("Id", QString::number(nVidID));

        QString sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                                     sMimeType);

        Resource *pRes = pItem->AddResource( sProtocol, resURI.toEncoded() );
        pRes->AddAttribute( "size"    , QString("%1").arg(fInfo.size()) );
        pRes->AddAttribute( "duration", UPnPDateTime::resDurationFormat(nLength) );

        // ----------------------------------------------------------------------
        // Add Artwork
        // ----------------------------------------------------------------------
        if (!sCoverArt.isEmpty() && (sCoverArt != "No Cover"))
        {
            PopulateArtworkURIS(pItem, nVidID, URIBase);
        }

        pResults->Add( pItem );
        pItem->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() >= 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

void UPnpCDSVideo::PopulateArtworkURIS(CDSObject* pItem, int nVidID,
                                       const QUrl& URIBase)
{
    QUrl artURI = URIBase;
    artURI.setPath("Content/GetVideoArtwork");
    artURI.addQueryItem("Id", QString::number(nVidID));

    // Prefer JPEG over PNG here, although PNG is allowed JPEG probably
    // has wider device support and crucially the filesizes are smaller
    // which speeds up loading times over the network

    // We MUST include the thumbnail size, but since some clients may use the
    // first image they see and the thumbnail is tiny, instead return the
    // medium first. The large could be very large, which is no good if the
    // client is pulling images for an entire list at once!

    // Thumbnail
    // At least one albumArtURI must be a ThumbNail (TN) no larger
    // than 160x160, and it must also be a jpeg
    QUrl thumbURI = artURI;
    if (pItem->m_sClass == "object.item.videoItem") // Show screenshot for TV, coverart for movies
        thumbURI.addQueryItem("Type", "screenshot");
    else
        thumbURI.addQueryItem("Type", "coverart");
    thumbURI.addQueryItem("Width", "160");
    thumbURI.addQueryItem("Height", "160");

    // Small
    // Must be no more than 640x480
    QUrl smallURI = artURI;
    smallURI.addQueryItem("Type", "coverart");
    smallURI.addQueryItem("Width", "640");
    smallURI.addQueryItem("Height", "480");

    // Medium
    // Must be no more than 1024x768
    QUrl mediumURI = artURI;
    mediumURI.addQueryItem("Type", "coverart");
    mediumURI.addQueryItem("Width", "1024");
    mediumURI.addQueryItem("Height", "768");

    // Large
    // Must be no more than 4096x4096 - for our purposes, just return
    // a fullsize image
    QUrl largeURI = artURI;
    largeURI.addQueryItem("Type", "fanart");

    QList<Property*> propList = pItem->GetProperties("albumArtURI");
    if (propList.size() >= 4)
    {
        Property *pProp = propList.at(0);
        if (pProp)
        {
            pProp->SetValue(mediumURI.toEncoded());
            pProp->AddAttribute("dlna:profileID", "JPEG_MED");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        pProp = propList.at(1);
        if (pProp)
        {

            pProp->SetValue(thumbURI.toEncoded());
            pProp->AddAttribute("dlna:profileID", "JPEG_TN");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        pProp = propList.at(2);
        if (pProp)
        {
            pProp->SetValue(smallURI.toEncoded());
            pProp->AddAttribute("dlna:profileID", "JPEG_SM");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        pProp = propList.at(3);
        if (pProp)
        {
            pProp->SetValue(largeURI.toEncoded());
            pProp->AddAttribute("dlna:profileID", "JPEG_LRG");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }
    }

    if (pItem->m_sClass.startsWith("object.item.videoItem"))
    {
        QString sProtocol;

        sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                             "image/jpeg", QSize(1024, 768));
        pItem->AddResource( sProtocol, mediumURI.toEncoded());

        sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                             "image/jpeg", QSize(160, 160));
        pItem->AddResource( sProtocol, thumbURI.toEncoded());

        sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                             "image/jpeg", QSize(640, 480));
        pItem->AddResource( sProtocol, smallURI.toEncoded());

        sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                             "image/jpeg", QSize(1920, 1080)); // Not the actual res, we don't know that
        pItem->AddResource( sProtocol, largeURI.toEncoded());
    }
}

QString UPnpCDSVideo::BuildWhereClause(QStringList clauses, IDTokenMap tokens)
{
    if (tokens["video"].toInt() > 0)
        clauses.append("v.intid=:VIDEO_ID");
    if (!tokens["series"].isEmpty())
        clauses.append("v.title=:TITLE");
    if (tokens["season"].toInt() > 0)
        clauses.append("v.season=:SEASON");
    if (!tokens["type"].isEmpty())
        clauses.append("v.contenttype=:TYPE");
    if (tokens["genre"].toInt() > 0)
        clauses.append("v.category=:GENRE_ID");

    QString whereString;
    if (!clauses.isEmpty())
    {
        whereString = " WHERE ";
        whereString.append(clauses.join(" AND "));
    }

    return whereString;
}

void UPnpCDSVideo::BindValues(MSqlQuery& query, IDTokenMap tokens)
{
    if (tokens["video"].toInt() > 0)
        query.bindValue(":VIDEO_ID", tokens["video"]);
    if (!tokens["series"].isEmpty())
        query.bindValue(":TITLE", tokens["series"]);
    if (tokens["season"].toInt() > 0)
        query.bindValue(":SEASON", tokens["season"]);
    if (!tokens["type"].isEmpty())
        query.bindValue(":TYPE", tokens["type"]);
    if (tokens["genre"].toInt() > 0)
        query.bindValue(":GENRE_ID", tokens["genre"]);
}
