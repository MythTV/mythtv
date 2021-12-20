#include "v2video.h"
#include "mythversion.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "videometadata.h"
#include "metadatafactory.h"
#include "bluraymetadata.h"
#include "storagegroup.h"
#include "globals.h"
#include "programinfo.h"
#include "mythmiscutil.h"
#include "v2serviceUtil.h"
#include "v2artworkInfoList.h"
#include "v2castMemberList.h"
#include "v2genreList.h"
#include "v2artworkInfoList.h"
#include "mythdb.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythavutil.h"
// #include <vector>


// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (VIDEO_HANDLE, V2Video::staticMetaObject, &V2Video::RegisterCustomTypes))

void V2Video::RegisterCustomTypes()
{
    qRegisterMetaType<V2VideoMetadataInfo*>("V2VideoMetadataInfo");
    qRegisterMetaType<V2VideoMetadataInfoList*>("V2VideoMetadataInfoList");
    qRegisterMetaType<V2VideoLookupList*>("V2VideoLookupList");
    qRegisterMetaType<V2BlurayInfo*>("V2BlurayInfo");
    qRegisterMetaType<V2VideoStreamInfoList*>("V2VideoStreamInfoList");
    qRegisterMetaType<V2VideoStreamInfo*>("V2VideoStreamInfo");
    qRegisterMetaType<V2ArtworkInfoList*>("V2ArtworkInfoList");
    qRegisterMetaType<V2ArtworkInfo*>("V2ArtworkInfo");
    qRegisterMetaType<V2CastMemberList*>("V2CastMemberList");
    qRegisterMetaType<V2CastMember*>("V2CastMember");
    qRegisterMetaType<V2GenreList*>("V2GenreList");
    qRegisterMetaType<V2Genre*>("V2Genre");
    qRegisterMetaType<V2VideoLookupList*>("V2VideoLookupList");
    qRegisterMetaType<V2VideoLookup*>("V2VideoLookup");
    qRegisterMetaType<V2ArtworkItem*>("V2ArtworkItem");
}

V2Video::V2Video()
  : MythHTTPService(s_service)
{
}

V2VideoMetadataInfo* V2Video::GetVideo( int Id )
{
    VideoMetadataListManager::VideoMetadataPtr metadata =
                          VideoMetadataListManager::loadOneFromDatabase(Id);

    if ( !metadata )
        throw( QString( "No metadata found for selected ID!." ));

    auto *pVideoMetadataInfo = new V2VideoMetadataInfo();

    V2FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );

    return pVideoMetadataInfo;
}

V2VideoMetadataInfo* V2Video::GetVideoByFileName( const QString &FileName )
{
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    QScopedPointer<VideoMetadataListManager> mlm(new VideoMetadataListManager());
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byFilename(FileName);

    if ( !metadata )
        throw( QString( "No metadata found for selected filename!." ));

    auto *pVideoMetadataInfo = new V2VideoMetadataInfo();

    V2FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );

    return pVideoMetadataInfo;
}


/////////////////////////////////////////////////////////////////////////////
// Get bookmark of a video as a frame number.
/////////////////////////////////////////////////////////////////////////////

long V2Video::GetSavedBookmark( int  Id )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filename "
                  "FROM videometadata "
                  "WHERE intid = :ID ");
    query.bindValue(":ID", Id);

    if (!query.exec())
    {
        MythDB::DBError("V2Video::GetSavedBookmark", query);
        return 0;
    }

    QString fileName;

    if (query.next())
        fileName = query.value(0).toString();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("V2Video/GetSavedBookmark Video id %1 Not found.").arg(Id));
        return -1;
    }

    ProgramInfo pi(fileName,
                         nullptr, // _plot,
                         nullptr, // _title,
                         nullptr, // const QString &_sortTitle,
                         nullptr, // const QString &_subtitle,
                         nullptr, // const QString &_sortSubtitle,
                         nullptr, // const QString &_director,
                         0, // int _season,
                         0, // int _episode,
                         nullptr, // const QString &_inetref,
                         0min, // uint _length_in_minutes,
                         0, // uint _year,
                         nullptr); //const QString &_programid);

    long ret = pi.QueryBookmark();
    return ret;
}

V2VideoMetadataInfoList* V2Video::GetVideoList( const QString &Folder,
                                                 const QString &Sort,
                                                 bool bDescending,
                                                 int nStartIndex,
                                                 int nCount       )
{
    QString fields = "title,director,studio,plot,rating,year,releasedate,"
                     "userrating,length,playcount,filename,hash,showlevel,"
                     "coverfile,inetref,collectionref,homepage,childid,browse,watched,"
                     "playcommand,category,intid,trailer,screenshot,banner,fanart,"
                     "subtitle,tagline,season,episode,host,insertdate,processed,contenttype";

    QStringList sortFields = fields.split(',');

    VideoMetadataListManager::metadata_list videolist;

    QString sql = "";
    if (!Folder.isEmpty())
        sql.append(" WHERE filename LIKE '" + Folder + "%'");
    sql.append(" ORDER BY ");
    QString sort = Sort.toLower();
    if (sort == "added")
        sql.append("insertdate");
    else if (sort == "released")
        sql.append("releasedate");
    else if (sortFields.contains(sort))
        sql.append(sort);
    else
        sql.append("intid");

    if (bDescending)
        sql += " DESC";
    VideoMetadataListManager::loadAllFromDatabase(videolist, sql);

    std::vector<VideoMetadataListManager::VideoMetadataPtr> videos(videolist.begin(), videolist.end());

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pVideoMetadataInfos = new V2VideoMetadataInfoList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)videos.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)videos.size() ) : videos.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)videos.size() );

    for( int n = nStartIndex; n < nEndIndex; n++ )
    {
        V2VideoMetadataInfo *pVideoMetadataInfo = pVideoMetadataInfos->AddNewVideoMetadataInfo();

        VideoMetadataListManager::VideoMetadataPtr metadata = videos[n];

        if (metadata)
            V2FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );
    }

    int curPage = 0;
    int totalPages = 0;
    if (nCount == 0)
        totalPages = 1;
    else
        totalPages = (int)ceil((float)videos.size() / nCount);

    if (totalPages == 1)
        curPage = 1;
    else
    {
        curPage = (int)ceil((float)nStartIndex / nCount) + 1;
    }

    pVideoMetadataInfos->setStartIndex    ( nStartIndex     );
    pVideoMetadataInfos->setCount         ( nCount          );
    pVideoMetadataInfos->setCurrentPage   ( curPage         );
    pVideoMetadataInfos->setTotalPages    ( totalPages      );
    pVideoMetadataInfos->setTotalAvailable( videos.size()   );
    pVideoMetadataInfos->setAsOf          ( MythDate::current() );
    pVideoMetadataInfos->setVersion       ( MYTH_BINARY_VERSION );
    pVideoMetadataInfos->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pVideoMetadataInfos;
}


V2VideoLookupList* V2Video::LookupVideo( const QString    &Title,
                                              const QString    &Subtitle,
                                              const QString    &Inetref,
                                              int              Season,
                                              int              Episode,
                                              const QString    &GrabberType,
                                              bool             AllowGeneric )
{
    auto *pVideoLookups = new V2VideoLookupList();

    MetadataLookupList list;

    auto *factory = new MetadataFactory(nullptr);

    if (factory)
    {
        list = factory->SynchronousLookup(Title, Subtitle,
                                         Inetref, Season, Episode,
                                         GrabberType, AllowGeneric);
    }

    if ( list.empty() )
        return pVideoLookups;

    //MetadataLookupList is a reference counted list.
    //it will delete all its content at its end of life
    for( int n = 0; n < list.size(); n++ )
    {
        V2VideoLookup *pVideoLookup = pVideoLookups->AddNewVideoLookup();

        MetadataLookup *lookup = list[n];

        if (lookup)
        {
            pVideoLookup->setTitle(lookup->GetTitle());
            pVideoLookup->setSubTitle(lookup->GetSubtitle());
            pVideoLookup->setSeason(lookup->GetSeason());
            pVideoLookup->setEpisode(lookup->GetEpisode());
            pVideoLookup->setYear(lookup->GetYear());
            pVideoLookup->setTagline(lookup->GetTagline());
            pVideoLookup->setDescription(lookup->GetDescription());
            pVideoLookup->setCertification(lookup->GetCertification());
            pVideoLookup->setInetref(lookup->GetInetref());
            pVideoLookup->setCollectionref(lookup->GetCollectionref());
            pVideoLookup->setHomePage(lookup->GetHomepage());
            pVideoLookup->setReleaseDate(
                QDateTime(lookup->GetReleaseDate(),
                          QTime(0,0),Qt::LocalTime).toUTC());
            pVideoLookup->setUserRating(lookup->GetUserRating());
            pVideoLookup->setLength(lookup->GetRuntime().count());
            pVideoLookup->setLanguage(lookup->GetLanguage());
            pVideoLookup->setCountries(lookup->GetCountries());
            pVideoLookup->setPopularity(lookup->GetPopularity());
            pVideoLookup->setBudget(lookup->GetBudget());
            pVideoLookup->setRevenue(lookup->GetRevenue());
            pVideoLookup->setIMDB(lookup->GetIMDB());
            pVideoLookup->setTMSRef(lookup->GetTMSref());

            ArtworkList coverartlist = lookup->GetArtwork(kArtworkCoverart);
            ArtworkList::iterator c;
            for (c = coverartlist.begin(); c != coverartlist.end(); ++c)
            {
                V2ArtworkItem *art = pVideoLookup->AddNewArtwork();
                art->setType("coverart");
                art->setUrl((*c).url);
                art->setThumbnail((*c).thumbnail);
                art->setWidth((*c).width);
                art->setHeight((*c).height);
            }
            ArtworkList fanartlist = lookup->GetArtwork(kArtworkFanart);
            ArtworkList::iterator f;
            for (f = fanartlist.begin(); f != fanartlist.end(); ++f)
            {
                V2ArtworkItem *art = pVideoLookup->AddNewArtwork();
                art->setType("fanart");
                art->setUrl((*f).url);
                art->setThumbnail((*f).thumbnail);
                art->setWidth((*f).width);
                art->setHeight((*f).height);
            }
            ArtworkList bannerlist = lookup->GetArtwork(kArtworkBanner);
            ArtworkList::iterator b;
            for (b = bannerlist.begin(); b != bannerlist.end(); ++b)
            {
                V2ArtworkItem *art = pVideoLookup->AddNewArtwork();
                art->setType("banner");
                art->setUrl((*b).url);
                art->setThumbnail((*b).thumbnail);
                art->setWidth((*b).width);
                art->setHeight((*b).height);
            }
            ArtworkList screenshotlist = lookup->GetArtwork(kArtworkScreenshot);
            ArtworkList::iterator s;
            for (s = screenshotlist.begin(); s != screenshotlist.end(); ++s)
            {
                V2ArtworkItem *art = pVideoLookup->AddNewArtwork();
                art->setType("screenshot");
                art->setUrl((*s).url);
                art->setThumbnail((*s).thumbnail);
                art->setWidth((*s).width);
                art->setHeight((*s).height);
            }
        }
    }

    pVideoLookups->setCount         ( list.count()                 );
    pVideoLookups->setAsOf          ( MythDate::current() );
    pVideoLookups->setVersion       ( MYTH_BINARY_VERSION          );
    pVideoLookups->setProtoVer      ( MYTH_PROTO_VERSION           );

    delete factory;

    return pVideoLookups;
}


bool V2Video::RemoveVideoFromDB( int Id )
{
    bool bResult = false;

    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    QScopedPointer<VideoMetadataListManager> mlm(new VideoMetadataListManager());
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byID(Id);

    if (metadata)
        bResult = metadata->DeleteFromDatabase();

    return bResult;
}

bool V2Video::AddVideo( const QString &sFileName,
                      const QString &sHostName )
{
    if ( sHostName.isEmpty() )
        throw( QString( "Host not provided! Local storage is deprecated and "
                        "is not supported by the API." ));

    if ( sFileName.isEmpty() ||
        (sFileName.contains("/../")) ||
        (sFileName.startsWith("../")) )
    {
        throw( QString( "Filename not provided, or fails sanity checks!" ));
    }

    StorageGroup sgroup("Videos", sHostName);

    QString fullname = sgroup.FindFile(sFileName);

    if ( !QFile::exists(fullname) )
        throw( QString( "Provided filename does not exist!" ));

    QString hash = FileHash(fullname);

    if (hash == "NULL")
    {
        LOG(VB_GENERAL, LOG_ERR, "Video Hash Failed. Unless this is a DVD or "
                                 "Blu-ray, something has probably gone wrong.");
        hash = "";
    }

    VideoMetadata newFile(sFileName, QString(), hash,
                          VIDEO_TRAILER_DEFAULT,
                          VIDEO_COVERFILE_DEFAULT,
                          VIDEO_SCREENSHOT_DEFAULT,
                          VIDEO_BANNER_DEFAULT,
                          VIDEO_FANART_DEFAULT,
                          QString(), QString(), QString(), QString(),
                          QString(), VIDEO_YEAR_DEFAULT,
                          QDate::fromString("0000-00-00","YYYY-MM-DD"),
                          VIDEO_INETREF_DEFAULT, 0, QString(),
                          VIDEO_DIRECTOR_DEFAULT, QString(), VIDEO_PLOT_DEFAULT,
                          0.0, VIDEO_RATING_DEFAULT, 0, 0,
                          0, 0,
                          MythDate::current().date(), 0,
                          ParentalLevel::plLowest);

    newFile.SetHost(sHostName);
    newFile.SaveToDatabase();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Video::UpdateVideoWatchedStatus ( int  nId,
                                       bool bWatched )
{
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    QScopedPointer<VideoMetadataListManager> mlm(new VideoMetadataListManager());
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byID(nId);

    if ( !metadata )
        return false;

    metadata->SetWatched(bWatched);
    metadata->UpdateDatabase();

    return true;
}

bool V2Video::UpdateVideoMetadata ( int           nId,
                                  const QString &sTitle,
                                  const QString &sSubTitle,
                                  const QString &sTagLine,
                                  const QString &sDirector,
                                  const QString &sStudio,
                                  const QString &sPlot,
                                  const QString &sRating,
                                  const QString &sInetref,
                                  int           nCollectionRef,
                                  const QString &sHomePage,
                                  int           nYear,
                                  QDate         sReleasedate,
                                  float         fUserRating,
                                  int           nLength,
                                  int           nPlayCount,
                                  int           nSeason,
                                  int           nEpisode,
                                  int           nShowLevel,
                                  const QString &sFileName,
                                  const QString &sHash,
                                  const QString &sCoverFile,
                                  int           nChildID,
                                  bool          bBrowse,
                                  bool          bWatched,
                                  bool          bProcessed,
                                  const QString &sPlayCommand,
                                  int           nCategory,
                                  const QString &sTrailer,
                                  const QString &sHost,
                                  const QString &sScreenshot,
                                  const QString &sBanner,
                                  const QString &sFanart,
                                  QDate         sInsertDate,
                                  const QString &sContentType,
                                  const QString &sGenres,
                                  const QString &sCast,
                                  const QString &sCountries)
{
    bool update_required = false;
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    QScopedPointer<VideoMetadataListManager> mlm(new VideoMetadataListManager());
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byID(nId);

    if (!metadata)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("UpdateVideoMetadata: Id=%1 not found")
            .arg(nId));
        return false;
    }

    if (HAS_PARAM("Title"))
    {
        metadata->SetTitle(sTitle);
        update_required = true;
    }

    if (HAS_PARAM("SubTitle"))
    {
        metadata->SetSubtitle(sSubTitle);
        update_required = true;
    }

    if (HAS_PARAM("TagLine"))
    {
        metadata->SetTagline(sTagLine);
        update_required = true;
    }

    if (HAS_PARAM("Director"))
    {
        metadata->SetDirector(sDirector);
        update_required = true;
    }

    if (HAS_PARAM("Studio"))
    {
        metadata->SetStudio(sStudio);
        update_required = true;
    }

    if (HAS_PARAM("Plot"))
    {
        metadata->SetPlot(sPlot);
        update_required = true;
    }

    if (HAS_PARAM("UserRating"))
    {
        metadata->SetUserRating(fUserRating);
        update_required = true;
    }

    if (HAS_PARAM("Inetref"))
    {
        metadata->SetInetRef(sInetref);
        update_required = true;
    }

    if (HAS_PARAM("CollectionRef"))
    {
        metadata->SetCollectionRef(nCollectionRef);
        update_required = true;
    }

    if (HAS_PARAM("HomePage"))
    {
        metadata->SetHomepage(sHomePage);
        update_required = true;
    }

    if (HAS_PARAM("Year"))
    {
        metadata->SetYear(nYear);
        update_required = true;
    }

    if (HAS_PARAM("ReleaseDate"))
    {
        metadata->SetReleaseDate(sReleasedate);
        update_required = true;
    }

    if (HAS_PARAM("Rating"))
    {
        metadata->SetRating(sRating);
        update_required = true;
    }

    if (HAS_PARAM("Length"))
    {
        metadata->SetLength(std::chrono::minutes(nLength));
        update_required = true;
    }

    if (HAS_PARAM("PlayCount"))
    {
        metadata->SetPlayCount(nPlayCount);
        update_required = true;
    }

    if (HAS_PARAM("Season"))
    {
        metadata->SetSeason(nSeason);
        update_required = true;
    }

    if (HAS_PARAM("Episode"))
    {
        metadata->SetEpisode(nEpisode);
        update_required = true;
    }

    if (HAS_PARAM("ShowLevel"))
    {
        metadata->SetShowLevel(ParentalLevel::Level(nShowLevel));
        update_required = true;
    }

    if (HAS_PARAM("FileName"))
    {
        metadata->SetFilename(sFileName);
        update_required = true;
    }

    if (HAS_PARAM("Hash"))
    {
        metadata->SetHash(sHash);
        update_required = true;
    }

    if (HAS_PARAM("CoverFile"))
    {
        metadata->SetCoverFile(sCoverFile);
        update_required = true;
    }

    if (HAS_PARAM("ChildID"))
    {
        metadata->SetChildID(nChildID);
        update_required = true;
    }

    if (HAS_PARAM("Browse"))
    {
        metadata->SetBrowse(bBrowse);
        update_required = true;
    }

    if (HAS_PARAM("Watched"))
    {
        metadata->SetWatched(bWatched);
        update_required = true;
    }

    if (HAS_PARAM("Processed"))
    {
        metadata->SetProcessed(bProcessed);
        update_required = true;
    }

    if (HAS_PARAM("PlayCommand"))
    {
        metadata->SetPlayCommand(sPlayCommand);
        update_required = true;
    }

    if (HAS_PARAM("Category"))
    {
        metadata->SetCategoryID(nCategory);
        update_required = true;
    }

    if (HAS_PARAM("Trailer"))
    {
        metadata->SetTrailer(sTrailer);
        update_required = true;
    }

    if (HAS_PARAM("Host"))
    {
        metadata->SetHost(sHost);
        update_required = true;
    }

    if (HAS_PARAM("Screenshot"))
    {
        metadata->SetScreenshot(sScreenshot);
        update_required = true;
    }

    if (HAS_PARAM("Banner"))
    {
        metadata->SetBanner(sBanner);
        update_required = true;
    }

    if (HAS_PARAM("Fanart"))
    {
        metadata->SetFanart(sFanart);
        update_required = true;
    }

    if (HAS_PARAM("InsertDate"))
    {
        metadata->SetInsertdate(sInsertDate);
        update_required = true;
    }

    if (HAS_PARAM("ContentType"))
    {
        // valid values for ContentType are 'MOVIE','TELEVISION','ADULT','MUSICVIDEO','HOMEVIDEO'
        VideoContentType contentType = kContentUnknown;
        if (sContentType == "MOVIE")
            contentType = kContentMovie;

        if (sContentType == "TELEVISION")
            contentType = kContentTelevision;

        if (sContentType == "ADULT")
            contentType = kContentAdult;

        if (sContentType == "MUSICVIDEO")
            contentType = kContentMusicVideo;

        if (sContentType == "HOMEVIDEO")
            contentType = kContentHomeMovie;

        if (contentType != kContentUnknown)
        {
            metadata->SetContentType(contentType);
            update_required = true;
        }
        else
            LOG(VB_GENERAL, LOG_ERR, QString("UpdateVideoMetadata: Ignoring unknown ContentType: %1").arg(sContentType));
    }

    if (HAS_PARAM("Genres"))
    {
        VideoMetadata::genre_list genres;
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
        QStringList genresList = sGenres.split(',', QString::SkipEmptyParts);
#else
        QStringList genresList = sGenres.split(',', Qt::SkipEmptyParts);
#endif

        std::transform(genresList.cbegin(), genresList.cend(), std::back_inserter(genres),
                       [](const QString& name)
                           {return VideoMetadata::genre_list::value_type(-1, name.simplified());} );

        metadata->SetGenres(genres);
        update_required = true;
    }

    if (HAS_PARAM("Cast"))
    {
        VideoMetadata::cast_list cast;
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
        QStringList castList = sCast.split(',', QString::SkipEmptyParts);
#else
        QStringList castList = sCast.split(',', Qt::SkipEmptyParts);
#endif

        std::transform(castList.cbegin(), castList.cend(), std::back_inserter(cast),
                       [](const QString& name)
                           {return VideoMetadata::cast_list::value_type(-1, name.simplified());} );

        metadata->SetCast(cast);
        update_required = true;
    }

    if (HAS_PARAM("Countries"))
    {
        VideoMetadata::country_list countries;
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
        QStringList countryList = sCountries.split(',', QString::SkipEmptyParts);
#else
        QStringList countryList = sCountries.split(',', Qt::SkipEmptyParts);
#endif

        std::transform(countryList.cbegin(), countryList.cend(), std::back_inserter(countries),
                       [](const QString& name)
                           {return VideoMetadata::country_list::value_type(-1, name.simplified());} );

        metadata->SetCountries(countries);
        update_required = true;
    }

    if (update_required)
        metadata->UpdateDatabase();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Set bookmark of a video as a frame number.
/////////////////////////////////////////////////////////////////////////////

bool V2Video::SetSavedBookmark( int  Id, long Offset )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filename "
                  "FROM videometadata "
                  "WHERE intid = :ID ");
    query.bindValue(":ID", Id);

    if (!query.exec())
    {
        MythDB::DBError("Video::SetSavedBookmark", query);
        return false;
    }

    QString fileName;

    if (query.next())
        fileName = query.value(0).toString();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Video/SetSavedBookmark Video id %1 Not found.").arg(Id));
        return false;
    }

    ProgramInfo pi(fileName,
                         nullptr, // _plot,
                         nullptr, // _title,
                         nullptr, // const QString &_sortTitle,
                         nullptr, // const QString &_subtitle,
                         nullptr, // const QString &_sortSubtitle,
                         nullptr, // const QString &_director,
                         0, // int _season,
                         0, // int _episode,
                         nullptr, // const QString &_inetref,
                         0min, // uint _length_in_minutes,
                         0, // uint _year,
                         nullptr); //const QString &_programid);

    pi.SaveBookmark(Offset);
    return true;
}


V2BlurayInfo* V2Video::GetBluray( const QString &sPath )
{
    QString path = sPath;

    if (sPath.isEmpty())
        path = gCoreContext->GetSetting( "BluRayMountpoint", "/media/cdrom");

    LOG(VB_GENERAL, LOG_NOTICE,
        QString("Parsing Blu-ray at path: %1 ").arg(path));

    auto *bdmeta = new BlurayMetadata(path);

    if ( !bdmeta )
        throw( QString( "Unable to open Blu-ray Metadata Parser!" ));

    if ( !bdmeta->OpenDisc() )
        throw( QString( "Unable to open Blu-ray Disc/Path!" ));

    if ( !bdmeta->ParseDisc() )
        throw( QString( "Unable to parse metadata from Blu-ray Disc/Path!" ));

    auto *pBlurayInfo = new V2BlurayInfo();

    pBlurayInfo->setPath(path);
    pBlurayInfo->setTitle(bdmeta->GetTitle());
    pBlurayInfo->setAltTitle(bdmeta->GetAlternateTitle());
    pBlurayInfo->setDiscLang(bdmeta->GetDiscLanguage());
    pBlurayInfo->setDiscNum(bdmeta->GetCurrentDiscNumber());
    pBlurayInfo->setTotalDiscNum(bdmeta->GetTotalDiscNumber());
    pBlurayInfo->setTitleCount(bdmeta->GetTitleCount());
    pBlurayInfo->setThumbCount(bdmeta->GetThumbnailCount());
    pBlurayInfo->setTopMenuSupported(bdmeta->GetTopMenuSupported());
    pBlurayInfo->setFirstPlaySupported(bdmeta->GetFirstPlaySupported());
    pBlurayInfo->setNumHDMVTitles(bdmeta->GetNumHDMVTitles());
    pBlurayInfo->setNumBDJTitles(bdmeta->GetNumBDJTitles());
    pBlurayInfo->setNumUnsupportedTitles(bdmeta->GetNumUnsupportedTitles());
    pBlurayInfo->setAACSDetected(bdmeta->GetAACSDetected());
    pBlurayInfo->setLibAACSDetected(bdmeta->GetLibAACSDetected());
    pBlurayInfo->setAACSHandled(bdmeta->GetAACSHandled());
    pBlurayInfo->setBDPlusDetected(bdmeta->GetBDPlusDetected());
    pBlurayInfo->setLibBDPlusDetected(bdmeta->GetLibBDPlusDetected());
    pBlurayInfo->setBDPlusHandled(bdmeta->GetBDPlusHandled());

    QStringList thumbs = bdmeta->GetThumbnails();
    if (!thumbs.empty())
        pBlurayInfo->setThumbPath(thumbs.at(0));

    delete bdmeta;

    return pBlurayInfo;
}


/////////////////////////////////////////////////////////////////////////////
// Jun 3, 2020
// Service to get stream info for all streams in a media file.
// This gets some basic info. If anything more is needed it can be added,
// depending on whether it is available from ffmpeg avformat apis.
// See the MythStreamInfoList class for the code that uses avformat to
// extract the information.
/////////////////////////////////////////////////////////////////////////////

V2VideoStreamInfoList* V2Video::GetStreamInfo
           ( const QString &storageGroup,
             const QString &FileName  )
{

    // Search for the filename

    StorageGroup storage( storageGroup );
    QString sFullFileName = storage.FindFile( FileName );
    MythStreamInfoList infos(sFullFileName);

    // The constructor of this class reads the file and gets the needed
    // information.
    auto *pVideoStreamInfos = new V2VideoStreamInfoList();

    pVideoStreamInfos->setCount         ( infos.m_streamInfoList.size() );
    pVideoStreamInfos->setAsOf          ( MythDate::current() );
    pVideoStreamInfos->setVersion       ( MYTH_BINARY_VERSION );
    pVideoStreamInfos->setProtoVer      ( MYTH_PROTO_VERSION  );
    pVideoStreamInfos->setErrorCode     ( infos.m_errorCode   );
    pVideoStreamInfos->setErrorMsg      ( infos.m_errorMsg    );

    for (const auto & info : qAsConst(infos.m_streamInfoList))
    {
        V2VideoStreamInfo *pVideoStreamInfo = pVideoStreamInfos->AddNewVideoStreamInfo();
        pVideoStreamInfo->setCodecType       ( QString(QChar(info.m_codecType)) );
        pVideoStreamInfo->setCodecName       ( info.m_codecName   );
        pVideoStreamInfo->setWidth           ( info.m_width 			   );
        pVideoStreamInfo->setHeight          ( info.m_height 			   );
        pVideoStreamInfo->setAspectRatio     ( info.m_SampleAspectRatio    );
        pVideoStreamInfo->setFieldOrder      ( info.m_fieldOrder           );
        pVideoStreamInfo->setFrameRate       ( info.m_frameRate            );
        pVideoStreamInfo->setAvgFrameRate    ( info.m_avgFrameRate 		   );
        pVideoStreamInfo->setChannels        ( info.m_channels   );
        pVideoStreamInfo->setDuration        ( info.m_duration   );

    }
    return pVideoStreamInfos;
}
